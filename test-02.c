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
// TLS keys
//------------------------------------------------------------------------------
tbthread_key_t key1;
tbthread_key_t key2;
tbthread_key_t key3;

//------------------------------------------------------------------------------
// TLS destructors
//------------------------------------------------------------------------------
void dest1(void *data)
{
  tbprint("[thread 0x%llx] Error: calling dest1\n", tbthread_self());
}

void dest2(void *data)
{
  tbprint("[thread 0x%llx] Error: calling dest2\n", tbthread_self());
}

void dest3(void *data)
{
  tbprint("[thread 0x%llx] Calling dest3\n", tbthread_self());
  free(data);
}

//------------------------------------------------------------------------------
// Thread function
//------------------------------------------------------------------------------
void *thread_func(void *arg)
{
  int num = *(int*)arg;
  int i;
  tbthread_t self = tbthread_self();
  tbprint("[thread 0x%llx] Hello from thread #%d\n", self, num);
  tbprint("[thread 0x%llx] Allocating the TLS data\n", self);
  void *data1 = malloc(20);
  void *data2 = malloc(20);
  void *data3 = malloc(20);
  tbthread_setspecific(key1, data1);
  tbthread_setspecific(key2, data2);
  tbthread_setspecific(key3, data3);

  tbprint("[thread 0x%llx] Sleeping 3 seconds\n", self);
  tbsleep(3);
  void *data1r = tbthread_getspecific(key1);
  void *data2r = tbthread_getspecific(key2);
  void *data3r = tbthread_getspecific(key3);
  if(data1r != data1)
    tbprint("[thread 0x%llx] Error: datar != data\n", self);
  if(data2r != 0)
    tbprint("[thread 0x%llx] Error: data2r != 0\n", self);
  if(data3r != data3)
    tbprint("[thread 0x%llx] Error: data3r != data3\n", self);
  free(data2);
  free(data1);
  tbthread_setspecific(key1, 0);
  tbprint("[thread 0x%llx] Sleeping 1 second\n", self);
  tbsleep(1);
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

  //----------------------------------------------------------------------------
  // Allocate the keys
  //----------------------------------------------------------------------------
  tbthread_key_create(&key1, dest1);
  tbthread_key_create(&key2, dest2);
  tbthread_key_create(&key3, dest3);
  tbthread_key_delete(key1);
  tbthread_key_delete(key3);
  tbthread_key_create(&key3, dest3);
  tbthread_key_create(&key1, dest1);
  tbprint("[thread main] TLS keys: %u, %u, %u\n", key1, key2, key3);

  //----------------------------------------------------------------------------
  // Spawn the threads
  //----------------------------------------------------------------------------
  tbthread_attr_init(&attr);
  for(int i = 0; i < 5; ++i) {
    targ[i] = i;
    st = tbthread_create(&thread[i], &attr, thread_func, &targ[i]);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, strerror(-st));
      goto exit;
    }
    tbthread_detach(thread[i]);
  }

  tbprint("[thread main] Threads spawned successfully\n");
  tbprint("[thread main] Sleeping 1 second\n");
  tbsleep(1);
  tbprint("[thread main] Destroying key2\n");
  tbthread_key_delete(key2);
  tbprint("[thread main] Sleeping 10 seconds\n");
  tbsleep(10);

exit:
  tbthread_finit();
  return st;
};
