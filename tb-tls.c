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

//------------------------------------------------------------------------------
// Get the pointer of the currently running thread
//------------------------------------------------------------------------------
tbthread_t tbthread_self()
{
  tbthread_t self;
  asm("movq %%fs:0, %0\n\t" : "=r" (self));
  return self;
}

//------------------------------------------------------------------------------
// The keys and helpers
//------------------------------------------------------------------------------
static struct
{
  uint64_t seq;
  void (*destructor)(void *);
} keys[TBTHREAD_MAX_KEYS];

#define KEY_UNUSED(k) ((keys[k].seq&1) == 0)
#define KEY_ACQUIRE(k) (__sync_bool_compare_and_swap(&(keys[k].seq), keys[k].seq, keys[k].seq+1))
#define KEY_RELEASE(k) (__sync_bool_compare_and_swap(&(keys[k].seq), keys[k].seq, keys[k].seq+1))

//------------------------------------------------------------------------------
// Create a key
//------------------------------------------------------------------------------
int tbthread_key_create(tbthread_key_t *key, void (*destructor)(void *))
{
  for(tbthread_key_t i = 0; i < TBTHREAD_MAX_KEYS; ++i) {
    if(KEY_UNUSED(i) && KEY_ACQUIRE(i)) {
      *key = i;
      keys[i].destructor = destructor;
      return 0;
    }
  }
  return -ENOMEM;
}

//------------------------------------------------------------------------------
// Delete the key
//------------------------------------------------------------------------------
int tbthread_key_delete(tbthread_key_t key)
{
  if(key >= TBTHREAD_MAX_KEYS)
    return -EINVAL;

  if(!KEY_UNUSED(key) && KEY_RELEASE(key))
    return 0;

  return -EINVAL;
}

//------------------------------------------------------------------------------
// Get the thread specific data associated with the key
//------------------------------------------------------------------------------
void *tbthread_getspecific(tbthread_key_t key)
{
  if(key >= TBTHREAD_MAX_KEYS || KEY_UNUSED(key))
    return 0;

  tbthread_t self = tbthread_self();
  if(self->tls[key].seq == keys[key].seq)
    return self->tls[key].data;
  return 0;
}

//------------------------------------------------------------------------------
// Associate thread specific data with the key
//------------------------------------------------------------------------------
int tbthread_setspecific(tbthread_key_t key, void *value)
{
  if(key >= TBTHREAD_MAX_KEYS || KEY_UNUSED(key))
    return -EINVAL;

  tbthread_t self = tbthread_self();
  self->tls[key].seq = keys[key].seq;
  self->tls[key].data = value;
  return 0;
}

//------------------------------------------------------------------------------
// Call the destructors of all the non-null values
//------------------------------------------------------------------------------
void tb_tls_call_destructors()
{
  tbthread_t self = tbthread_self();
  for(tbthread_key_t i = 0; i < TBTHREAD_MAX_KEYS; ++i) {
    if(!KEY_UNUSED(i) && self->tls[i].seq == keys[i].seq &&
       self->tls[i].data && keys[i].destructor) {
       void *data = self->tls[i].data;
       self->tls[i].data = 0;
       keys[i].destructor(data);
     }
   }
}
