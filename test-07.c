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

#define CANCEL_DISABLED 0
#define CANCEL_DEFERRED 1
#define CANCEL_ASYNC 2

//------------------------------------------------------------------------------
// Thread function
//------------------------------------------------------------------------------
void *thread_func(void *arg)
{
  tbthread_t self = tbthread_self();
  int mode = *(int *)arg;
  tbthread_setcancelstate(TBTHREAD_CANCEL_DISABLE, 0);
  tbprint("[thread 0x%llx] Started\n", self);

  if(mode == CANCEL_ASYNC)
    tbthread_setcanceltype(TBTHREAD_CANCEL_ASYNCHRONOUS, 0);

  if(mode != CANCEL_DISABLED)
    tbthread_setcancelstate(TBTHREAD_CANCEL_ENABLE, 0);

  for(int i = 0; i < 5; ++i) {
    if(mode != CANCEL_ASYNC)
      tbprint("[thread 0x%llx] Ping\n", self);
    tbsleep(1);
    if(mode != CANCEL_ASYNC)
      tbthread_testcancel();
  }
  tbprint("[thread 0x%llx] Not canceled\n", self);
  return 0;
}

//------------------------------------------------------------------------------
// Run threads in a given mode
//------------------------------------------------------------------------------
int run(int mode, const char *msg)
{
  tbthread_t       thread[5];
  tbthread_attr_t  attr;
  int              st = 0;

  //----------------------------------------------------------------------------
  // Spawn the threads
  //----------------------------------------------------------------------------
  tbprint("[thread main] %s\n", msg);
  tbthread_attr_init(&attr);
  for(int i = 0; i < 5; ++i) {
    st = tbthread_create(&thread[i], &attr, thread_func, &mode);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      return st;
    }
  }

  tbprint("[thread main] Threads spawned successfully\n");
  tbsleep(1);

  for(int i = 0; i < 5; ++i) {
    tbprint("[thread main] Canceling thread #%d\n", i);
    tbthread_cancel(thread[i]);
  }

  void *ret;
  for(int i = 0; i < 5; ++i) {
    st = tbthread_join(thread[i], &ret);
    if(st != 0) {
      tbprint("Failed to join thread %d: %s\n", i, tbstrerror(-st));
      return st;
    }

    if(mode != CANCEL_DISABLED && ret != TBTHREAD_CANCELED) {
      tbprint("Thread #%d was not canceled\n", i);
      return -EFAULT;
    }
  }

  tbprint("[thread main] Threads joined\n");
  tbprint("---\n");
  return 0;
}

//------------------------------------------------------------------------------
// Start the show
//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  tbthread_init();
  int st = 0;

  if((st = run(CANCEL_DISABLED, "Test CANCEL_DISABLED")))
    goto exit;

  if((st = run(CANCEL_DEFERRED, "Test CANCEL_DEFERRED")))
    goto exit;

  if((st = run(CANCEL_ASYNC, "Test CANCEL_ASYNC")))
    goto exit;

exit:
  tbthread_finit();
  return st;
};
