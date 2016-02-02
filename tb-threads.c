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
  free(th);

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
  *thread = malloc(sizeof(struct tbthread));
  memset(*thread, 0, sizeof(struct tbthread));
  (*thread)->self = *thread;
  (*thread)->stack = stack;
  (*thread)->stack_size = attr->stack_size;
  (*thread)->fn = f;
  (*thread)->arg = arg;

  //----------------------------------------------------------------------------
  // Spawn the thread
  //----------------------------------------------------------------------------
  int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SYSVSEM | CLONE_SIGHAND;
  flags |= CLONE_THREAD | CLONE_SETTLS;
  int tid = tbclone(start_thread, *thread, flags, stack+attr->stack_size,
                    0, 0, *thread);
  if(tid < 0) {
    tbmunmap(stack, attr->stack_size);
    free(*thread);
    return tid;
  }

  return 0;
}
