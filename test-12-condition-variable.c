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

tbthread_mutex_t mutex   = TBTHREAD_MUTEX_INITIALIZER;
tbthread_cond_t  condvar = TBTHREAD_COND_INITIALIZER;
int num = 0;

//------------------------------------------------------------------------------
// Thread function
//------------------------------------------------------------------------------
void *waiter_func(void *arg)
{
  tbthread_t self = tbthread_self();
  tbprint("[thread 0x%llx] Starting a waiter\n", self);
  for(int i = 0; i < 2; ++i) {
    tbthread_mutex_lock(&mutex);
    tbprint("[thread 0x%llx] Waiting for the condvar\n", self);
    tbthread_cond_wait(&condvar, &mutex);
    ++num;
    tbsleep(1);
    tbprint("[thread 0x%llx] Num = %d\n", self, num);
    tbthread_mutex_unlock(&mutex);
    tbsleep(1);
  }
  tbprint("[thread 0x%llx] Finishing a waiter\n", self);
  return 0;
}

//------------------------------------------------------------------------------
// Thread function
//------------------------------------------------------------------------------
void *poster_func(void *arg)
{
  tbthread_t self = tbthread_self();
  tbprint("[thread 0x%llx] Starting the poster\n", self);
  tbsleep(1);
  tbthread_mutex_lock(&mutex);
  for(int i = 0; i < 5; ++i) {
    tbprint("[thread 0x%llx] Signal %d\n", self, i);
    tbthread_cond_signal(&condvar);
  }
  tbthread_mutex_unlock(&mutex);
  tbsleep(10);
  tbthread_mutex_lock(&mutex);
  tbprint("[thread 0x%llx] Broadcast\n", self);
  tbthread_cond_broadcast(&condvar);
  tbthread_mutex_unlock(&mutex);
  tbprint("[thread 0x%llx] Finishing the poster\n", self);
  return 0;
}

//------------------------------------------------------------------------------
// Start the show
//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  tbthread_init();

  tbthread_t       thread[6];
  void *(*thfunc[6])(void *);
  tbthread_attr_t  attr;
  int              st = 0;

  for(int i = 0; i < 5; ++i)
    thfunc[i] = waiter_func;
  thfunc[5] = poster_func;

  //----------------------------------------------------------------------------
  // Spawn the threads
  //----------------------------------------------------------------------------
  tbthread_attr_init(&attr);
  for(int i = 0; i < 6; ++i) {
    st = tbthread_create(&thread[i], &attr, thfunc[i], 0);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
  }

  tbprint("[thread main] Threads spawned successfully\n");

  for(int i = 0; i < 6; ++i) {
    st = tbthread_join(thread[i], 0);
    if(st != 0) {
      tbprint("Failed to join thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
  }

  tbprint("[thread main] Threads joined\n");

exit:
  tbthread_finit();
  return st;
};
