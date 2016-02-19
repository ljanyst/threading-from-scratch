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
// Prototypes
//------------------------------------------------------------------------------
static void release_descriptor(tbthread_t desc);
static struct tbthread *get_descriptor();

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
}

//------------------------------------------------------------------------------
// Thread function wrapper
//------------------------------------------------------------------------------
static int start_thread(void *arg)
{
  tbthread_t th = (tbthread_t)arg;
  uint32_t stack_size = th->stack_size;
  void *stack = th->stack;
  th->fn(th->arg);
  tb_tls_call_destructors();
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
  return 0;
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
static tbthread_mutex_t desc_mutex = TBTHREAD_MUTEX_INITIALIZER;

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
  (*thread)->exit_futex = 0;
  (*thread)->fn = f;
  (*thread)->arg = arg;

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

  return 0;
}
