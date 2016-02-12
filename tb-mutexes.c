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

#include <linux/futex.h>

//------------------------------------------------------------------------------
// Normal mutex
//------------------------------------------------------------------------------
static int lock_normal(tbthread_mutex_t *mutex)
{
  while(1) {
    if(__sync_bool_compare_and_swap(&mutex->futex, 0, 1))
      return 0;
    SYSCALL3(__NR_futex, &mutex->futex, FUTEX_WAIT, 1);
  }
}

static int trylock_normal(tbthread_mutex_t *mutex)
{
  if(__sync_bool_compare_and_swap(&mutex->futex, 0, 1))
      return 0;
  return -EBUSY;
}

static int unlock_normal(tbthread_mutex_t *mutex)
{
  if(__sync_bool_compare_and_swap(&mutex->futex, 1, 0))
    SYSCALL3(__NR_futex, &mutex->futex, FUTEX_WAKE, 1);
  return 0;
}

//------------------------------------------------------------------------------
// Errorcheck mutex
//------------------------------------------------------------------------------
static int lock_errorcheck(tbthread_mutex_t *mutex)
{
  tbthread_t self = tbthread_self();
  if(mutex->owner == self)
    return -EDEADLK;
  lock_normal(mutex);
  mutex->owner = self;
  return 0;
}

static int trylock_errorcheck(tbthread_mutex_t *mutex)
{
  int ret = trylock_normal(mutex);
  if(ret == 0)
    mutex->owner = tbthread_self();
  return ret;
}

static int unlock_errorcheck(tbthread_mutex_t *mutex)
{
  if(mutex->owner != tbthread_self() || mutex->futex == 0)
    return -EPERM;
  mutex->owner = 0;
  unlock_normal(mutex);
  return 0;
}

//------------------------------------------------------------------------------
// Recursive mutex
//------------------------------------------------------------------------------
static int lock_recursive(tbthread_mutex_t *mutex)
{
  tbthread_t self = tbthread_self();
  if(mutex->owner != self) {
    lock_normal(mutex);
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
  if(mutex->owner != self && trylock_normal(mutex))
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
    return unlock_normal(mutex);
  }
  return 0;
}

//------------------------------------------------------------------------------
// Mutex function tables
//------------------------------------------------------------------------------
static int (*lockers[])(tbthread_mutex_t *) = {
  lock_normal,
  lock_errorcheck,
  lock_recursive
};

static int (*trylockers[])(tbthread_mutex_t *) = {
  trylock_normal,
  trylock_errorcheck,
  trylock_recursive
};

static int (*unlockers[])(tbthread_mutex_t *) = {
  unlock_normal,
  unlock_errorcheck,
  unlock_recursive
};

//------------------------------------------------------------------------------
// Init attributes
//------------------------------------------------------------------------------
int tbthread_mutexattr_init(tbthread_mutexattr_t *attr)
{
  attr->type = TBTHREAD_MUTEX_DEFAULT;
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
  uint8_t type = TBTHREAD_MUTEX_DEFAULT;
  if(attr)
    type = attr->type;
  mutex->futex   = 0;
  mutex->type    = type;
  mutex->owner   = 0;
  mutex->counter = 0;
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
