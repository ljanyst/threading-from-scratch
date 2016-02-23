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

#pragma once

#include <stdint.h>
#include <asm/unistd_64.h>
#include <asm-generic/errno.h>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
#define TBTHREAD_MAX_KEYS 1024
#define TBTHREAD_MUTEX_NORMAL 0
#define TBTHREAD_MUTEX_ERRORCHECK 1
#define TBTHREAD_MUTEX_RECURSIVE 2
#define TBTHREAD_MUTEX_DEFAULT 0
#define TBTHREAD_CREATE_DETACHED 0
#define TBTHREAD_CREATE_JOINABLE 1

//------------------------------------------------------------------------------
// Thread attirbutes
//------------------------------------------------------------------------------
typedef struct
{
  uint32_t  stack_size;
  uint8_t   joinable;
} tbthread_attr_t;

//------------------------------------------------------------------------------
// Thread descriptor
//------------------------------------------------------------------------------
typedef struct tbthread
{
  struct tbthread *self;
  void *stack;
  uint32_t stack_size;
  uint32_t exit_futex;
  void *(*fn)(void *);
  void *arg;
  void *retval;
  struct
  {
    uint64_t seq;
    void *data;
  } tls[TBTHREAD_MAX_KEYS];
  uint8_t join_status;
  struct tbthread *joiner;
} *tbthread_t;

//------------------------------------------------------------------------------
// Mutex attributes
//------------------------------------------------------------------------------
typedef struct
{
  uint8_t type;
} tbthread_mutexattr_t;

//------------------------------------------------------------------------------
// Mutex
//------------------------------------------------------------------------------
typedef struct
{
  int        futex;
  uint8_t    type;
  tbthread_t owner;
  uint64_t   counter;
} tbthread_mutex_t;

#define TBTHREAD_MUTEX_INITIALIZER {0, 0, 0, 0}

//------------------------------------------------------------------------------
// Once
//------------------------------------------------------------------------------
typedef int tbthread_once_t;

#define TBTHREAD_ONCE_INIT 0

//------------------------------------------------------------------------------
// General threading
//------------------------------------------------------------------------------
void tbthread_init();
void tbthread_finit();
void tbthread_attr_init(tbthread_attr_t *attr);
int tbthread_attr_setdetachstate(tbthread_attr_t *attr, int state);
int tbthread_create(tbthread_t *thread, const tbthread_attr_t *attrs,
  void *(*f)(void *), void *arg);
void tbthread_exit(void *retval);
int tbthread_detach(tbthread_t thread);
int tbthread_join(tbthread_t thread, void **retval);
int tbthread_equal(tbthread_t t1, tbthread_t t2);
int tbthread_once(tbthread_once_t *once, void (*func)(void));

//------------------------------------------------------------------------------
// TLS
//------------------------------------------------------------------------------
typedef uint16_t tbthread_key_t;
tbthread_t tbthread_self();
int tbthread_key_create(tbthread_key_t *key, void (*destructor)(void *));
int tbthread_key_delete(tbthread_key_t key);
void *tbthread_getspecific(tbthread_key_t key);
int tbthread_setspecific(tbthread_key_t kay, void *value);

//------------------------------------------------------------------------------
// Mutexes
//------------------------------------------------------------------------------
int tbthread_mutexattr_init(tbthread_mutexattr_t *attr);
int tbthread_mutexattr_destroy(tbthread_mutexattr_t *attr);
int tbthread_mutexattr_gettype(const tbthread_mutexattr_t *attr, int *type);
int tbthread_mutexattr_settype(tbthread_mutexattr_t *attr, int type);

int tbthread_mutex_init(tbthread_mutex_t *mutex,
  const tbthread_mutexattr_t *attr);
int tbthread_mutex_destroy(tbthread_mutex_t *mutex);
int tbthread_mutex_lock(tbthread_mutex_t *mutex);
int tbthread_mutex_trylock(tbthread_mutex_t *mutex);
int tbthread_mutex_unlock(tbthread_mutex_t *mutex);

//------------------------------------------------------------------------------
// Utility functions
//------------------------------------------------------------------------------
void tbprint(const char *format, ...);
int tbwrite(int fd, const char *buffer, unsigned long len);
void tbsleep(int secs);
void *tbmmap(void *addr, unsigned long length, int prot, int flags, int fd,
  unsigned long offset);
int tbmunmap(void *addr, unsigned long length);

int tbclone(int (*fn)(void *), void *arg, int flags, void *child_stack, ...
  /* pid_t *ptid, pid_t *ctid, void *tls */ );

void *tbbrk(void *addr);

uint64_t tbtime();
uint32_t tbrandom(uint32_t *seed);

//------------------------------------------------------------------------------
// Syscall interface
//------------------------------------------------------------------------------
#define SYSCALL(name, a1, a2, a3, a4, a5, a6)           \
  ({                                                    \
    long result;                                        \
    long __a1 = (long)(a1);                             \
    long __a2 = (long)(a2);                             \
    long __a3 = (long)(a3);                             \
    long __a4 = (long)(a4);                             \
    long __a5 = (long)(a5);                             \
    long __a6 = (long)(a6);                             \
    register long _a1 asm("rdi") = __a1;                \
    register long _a2 asm("rsi") = __a2;                \
    register long _a3 asm("rdx") = __a3;                \
    register long _a4 asm("r10") = __a4;                \
    register long _a5 asm("r8")  = __a5;                \
    register long _a6 asm("r9")  = __a6;                \
    asm volatile (                                      \
      "syscall\n\t"                                     \
      : "=a" (result)                                   \
      : "0" (name), "r" (_a1), "r" (_a2), "r" (_a3),    \
        "r" (_a4), "r" (_a5), "r" (_a6)                 \
      : "memory", "cc", "r11", "cx");                   \
    (long) result; })

#define SYSCALL1(name, a1) \
  SYSCALL(name, a1, 0, 0, 0, 0, 0)
#define SYSCALL2(name, a1, a2) \
  SYSCALL(name, a1, a2, 0, 0, 0, 0)
#define SYSCALL3(name, a1, a2, a3) \
  SYSCALL(name, a1, a2, a3, 0, 0, 0)
#define SYSCALL4(name, a1, a2, a3, a4) \
  SYSCALL(name, a1, a2, a3, a4, 0, 0)
#define SYSCALL5(name, a1, a2, a3, a4, a5) \
  SYSCALL(name, a1, a2, a3, a4, a5, 0)
#define SYSCALL6(name, a1, a2, a3, a4, a5, a6) \
  SYSCALL(name, a1, a2, a3, a4, a5, a6)

//------------------------------------------------------------------------------
// List
//------------------------------------------------------------------------------
typedef struct list {
  struct list *next;
  struct list *prev;
  void        *element;
} list_t;

int list_add_elem(list_t *list, void *element);
void list_add(list_t *list, list_t *node);
void list_rm(list_t *node);
list_t *list_find_elem(list_t *list, void *element);
void list_for_each_elem(list_t *list, void (*func)(void *));
void list_clear(list_t *list);
