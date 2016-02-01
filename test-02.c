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
  int num = *(int*)arg;
  int i;

  tbprint("Hello from thread #%d, ptr: 0x%llx\n", num, tbthread_self());

  return 0;
}

//------------------------------------------------------------------------------
// Start the show
//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  tbthread_t       thread[5];
  int              targ[5];
  tbthread_attr_t  attr;
  void            *ret;
  int              st;
  tbthread_attr_init(&attr);
  for(int i = 0; i < 5; ++i) {
    targ[i] = i;
    st = tbthread_create(&thread[i], &attr, thread_func, &targ[i]);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, strerror(-st));
      return 1;
    }
  }

  tbprint("Threads spawned successfully\n");

  tbsleep(3);

  return 0;
};
