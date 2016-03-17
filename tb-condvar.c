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
#include "tb-private.h"

#include <limits.h>
#include <linux/futex.h>

//------------------------------------------------------------------------------
// Broadcast
//------------------------------------------------------------------------------
int tbthread_cond_broadcast(tbthread_cond_t *cond)
{
  tb_futex_lock(&cond->lock);
  if(!cond->waiters)
    goto exit;
  ++cond->futex;
  ++cond->broadcast_seq;
  SYSCALL3(__NR_futex, &cond->futex, FUTEX_WAKE, INT_MAX);
exit:
  tb_futex_unlock(&cond->lock);
  return 0;
}

//------------------------------------------------------------------------------
// Signal
//------------------------------------------------------------------------------
int tbthread_cond_signal(tbthread_cond_t *cond)
{
  tb_futex_lock(&cond->lock);
  if(cond->waiters == cond->signal_num)
    goto exit;
  ++cond->futex;
  ++cond->signal_num;
  SYSCALL3(__NR_futex, &cond->futex, FUTEX_WAKE, 1);
exit:
  tb_futex_unlock(&cond->lock);
  return 0;
}

//------------------------------------------------------------------------------
// Wait
//------------------------------------------------------------------------------
int tbthread_cond_wait(tbthread_cond_t *cond, tbthread_mutex_t *mutex)
{
  tb_futex_lock(&cond->lock);
  int st = 0;

  if(!cond->mutex)
    cond->mutex = mutex;

  if(cond->mutex != mutex) {
    st = -EINVAL;
    goto error;
  }

  st = tbthread_mutex_unlock(mutex);
  if(st) goto error;

  ++cond->waiters;
  int bseq = cond->broadcast_seq;
  int futex = cond->futex;
  tb_futex_unlock(&cond->lock);

  while(1) {
    st = SYSCALL3(__NR_futex, &cond->futex, FUTEX_WAIT, futex);
    if(st == -EINTR)
      continue;

    tb_futex_lock(&cond->lock);
    if(cond->signal_num) {
      --cond->signal_num;
      goto exit;
    }

    if(bseq != cond->broadcast_seq)
      goto exit;
    tb_futex_unlock(&cond->lock);
  }

error:
  if(!cond->waiters)
    cond->mutex = 0;

  tb_futex_unlock(&cond->lock);
  return st;

exit:
  --cond->waiters;
  if(!cond->waiters)
    cond->mutex = 0;

  tb_futex_unlock(&cond->lock);
  tbthread_mutex_lock(mutex);
  return st;
}
