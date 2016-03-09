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

tbthread_rwlock_t lock = TBTHREAD_RWLOCK_INIT;
int a = 0;
int b = 0;

//------------------------------------------------------------------------------
// Thread function
//------------------------------------------------------------------------------
void *reader_func(void *arg)
{
  tbthread_t self = tbthread_self();
  tbprint("[thread 0x%llx] Starting a reader\n", self);
  while(1) {
    tbthread_rwlock_rdlock(&lock);
    int br = (b == 30);
    if(a != b) tbprint("[thread 0x%llx] a != b\n", self);
    tbthread_rwlock_unlock(&lock);
    if(br)
      break;
  }
  tbprint("[thread 0x%llx] Finishing a reader\n", self);
  return 0;
}

//------------------------------------------------------------------------------
// Thread function
//------------------------------------------------------------------------------
void *writer_func(void *arg)
{
  tbthread_t self = tbthread_self();
  tbprint("[thread 0x%llx] Starting a writer\n", self);
  for(int i = 0; i < 10; ++i) {
    tbthread_rwlock_wrlock(&lock);
    a = b+1;
    tbprint("[thread 0x%llx] Bumped a to %d\n", self, a);
    for(uint64_t z = 0; z < 200000000ULL; ++z);
    b = a;
    tbprint("[thread 0x%llx] Bumped b to %d\n", self, b);
    tbthread_rwlock_unlock(&lock);
    tbsleep(4);
  }
  tbprint("[thread 0x%llx] Finishing a writer\n", self);
  return 0;
}

//------------------------------------------------------------------------------
// Start the show
//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  tbthread_init();

  tbthread_t       thread[10];
  void *(*thfunc[10])(void *);
  tbthread_attr_t  attr;
  int              st = 0;

  for(int i = 0; i < 7; ++i)
    thfunc[i] = reader_func;
  for(int i = 7; i < 10; ++i)
    thfunc[i] = writer_func;

  //----------------------------------------------------------------------------
  // Spawn the threads
  //----------------------------------------------------------------------------
  tbthread_attr_init(&attr);
  for(int i = 0; i < 10; ++i) {
    st = tbthread_create(&thread[i], &attr, thfunc[i], 0);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      goto exit;
    }
  }

  tbprint("[thread main] Threads spawned successfully\n");

  for(int i = 0; i < 10; ++i) {
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
