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
// Lock for reading
//------------------------------------------------------------------------------
int tbthread_rwlock_rdlock(tbthread_rwlock_t *rwlock)
{
  while(1) {
    tb_futex_lock(&rwlock->lock);

    if(!rwlock->writer && !rwlock->writers_queued) {
      ++rwlock->readers;
      tb_futex_unlock(&rwlock->lock);
      return 0;
    }
    int sleep_status = rwlock->rd_futex;

    tb_futex_unlock(&rwlock->lock);

    SYSCALL3(__NR_futex, &rwlock->rd_futex, FUTEX_WAIT, sleep_status);
  }
}

//------------------------------------------------------------------------------
// Lock for writing
//------------------------------------------------------------------------------
int tbthread_rwlock_wrlock(tbthread_rwlock_t *rwlock)
{
  int queued = 0;
  while(1) {
    tb_futex_lock(&rwlock->lock);

    if(!queued) {
      queued = 1;
      ++rwlock->writers_queued;
    }

    if(!rwlock->writer && !rwlock->readers) {
      rwlock->writer = tbthread_self();
      --rwlock->writers_queued;
      tb_futex_unlock(&rwlock->lock);
      return 0;
    }
    int sleep_status = rwlock->wr_futex;

    tb_futex_unlock(&rwlock->lock);

    SYSCALL3(__NR_futex, &rwlock->wr_futex, FUTEX_WAIT, sleep_status);
  }
}

//------------------------------------------------------------------------------
// Unlock
//------------------------------------------------------------------------------
int tbthread_rwlock_unlock(tbthread_rwlock_t *rwlock)
{
  tb_futex_lock(&rwlock->lock);
  if(rwlock->writer) {
    rwlock->writer = 0;
    if(rwlock->writers_queued) {
      __sync_fetch_and_add(&rwlock->wr_futex, 1);
      SYSCALL3(__NR_futex, &rwlock->wr_futex, FUTEX_WAKE, 1);
    } else {
      __sync_fetch_and_add(&rwlock->rd_futex, 1);
      SYSCALL3(__NR_futex, &rwlock->rd_futex, FUTEX_WAKE, INT_MAX);
    }
    goto exit;
  }

  --rwlock->readers;
  if(!rwlock->readers && rwlock->writers_queued) {
    __sync_fetch_and_add(&rwlock->wr_futex, 1);
    SYSCALL3(__NR_futex, &rwlock->wr_futex, FUTEX_WAKE, 1);
  }

exit:
  tb_futex_unlock(&rwlock->lock);
  return 0;
}

//------------------------------------------------------------------------------
// Try to lock for reading
//------------------------------------------------------------------------------
int tbthread_rwlock_tryrdlock(tbthread_rwlock_t *rwlock)
{
  tb_futex_lock(&rwlock->lock);
  int status = -EBUSY;
  if(!rwlock->writer && !rwlock->writers_queued) {
    ++rwlock->readers;
    status = 0;
    goto exit;
  }
exit:
  tb_futex_unlock(&rwlock->lock);
  return status;
}

//------------------------------------------------------------------------------
// Try to lock for writing
//------------------------------------------------------------------------------
int tbthread_rwlock_trywrlock(tbthread_rwlock_t *rwlock)
{
  tb_futex_lock(&rwlock->lock);
  int status = -EBUSY;
  if(!rwlock->writer && !rwlock->readers) {
    rwlock->writer = tbthread_self();
    status = 0;
    goto exit;
  }
exit:
  tb_futex_unlock(&rwlock->lock);
  return status;
}
