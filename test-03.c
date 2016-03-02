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

#include <tb.h>
#include <string.h>

//------------------------------------------------------------------------------
// Test normal mutex
//------------------------------------------------------------------------------
void *thread_func_normal(void *arg)
{
  tbthread_t self = tbthread_self();
  tbthread_mutex_t *mutex = (tbthread_mutex_t*)arg;
  int locked = 0;
  if(!tbthread_mutex_trylock(mutex)) {
    tbprint("[thread 0x%llx] Trying to actuire the mutex succeeded\n", self);
    locked = 1;
  }
  else
    tbprint("[thread 0x%llx] Trying to actuire the mutex failed\n", self);

  if(!locked)
    tbthread_mutex_lock(mutex);
  tbprint("[thread 0x%llx] Starting normal mutex test\n", self);
  tbsleep(1);
  tbprint("[thread 0x%llx] Finishing normal mutex test\n", self);
  tbthread_mutex_unlock(mutex);
  return 0;
}

//------------------------------------------------------------------------------
// Test errorcheck mutex
//------------------------------------------------------------------------------
void *thread_func_errorcheck1(void *arg)
{
  tbthread_t self = tbthread_self();
  tbthread_mutex_t *mutex = (tbthread_mutex_t*)arg;

  tbthread_mutex_lock(mutex);
  tbprint("[thread 0x%llx] Starting errorcheck mutex test\n", self);
  if(tbthread_mutex_lock(mutex) == -EDEADLK)
    tbprint("[thread 0x%llx] Trying to lock again would deadlock\n", self);
  tbsleep(2);
  tbprint("[thread 0x%llx] Finishing errorcheck mutex test\n", self);
  tbthread_mutex_unlock(mutex);
  return 0;
}

void *thread_func_errorcheck2(void *arg)
{
  tbthread_t self = tbthread_self();
  tbthread_mutex_t *mutex = (tbthread_mutex_t*)arg;
  tbsleep(1);
  if(tbthread_mutex_unlock(mutex) == -EPERM)
    tbprint("[thread 0x%llx] Trying to unlock a mutex we don't own\n", self);
  tbthread_mutex_lock(mutex);
  tbprint("[thread 0x%llx] Starting errorcheck mutex test\n", self);
  tbsleep(1);
  tbprint("[thread 0x%llx] Finishing errorcheck mutex test\n", self);
  tbthread_mutex_unlock(mutex);
  if(tbthread_mutex_unlock(mutex) == -EPERM)
    tbprint("[thread 0x%llx] Trying to unlock unlocked mutex\n", self);
  return 0;
}

//------------------------------------------------------------------------------
// Test recursive mutex
//------------------------------------------------------------------------------
void *thread_func_recursive(void *arg)
{
  tbthread_t self = tbthread_self();
  tbthread_mutex_t *mutex = (tbthread_mutex_t*)arg;
  int locked = 0;
  if(!tbthread_mutex_trylock(mutex)) {
    tbprint("[thread 0x%llx] Trying to actuire the mutex succeeded\n", self);
    locked = 1;
  }
  else
    tbprint("[thread 0x%llx] Trying to actuire the mutex failed\n", self);

  if(!locked)
    tbthread_mutex_lock(mutex);
  tbthread_mutex_lock(mutex);
  tbthread_mutex_lock(mutex);
  tbprint("[thread 0x%llx] Starting recursive mutex test\n", self);
  tbsleep(1);
  tbprint("[thread 0x%llx] Finishing recursive mutex test\n", self);
  tbthread_mutex_unlock(mutex);
  tbthread_mutex_unlock(mutex);
  tbthread_mutex_unlock(mutex);
  return 0;
}

//------------------------------------------------------------------------------
// Start the show
//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  tbthread_init();

  tbthread_t       thread[5];
  tbthread_attr_t  attr;
  void            *ret;
  int              st = 0;

  //----------------------------------------------------------------------------
  // Initialize the mutexes
  //----------------------------------------------------------------------------
  tbthread_mutexattr_t mattr;
  tbthread_mutex_t     mutex_normal;
  tbthread_mutex_t     mutex_errorcheck;
  tbthread_mutex_t     mutex_recursive;

  tbthread_mutexattr_init(&mattr);
  tbthread_mutex_init(&mutex_normal, 0);
  tbthread_mutexattr_settype(&mattr, TBTHREAD_MUTEX_ERRORCHECK);
  tbthread_mutex_init(&mutex_errorcheck, &mattr);
  tbthread_mutexattr_settype(&mattr, TBTHREAD_MUTEX_RECURSIVE);;
  tbthread_mutex_init(&mutex_recursive, &mattr);

  //----------------------------------------------------------------------------
  // Spawn the threads to test the normal mutex
  //----------------------------------------------------------------------------
  tbprint("[thread main] Testing normal mutex\n");
  tbthread_attr_init(&attr);
  for(int i = 0; i < 5; ++i) {
    st = tbthread_create(&thread[i], &attr, thread_func_normal, &mutex_normal);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
    tbthread_detach(thread[i]);
  }

  tbprint("[thread main] Threads spawned successfully\n");
  tbprint("[thread main] Sleeping 7 seconds\n");
  tbsleep(7);

  //----------------------------------------------------------------------------
  // Spawn threads to thest the errorcheck mutex
  //----------------------------------------------------------------------------
  void *(*errChkFunc[2])(void *) = {
    thread_func_errorcheck1, thread_func_errorcheck2 };
  tbprint("---\n");
  tbprint("[thread main] Testing errorcheck mutex\n");
  for(int i = 0; i < 2; ++i) {
    st = tbthread_create(&thread[i], &attr, errChkFunc[i], &mutex_errorcheck);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
    tbthread_detach(thread[i]);
  }

  tbprint("[thread main] Threads spawned successfully\n");
  tbprint("[thread main] Sleeping 5 seconds\n");
  tbsleep(5);

  //----------------------------------------------------------------------------
  // Spawn the threads to test the recursive mutex
  //----------------------------------------------------------------------------
  tbprint("---\n");
  tbprint("[thread main] Testing recursive mutex\n");
  tbthread_attr_init(&attr);
  for(int i = 0; i < 5; ++i) {
    st = tbthread_create(&thread[i], &attr, thread_func_recursive,
      &mutex_recursive);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
    tbthread_detach(thread[i]);
  }

  tbprint("[thread main] Threads spawned successfully\n");
  tbprint("[thread main] Sleeping 7 seconds\n");
  tbsleep(7);

exit:
  tbthread_finit();
  return st;
};
