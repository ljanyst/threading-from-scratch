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
// Thread function
//------------------------------------------------------------------------------
void *thread_func(void *arg)
{
  tbthread_t self = tbthread_self();
  int num = *(int*)arg;
  int i;

  for(i = 0; i < 5; ++i) {
    tbprint("[thread 0x%llx] Hello from thread #%d\n", self, num);
    tbsleep(1);
  }
  return arg;
}

//------------------------------------------------------------------------------
// Thread joiner
//------------------------------------------------------------------------------
struct joiner_arg {
  int sleep_before;
  tbthread_t thread;
  int join_status;
  int sleep_after;
};

void *thread_joiner_func(void *arg)
{
  tbthread_t self = tbthread_self();
  struct joiner_arg *a = (struct joiner_arg *)arg;

  tbprint("[thread 0x%llx] Sleeping %d seconds\n", self, a->sleep_before);
  tbsleep(a->sleep_before);
  int ret = tbthread_join(a->thread, 0);
  if(ret == a->join_status)
    tbprint("[thread 0x%llx] Joining performed as expected\n", self);
  else
    tbprint("[thread 0x%llx] Unexpected result of joining: %d\n", self, ret);  
  tbprint("[thread 0x%llx] Sleeping %d seconds\n", self, a->sleep_after);
  tbsleep(a->sleep_after);
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
  void            *ret;
  int              st = 0;
  tbthread_attr_init(&attr);

  //----------------------------------------------------------------------------
  // Test the joiner waiting for threads to finish
  //----------------------------------------------------------------------------
  tbprint("[thread main] Testing the joiner waiting for threads\n");
  for(int i = 0; i < 5; ++i) {
    targ[i] = i;
    st = tbthread_create(&thread[i], &attr, thread_func, &targ[i]);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
  }

  tbprint("[thread main] Threads spawned successfully\n");

  for(int i = 0; i < 5; ++i) {
    void *ret;
    st = tbthread_join(thread[i], &ret);
    if(st != 0) {
      tbprint("Failed to join thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
    tbprint("[thread main] Thread %d joined, retval; %d\n", i,  *(int*)ret);
  }

  //----------------------------------------------------------------------------
  // Let the threads finish, wait, and then join
  //----------------------------------------------------------------------------
  tbprint("---\n");
  tbprint("[thread main] Testing the threads exiting before joining\n");
  for(int i = 0; i < 5; ++i) {
    targ[i] = i;
    st = tbthread_create(&thread[i], &attr, thread_func, &targ[i]);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
  }

  tbprint("[thread main] Threads spawned successfully\n");
  tbprint("[thread main] Sleeping 10 seconds\n");
  tbsleep(10);

  for(int i = 0; i < 5; ++i) {
    void *ret;
    st = tbthread_join(thread[i], &ret);
    if(st != 0) {
      tbprint("Failed to join thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
    tbprint("Thread %d joined, retval; %d\n", i,  *(int*)ret);
  }

  //----------------------------------------------------------------------------
  // Join detached threads that exist
  //----------------------------------------------------------------------------
  tbprint("---\n");
  tbprint("[thread main] Join detached threads that still exist\n");
  for(int i = 0; i < 5; ++i) {
    targ[i] = i;
    st = tbthread_create(&thread[i], &attr, thread_func, &targ[i]);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
    tbthread_detach(thread[i]);
  }

  tbprint("[thread main] Threads spawned successfully\n");

  for(int i = 0; i < 5; ++i) {
    void *ret;
    st = tbthread_join(thread[i], &ret);
    if(st != -EINVAL) {
      tbprint("Thread %d (0x%llx) does not seem to be detached\n", i);
      goto exit;
    }
  }
  tbprint("[thread main] Sleeping 10 seconds\n");
  tbsleep(10);

  //----------------------------------------------------------------------------
  // Join detached threads that exited
  //----------------------------------------------------------------------------
  tbprint("---\n");
  tbprint("[thread main] Join detached threads that exited\n");
  for(int i = 0; i < 5; ++i) {
    targ[i] = i;
    st = tbthread_create(&thread[i], &attr, thread_func, &targ[i]);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
    tbthread_detach(thread[i]);
  }

  tbprint("[thread main] Threads spawned successfully\n");
  tbprint("[thread main] Sleeping 10 seconds\n");
  tbsleep(10);

  for(int i = 0; i < 5; ++i) {
    void *ret;
    st = tbthread_join(thread[i], &ret);
    if(st != -ESRCH) {
      tbprint("Thread %d (0x%llx) exists but shouldn't\n", i);
      goto exit;
    }
  }

  //----------------------------------------------------------------------------
  // Join self
  //----------------------------------------------------------------------------
  if(tbthread_join(tbthread_self(), 0) != -EDEADLK)
    tbprint("Joining self shouldn't have succeeded\n");

  //----------------------------------------------------------------------------
  // Mutual joining
  //----------------------------------------------------------------------------
  tbprint("---\n");
  tbprint("[thread main] Test mutual joining\n");
  struct joiner_arg jarg[3];
  memset(jarg, 0, sizeof(jarg));
  jarg[0].sleep_before = 4;
  jarg[1].sleep_before = 2;
  jarg[0].join_status  = -EDEADLK;
  jarg[1].join_status  = 0;

  for(int i = 0; i < 2; ++i) {
    st = tbthread_create(&thread[i], &attr, thread_joiner_func, &jarg[i]);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
  }
  jarg[0].thread = thread[1];
  jarg[1].thread = thread[0];

  tbprint("[thread main] Threads spawned successfully\n");

  st = tbthread_join(thread[1], 0);
  if(st != 0) {
    tbprint("Unable to join thread 0x%llx\n", thread[1]);
    goto exit;
  }
  tbprint("Thread joined\n");

  //----------------------------------------------------------------------------
  // Double join
  //----------------------------------------------------------------------------
  tbprint("---\n");
  tbprint("[thread main] Test double join\n");
  memset(jarg, 0, sizeof(jarg));
  jarg[0].sleep_before = 5;
  jarg[1].sleep_before = 2;
  jarg[2].sleep_before = 3;
  jarg[0].join_status  = -ESRCH;
  jarg[1].join_status  = 0;
  jarg[2].join_status  = -EINVAL;

  for(int i = 0; i < 3; ++i) {
    st = tbthread_create(&thread[i], &attr, thread_joiner_func, &jarg[i]);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
  }
  jarg[1].thread = thread[0];
  jarg[2].thread = thread[0];

  tbprint("[thread main] Threads spawned successfully\n");

  for(int i = 1; i < 3; ++i) {
    st = tbthread_join(thread[i], 0);
    if(st != 0) {
      tbprint("Failed to join thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
    tbprint("Thread %d joined\n", i);
  }

exit:
  tbthread_finit();
  return st;
};
