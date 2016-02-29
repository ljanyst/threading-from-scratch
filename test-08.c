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
// 
//------------------------------------------------------------------------------
void cleanup(void *arg)
{
  tbprint("[thread 0x%llx] Canceled\n", arg);
}

//------------------------------------------------------------------------------
// The once initializer
//------------------------------------------------------------------------------
tbthread_t once_thread;
int once_one = 0;
void once_func()
{
  tbthread_t self = tbthread_self();
  tbthread_setcancelstate(TBTHREAD_CANCEL_DISABLE, 0);
  tbthread_cleanup_push(cleanup, self);
  tbprint("[thread 0x%llx] Running the once function\n", self);
  if(!once_one) {
    once_thread = self;
    once_one = 1;
  }
  tbthread_setcanceltype(TBTHREAD_CANCEL_ASYNCHRONOUS, 0);
  tbthread_setcancelstate(TBTHREAD_CANCEL_ENABLE, 0);
  tbsleep(5);
}

tbthread_once_t once = TBTHREAD_ONCE_INIT;

//------------------------------------------------------------------------------
// Thread function
//------------------------------------------------------------------------------
void *thread_func(void *arg)
{
  tbthread_t self = tbthread_self();
  tbprint("[thread 0x%llx] About to run the once function\n", self);
  tbthread_once(&once, once_func);
  tbprint("[thread 0x%llx] Finished running the once function\n", self);
  return 0;
}

//------------------------------------------------------------------------------
// Start the show
//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  tbthread_init();

  tbthread_t       thread[5];
  int              targ[5];
  tbthread_attr_t  attr;
  int              st = 0;

  //----------------------------------------------------------------------------
  // Spawn the threads
  //----------------------------------------------------------------------------
  tbthread_attr_init(&attr);
  for(int i = 0; i < 5; ++i) {
    targ[i] = i;
    st = tbthread_create(&thread[i], &attr, thread_func, &targ[i]);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
  }

  tbprint("[thread main] Threads spawned successfully\n");
  tbsleep(1);
  tbprint("[thread main] Canceling the once thread: 0x%llx\n", once_thread);
  tbthread_cancel(once_thread);

  for(int i = 0; i < 5; ++i) {
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
