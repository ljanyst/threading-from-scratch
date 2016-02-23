//------------------------------------------------------------------------------
// Copyright (c) 2016 by Lukasz Janyst <lukasz@jany.st>
//------------------------------------------------------------------------------
// This file is part of thread-bites.
//
// thread-bites is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// thread-bites is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with thread-bites.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include "tb.h"
#include "tb-private.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <linux/sched.h>
#include <linux/mman.h>
#include <asm-generic/mman-common.h>
#include <asm-generic/param.h>
#include <linux/futex.h>
#include <asm/prctl.h>

//------------------------------------------------------------------------------
// Prototypes and globals
//------------------------------------------------------------------------------
static void release_descriptor(tbthread_t desc);
static struct tbthread *get_descriptor();
static tbthread_mutex_t desc_mutex = TBTHREAD_MUTEX_INITIALIZER;

//------------------------------------------------------------------------------
// Initialize threading
//------------------------------------------------------------------------------
static void *glibc_thread_desc;
void tbthread_init()
{
  glibc_thread_desc = tbthread_self();
  tbthread_t thread = malloc(sizeof(struct tbthread));
  memset(thread, 0, sizeof(struct tbthread));
  thread->self = thread;
  SYSCALL2(__NR_arch_prctl, ARCH_SET_FS, thread);
}

//------------------------------------------------------------------------------
// Finalize threading
//------------------------------------------------------------------------------
void tbthread_finit()
{
  free(tbthread_self());
  SYSCALL2(__NR_arch_prctl, ARCH_SET_FS, glibc_thread_desc);
}

//------------------------------------------------------------------------------
// Init the attrs to the defaults
//------------------------------------------------------------------------------
void tbthread_attr_init(tbthread_attr_t *attr)
{
  attr->stack_size = 8192 * 1024;
  attr->joinable   = 1;
}

int tbthread_attr_setdetachstate(tbthread_attr_t *attr, int state)
{
  if(state == TBTHREAD_CREATE_DETACHED)
    attr->joinable = 0;
  else
    attr->joinable = 1;
}

//------------------------------------------------------------------------------
// Thread function wrapper
//------------------------------------------------------------------------------
static int start_thread(void *arg)
{
  tbthread_t th = (tbthread_t)arg;
  tbthread_exit(th->fn(th->arg));
  return 0;
}

//------------------------------------------------------------------------------
// Terminate the current thread
//------------------------------------------------------------------------------
void tbthread_exit(void *retval)
{
  tbthread_t th = tbthread_self();
  uint32_t stack_size = th->stack_size;
  void *stack = th->stack;
  int free_desc = 0;

  th->retval = retval;
  tb_tls_call_destructors();

  tbthread_mutex_lock(&desc_mutex);
  if(th->join_status == TB_DETACHED)
    free_desc = 1;
  th->join_status = TB_JOINABLE_FIXED;
  tbthread_mutex_unlock(&desc_mutex);

  if(free_desc)
    release_descriptor(th);

  //----------------------------------------------------------------------------
  // Free the stack and exit. We do it this way because we remove the stack from
  // underneath our feet and cannot allow the C code to write on it anymore.
  //----------------------------------------------------------------------------
  register long a1 asm("rdi") = (long)stack;
  register long a2 asm("rsi") = stack_size;
  asm volatile(
    "syscall\n\t"
    "movq $60, %%rax\n\t" // 60 = __NR_exit
    "movq $0, %%rdi\n\t"
    "syscall"
    :
    : "a" (__NR_munmap), "r" (a1), "r" (a2)
    : "memory", "cc", "r11", "cx");
}

//------------------------------------------------------------------------------
// Wait for exit
//------------------------------------------------------------------------------
static void wait_for_thread(tbthread_t thread)
{
  uint32_t tid = thread->exit_futex;
  long ret = 0;
  if(tid != 0)
    do {
      ret = SYSCALL3(__NR_futex, &thread->exit_futex, FUTEX_WAIT, tid);
    } while(ret != -EWOULDBLOCK && ret != 0);
}

//------------------------------------------------------------------------------
// Descriptor lists
//------------------------------------------------------------------------------
static list_t used_desc;
static list_t free_desc;

//------------------------------------------------------------------------------
// Get a descriptor
//------------------------------------------------------------------------------
static struct tbthread *get_descriptor()
{
  tbthread_t  desc = 0;
  list_t     *node = 0;

  //----------------------------------------------------------------------------
  // Try to re-use a thread descriptor and make sure that the corresponding
  // thread has actually exited
  //----------------------------------------------------------------------------
  tbthread_mutex_lock(&desc_mutex);
  node = free_desc.next;
  if(node)
    list_rm(node);
  tbthread_mutex_unlock(&desc_mutex);

  if(node) {
    desc = (tbthread_t)node->element;
    wait_for_thread(desc);
  }

  //----------------------------------------------------------------------------
  // We don't have any free descriptors so we allocate and add to the list of
  // used descriptors
  //----------------------------------------------------------------------------
  if(!node) {
    desc = malloc(sizeof(struct tbthread));
    node = malloc(sizeof(list_t));
    node->element = desc;
  }

  tbthread_mutex_lock(&desc_mutex);

  list_add(&used_desc, node);
  tbthread_mutex_unlock(&desc_mutex);
  return desc;
}

//------------------------------------------------------------------------------
// Release a descriptor
//------------------------------------------------------------------------------
static void release_descriptor(tbthread_t desc)
{
  tbthread_mutex_lock(&desc_mutex);
  list_t *node = list_find_elem(&used_desc, desc);
  if(!node) {
    tbprint("Releasing unknown descriptor: 0x%llx! Scared and confused!\n",
            desc);
    // abort!
  }
  list_rm(node);
  list_add(&free_desc, node);
  tbthread_mutex_unlock(&desc_mutex);
}

//------------------------------------------------------------------------------
// Spawn a thread
//------------------------------------------------------------------------------
int tbthread_create(
  tbthread_t            *thread,
  const tbthread_attr_t *attr,
  void                  *(*f)(void *),
  void                  *arg)
{
  //----------------------------------------------------------------------------
  // Allocate the stack with a guard page at the end so that we could protect
  // from overflows (by receiving a SIGSEGV)
  //----------------------------------------------------------------------------
  void *stack = tbmmap(NULL, attr->stack_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  long status = (long)stack;
  if(status < 0)
    return status;

  status = SYSCALL3(__NR_mprotect, stack, EXEC_PAGESIZE, PROT_NONE);
  if(status < 0) {
    tbmunmap(stack, attr->stack_size);
    return status;
  }

  //----------------------------------------------------------------------------
  // Pack everything up
  //----------------------------------------------------------------------------
  *thread = get_descriptor();
  memset(*thread, 0, sizeof(struct tbthread));
  (*thread)->self = *thread;
  (*thread)->stack = stack;
  (*thread)->stack_size = attr->stack_size;
  (*thread)->fn = f;
  (*thread)->arg = arg;
  (*thread)->join_status = attr->joinable;

  //----------------------------------------------------------------------------
  // Spawn the thread
  //----------------------------------------------------------------------------
  int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SYSVSEM | CLONE_SIGHAND;
  flags |= CLONE_THREAD | CLONE_SETTLS;
  flags |= CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID;

  int tid = tbclone(start_thread, *thread, flags, stack+attr->stack_size,
                    0, &(*thread)->exit_futex, *thread);
  if(tid < 0) {
    tbmunmap(stack, attr->stack_size);
    free(*thread);
    return tid;
  }

  //----------------------------------------------------------------------------
  // It may happen that we will start to wait for this futex before the kernel
  // manages to fill it with something meaningful.
  //----------------------------------------------------------------------------
  (*thread)->exit_futex = tid;

  return 0;
}

//------------------------------------------------------------------------------
// Detach a thread
//------------------------------------------------------------------------------
int tbthread_detach(tbthread_t thread)
{
  int ret = 0;

  tbthread_mutex_lock(&desc_mutex);
  if(!list_find_elem(&used_desc, thread)) {
    ret = -ESRCH;
    goto exit;
  }

  if(thread->join_status == TB_JOINABLE_FIXED) {
    ret = -EINVAL;
    goto exit;
  }

  thread->join_status = TB_DETACHED;
exit:
  tbthread_mutex_unlock(&desc_mutex);
  return ret;
}

//------------------------------------------------------------------------------
// Join a thread
//------------------------------------------------------------------------------
int tbthread_join(tbthread_t thread, void **retval)
{
  tbthread_t self = tbthread_self();
  int ret = 0;

  //----------------------------------------------------------------------------
  // Check if the thread may be joined
  //----------------------------------------------------------------------------
  tbthread_mutex_lock(&desc_mutex);

  if(thread == self) {
    ret = -EDEADLK;
    goto error;
  }

  if(!list_find_elem(&used_desc, thread)) {
    ret = -ESRCH;
    goto error;
  }

  if(thread->join_status == TB_DETACHED) {
    ret = -EINVAL;
    goto error;
  }

  if(self->joiner == thread) {
    ret = -EDEADLK;
    goto error;
  }

  if(thread->joiner) {
    ret = -EINVAL;
    goto error;
  }

  thread->join_status = TB_JOINABLE_FIXED;
  thread->joiner = self;

  //----------------------------------------------------------------------------
  // We can release the lock now, because we're responsible for releasing the
  // thread descriptor now, so it's not going to go away.
  //----------------------------------------------------------------------------
  tbthread_mutex_unlock(&desc_mutex);

  wait_for_thread(thread);
  if(retval)
    *retval = thread->retval;
  release_descriptor(thread);
  return 0;

error:
  tbthread_mutex_unlock(&desc_mutex);
  return ret;
}

//------------------------------------------------------------------------------
// Thread equal
//------------------------------------------------------------------------------
int tbthread_equal(tbthread_t t1, tbthread_t t2)
{
  return t1 == t2;
}

//------------------------------------------------------------------------------
// Run the code once
//------------------------------------------------------------------------------
int tbthread_once(tbthread_once_t *once, void (*func)(void))
{
  if(!once || !func)
    return -EINVAL;

  if(*once == TB_ONCE_DONE)
    return 0;

  if(__sync_bool_compare_and_swap(once, TB_ONCE_NEW, TB_ONCE_IN_PROGRESS)) {
    (*func)();
    *once = TB_ONCE_DONE;
    SYSCALL3(__NR_futex, once, FUTEX_WAKE, INT_MAX);
    return 0;
  }

  while(*once != TB_ONCE_DONE)
    SYSCALL3(__NR_futex, once, FUTEX_WAIT, TB_ONCE_IN_PROGRESS);
  return 0;
}
