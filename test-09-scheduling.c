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
// Policy to string
//------------------------------------------------------------------------------
const char *strpolicy(int policy)
{
  switch(policy)
  {
    case SCHED_NORMAL: return "SCHED_NORMAL";
    case SCHED_FIFO: return "SCHED_FIFO";
    case SCHED_RR: return "SCHED_RR";
    default: return "UNKNOWN";
  }
}

//------------------------------------------------------------------------------
// Thread function
//------------------------------------------------------------------------------
void *thread_func(void *arg)
{
  tbthread_t self = tbthread_self();
  int policy, priority;
  tbthread_getschedparam(self, &policy, &priority);
  tbprint("[thread 0x%llx] Started. Policy: %s, priority: %d\n", self,
    strpolicy(policy), priority);
  for(uint64_t i = 0; i < 5000000000ULL; ++i);
  tbprint("[thread 0x%llx] Done. Policy: %s, priority: %d\n", self,
    strpolicy(policy), priority);
  return 0;
}

//------------------------------------------------------------------------------
// Run the threads
//------------------------------------------------------------------------------
int run(tbthread_attr_t attr[10])
{
  tbprint("Testing policy: %s\n", strpolicy(attr[0].sched_policy));
  tbthread_t thread[10];
  int st = 0;
  for(int i = 0; i < 10; ++i) {
    st = tbthread_create(&thread[i], &attr[i], thread_func, 0);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      return st;
    }
  }

  tbprint("[thread main] Threads spawned successfully\n");

  for(int i = 0; i < 10; ++i) {
    st = tbthread_join(thread[i], 0);
    if(st != 0) {
      tbprint("Failed to join thread %d: %s\n", i, tbstrerror(-st));
      return st;
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
  tbthread_attr_t  attr[10];
  int              st = 0;

  //----------------------------------------------------------------------------
  // Warn about not being root
  //----------------------------------------------------------------------------
  if(SYSCALL0(__NR_getuid) != 0)
    tbprint("[!!!] You should run this test as root\n");

  //----------------------------------------------------------------------------
  // Run the threads with normal policy
  //----------------------------------------------------------------------------
  for(int i = 0; i < 10; ++i)
    tbthread_attr_init(&attr[i]);

  if((st = run(attr)))
    goto exit;

  //----------------------------------------------------------------------------
  // Run the threads with FIFO policy
  //----------------------------------------------------------------------------
  tbthread_setschedparam(tbthread_self(), SCHED_FIFO, 99);
  for(int i = 0; i < 10; ++i) {
    int priority = 10-i/3;
    tbthread_attr_setschedpolicy(&attr[i], SCHED_FIFO);
    tbthread_attr_setschedpriority(&attr[i], priority);
    tbthread_attr_setinheritsched(&attr[i], TBTHREAD_EXPLICIT_SCHED);
  }

  if((st = run(attr)))
    goto exit;

exit:
  tbthread_finit();
  return st;
};
