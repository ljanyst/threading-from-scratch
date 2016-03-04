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

#include <linux/futex.h>
#include <string.h>

//------------------------------------------------------------------------------
// Lock function prototypes
//------------------------------------------------------------------------------
static int lock_normal(tbthread_mutex_t *mutex);
static int trylock_normal(tbthread_mutex_t *mutex);
static int unlock_normal(tbthread_mutex_t *mutex);

static int lock_errorcheck(tbthread_mutex_t *mutex);
static int trylock_errorcheck(tbthread_mutex_t *mutex);
static int unlock_errorcheck(tbthread_mutex_t *mutex);

static int lock_recursive(tbthread_mutex_t *mutex);
static int trylock_recursive(tbthread_mutex_t *mutex);
static int unlock_recursive(tbthread_mutex_t *mutex);

static int lock_prio_none(tbthread_mutex_t *mutex);
static int trylock_prio_none(tbthread_mutex_t *mutex);
static int unlock_prio_none(tbthread_mutex_t *mutex);

static int lock_prio_inherit(tbthread_mutex_t *mutex);
static int trylock_prio_inherit(tbthread_mutex_t *mutex);
static int unlock_prio_inherit(tbthread_mutex_t *mutex);

static int lock_prio_protect(tbthread_mutex_t *mutex);
static int trylock_prio_protect(tbthread_mutex_t *mutex);
static int unlock_prio_protect(tbthread_mutex_t *mutex);

//------------------------------------------------------------------------------
// Mutex function tables
//------------------------------------------------------------------------------
static int (*lockers[])(tbthread_mutex_t *) = {
  lock_normal,
  lock_errorcheck,
  lock_recursive,
  lock_prio_none,
  lock_prio_inherit,
  lock_prio_protect
};

static int (*trylockers[])(tbthread_mutex_t *) = {
  trylock_normal,
  trylock_errorcheck,
  trylock_recursive,
  trylock_prio_none,
  trylock_prio_inherit,
  trylock_prio_protect
};

static int (*unlockers[])(tbthread_mutex_t *) = {
  unlock_normal,
  unlock_errorcheck,
  unlock_recursive,
  unlock_prio_none,
  unlock_prio_inherit,
  unlock_prio_protect
};

//------------------------------------------------------------------------------
// Low level locking
//------------------------------------------------------------------------------
void tb_futex_lock(int *futex)
{
  while(1) {
    if(__sync_bool_compare_and_swap(futex, 0, 1))
      return;
    SYSCALL3(__NR_futex, futex, FUTEX_WAIT, 1);
  }
}

int tb_futex_trylock(int *futex)
{
  if(__sync_bool_compare_and_swap(futex, 0, 1))
      return 0;
  return -EBUSY;
}

void tb_futex_unlock(int *futex)
{
  if(__sync_bool_compare_and_swap(futex, 1, 0))
    SYSCALL3(__NR_futex, futex, FUTEX_WAKE, 1);
}

//------------------------------------------------------------------------------
// Normal mutex
//------------------------------------------------------------------------------
static int lock_normal(tbthread_mutex_t *mutex)
{
  return (*lockers[mutex->protocol])(mutex);
}

static int trylock_normal(tbthread_mutex_t *mutex)
{
  return (*trylockers[mutex->protocol])(mutex);
}

static int unlock_normal(tbthread_mutex_t *mutex)
{
  return (*unlockers[mutex->protocol])(mutex);
}

//------------------------------------------------------------------------------
// Errorcheck mutex
//------------------------------------------------------------------------------
static int lock_errorcheck(tbthread_mutex_t *mutex)
{
  tbthread_t self = tbthread_self();
  if(mutex->owner == self)
    return -EDEADLK;
  (*lockers[mutex->protocol])(mutex);
  return 0;
}

static int trylock_errorcheck(tbthread_mutex_t *mutex)
{
  return (*trylockers[mutex->protocol])(mutex);
}

static int unlock_errorcheck(tbthread_mutex_t *mutex)
{
  if(mutex->owner != tbthread_self() || mutex->futex == 0)
    return -EPERM;
  (*unlockers[mutex->protocol])(mutex);
  return 0;
}

//------------------------------------------------------------------------------
// Recursive mutex
//------------------------------------------------------------------------------
static int lock_recursive(tbthread_mutex_t *mutex)
{
  tbthread_t self = tbthread_self();
  if(mutex->owner != self) {
    (*lockers[mutex->protocol])(mutex);
    mutex->owner   = self;
  }
  if(mutex->counter == (uint64_t)-1)
    return -EAGAIN;
  ++mutex->counter;
  return 0;
}

static int trylock_recursive(tbthread_mutex_t *mutex)
{
  tbthread_t self = tbthread_self();
  if(mutex->owner != self && (*trylockers[mutex->protocol])(mutex))
    return -EBUSY;

  if(mutex->owner != self) {
    mutex->owner = self;
    mutex->counter = 1;
    return 0;
  }

  if(mutex->counter == (uint64_t)-1)
    return -EAGAIN;

  ++mutex->counter;
  return 0;
}

static int unlock_recursive(tbthread_mutex_t *mutex)
{
  if(mutex->owner != tbthread_self())
    return -EPERM;
  --mutex->counter;
  if(mutex->counter == 0) {
    mutex->owner = 0;
    return (*unlockers[mutex->protocol])(mutex);
  }
  return 0;
}

//------------------------------------------------------------------------------
// Priority none
//------------------------------------------------------------------------------
static int lock_prio_none(tbthread_mutex_t *mutex)
{
  tb_futex_lock(&mutex->futex);
  mutex->owner = tbthread_self();
  return 0;
}

static int trylock_prio_none(tbthread_mutex_t *mutex)
{
  int ret = tb_futex_trylock(&mutex->futex);
  if(ret == 0)
      mutex->owner = tbthread_self();
  return ret;
}

static int unlock_prio_none(tbthread_mutex_t *mutex)
{
  mutex->owner = 0;
  tb_futex_unlock(&mutex->futex);
  return 0;
}

//------------------------------------------------------------------------------
// Priority inherit
//------------------------------------------------------------------------------
static int lock_prio_inherit(tbthread_mutex_t *mutex)
{
  tbthread_t self = tbthread_self();

  while(1) {
    int locked = 0;
    tb_futex_lock(&mutex->internal_futex);
    if(mutex->futex == 0) {
      locked = 1;
      mutex->owner = self;
      mutex->futex = 1;
      tb_inherit_mutex_add(mutex);
    }
    else
      tb_inherit_mutex_sched(mutex, self);
    tb_futex_unlock(&mutex->internal_futex);
    if(locked)
      return 0;
    SYSCALL3(__NR_futex, &mutex->futex, FUTEX_WAIT, 1);
  }
}

static int trylock_prio_inherit(tbthread_mutex_t *mutex)
{
  tbthread_t self = tbthread_self();

  int locked = 0;
  tb_futex_lock(&mutex->internal_futex);
  if(mutex->futex == 0) {
    locked = 1;
    mutex->owner = self;
    mutex->futex = 1;
    tb_inherit_mutex_add(mutex);
  }
  tb_futex_unlock(&mutex->internal_futex);
  if(locked)
    return 0;
  return -EBUSY;
}

static int unlock_prio_inherit(tbthread_mutex_t *mutex)
{
  tb_futex_lock(&mutex->internal_futex);
  tb_inherit_mutex_unsched(mutex);
  mutex->owner = 0;
  mutex->futex = 0;
  SYSCALL3(__NR_futex, &mutex->futex, FUTEX_WAKE, 1);
  tb_futex_unlock(&mutex->internal_futex);
  return 0;
}

//------------------------------------------------------------------------------
// Priority protect
//------------------------------------------------------------------------------
static int lock_prio_protect(tbthread_mutex_t *mutex)
{
  lock_prio_none(mutex);
  tb_protect_mutex_sched(mutex);
  return 0;
}

static int trylock_prio_protect(tbthread_mutex_t *mutex)
{
  int ret = trylock_prio_none(mutex);
  if(ret == 0)
    tb_protect_mutex_sched(mutex);
  return ret;
}

static int unlock_prio_protect(tbthread_mutex_t *mutex)
{
  tbthread_t self = tbthread_self();
  tb_protect_mutex_unsched(mutex);
  unlock_prio_none(mutex);
  return 0;
}

//------------------------------------------------------------------------------
// Init attributes
//------------------------------------------------------------------------------
int tbthread_mutexattr_init(tbthread_mutexattr_t *attr)
{
  memset(attr, 0, sizeof(tbthread_mutexattr_t));
  attr->type = TBTHREAD_MUTEX_DEFAULT;
  attr->protocol = TBTHREAD_PRIO_NONE;
  return 0;
}

//------------------------------------------------------------------------------
// Destroy attributes - no op
//------------------------------------------------------------------------------
int tbthread_mutexattr_destroy(tbthread_mutexattr_t *attr)
{
  return 0;
}

//------------------------------------------------------------------------------
// Set type
//------------------------------------------------------------------------------
int tbthread_mutexattr_gettype(const tbthread_mutexattr_t *attr, int *type)
{
  *type = attr->type;
  return 0;
}

//------------------------------------------------------------------------------
// Get type
//------------------------------------------------------------------------------
int tbthread_mutexattr_settype(tbthread_mutexattr_t *attr, int type)
{
  if(type < TBTHREAD_MUTEX_NORMAL || type > TBTHREAD_MUTEX_RECURSIVE)
    return -EINVAL;
  attr->type = type;
}

//------------------------------------------------------------------------------
// Initialize the mutex
//------------------------------------------------------------------------------
int tbthread_mutex_init(tbthread_mutex_t *mutex,
  const tbthread_mutexattr_t *attr)
{
  memset(mutex, 0, sizeof(tbthread_mutex_t));
  uint8_t type = TBTHREAD_MUTEX_DEFAULT;
  uint8_t protocol = TBTHREAD_PRIO_NONE;
  uint16_t sched_info = 0;
  if(attr) {
    type = attr->type;
    protocol = attr->protocol;
    if(protocol == TBTHREAD_PRIO_PROTECT && attr->prioceiling != 0)
      sched_info = SCHED_INFO_PACK(SCHED_FIFO, attr->prioceiling);
  }
  mutex->type = type;
  mutex->protocol = protocol;
  mutex->sched_info = sched_info;
}

//------------------------------------------------------------------------------
// Destroy the mutex - no op
//------------------------------------------------------------------------------
int tbthread_mutex_destroy(tbthread_mutex_t *mutex)
{
  return 0;
}

//------------------------------------------------------------------------------
// Lock the mutex
//------------------------------------------------------------------------------
int tbthread_mutex_lock(tbthread_mutex_t *mutex)
{
  return (*lockers[mutex->type])(mutex);
}

//------------------------------------------------------------------------------
// Try locking the mutex
//------------------------------------------------------------------------------
int tbthread_mutex_trylock(tbthread_mutex_t *mutex)
{
  return (*trylockers[mutex->type])(mutex);
}

//------------------------------------------------------------------------------
// Unlock the mutex
//------------------------------------------------------------------------------
int tbthread_mutex_unlock(tbthread_mutex_t *mutex)
{
  return (*unlockers[mutex->type])(mutex);;
}

//------------------------------------------------------------------------------
// Set priority ceiling
//------------------------------------------------------------------------------
int tbthread_mutexattr_setprioceiling(tbthread_mutexattr_t *attr, int ceiling)
{
  if(ceiling < 0 || ceiling > 99)
    return -EINVAL;
  attr->prioceiling = ceiling;
  return 0;
}

//------------------------------------------------------------------------------
// Set protocol
//------------------------------------------------------------------------------
int tbthread_mutexattr_setprotocol(tbthread_mutexattr_t *attr, int protocol)
{
  if(protocol != TBTHREAD_PRIO_NONE && protocol != TBTHREAD_PRIO_INHERIT &&
     protocol != TBTHREAD_PRIO_PROTECT)
    return -EINVAL;
  attr->protocol = protocol;
  return 0;
}

//------------------------------------------------------------------------------
// Get priority ceiling
//------------------------------------------------------------------------------
int tbthread_mutex_getprioceiling(const tbthread_mutex_t *mutex, int *ceiling)
{
  if(!mutex)
    return -EINVAL;
  *ceiling = SCHED_INFO_PRIORITY(mutex->sched_info);
  return 0;
}

//------------------------------------------------------------------------------
// Set priority ceiling
//------------------------------------------------------------------------------
int tbthread_mutex_setprioceiling(tbthread_mutex_t *mutex, int ceiling,
  int *old_ceiling)
{
  if(!mutex)
    return -EINVAL;
  if(mutex->protocol != TBTHREAD_PRIO_PROTECT)
    return -EINVAL;
  if(ceiling < 0 || ceiling > 99)
    return -EINVAL;


  tbthread_t self = tbthread_self();
  int locked = 0;
  if(mutex->owner != self) {
    lock_normal(mutex);
    locked = 1;
  }
  if(old_ceiling)
    *old_ceiling = SCHED_INFO_PRIORITY(mutex->sched_info);
  mutex->sched_info = SCHED_INFO_PACK(SCHED_FIFO, ceiling);

  if(locked)
    unlock_normal(mutex);
  return 0;
}
