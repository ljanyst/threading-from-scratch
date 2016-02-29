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

//------------------------------------------------------------------------------
// Cancelation handler
//------------------------------------------------------------------------------
void tb_cancel_handler(int sig, siginfo_t *si, void *ctx)
{
  if(sig != SIGCANCEL || si->si_pid != tb_pid || si->si_code != SI_TKILL)
    return;

  tbthread_t self = tbthread_self();
  if(self->cancel_status & TB_CANCEL_DEFERRED)
    return;

  tbthread_testcancel();
}

//------------------------------------------------------------------------------
// Cancel a thread
//------------------------------------------------------------------------------
int tbthread_cancel(tbthread_t thread)
{
  int ret = 0;
  tbthread_mutex_lock(&desc_mutex);

  if(!list_find_elem(&used_desc, thread)) {
    ret = -ESRCH;
    goto exit;
  }

  uint8_t val, newval;
  while(1) {
    newval = val = thread->cancel_status;
    if(val & TB_CANCELING)
      goto exit;

    newval |= TB_CANCELING;
    if(__sync_bool_compare_and_swap(&thread->cancel_status, val, newval))
      break;
  }
  if((val & TB_CANCEL_ENABLED) && !(val & TB_CANCEL_DEFERRED))
    SYSCALL3(__NR_tgkill, tb_pid, thread->exit_futex, SIGCANCEL);

exit:
  tbthread_mutex_unlock(&desc_mutex);
  return ret;
}

//------------------------------------------------------------------------------
// Set the cancelation bit
//------------------------------------------------------------------------------
static int set_cancelation_bit(int bitmask, int value)
{
  tbthread_t thread = tbthread_self();
  int val, newval, oldbit;
  while(1) {
    newval = val = thread->cancel_status;
    if(val & bitmask) oldbit = 1;
    else oldbit = 0;
    if(value) newval |= bitmask;
    else newval &= ~bitmask;

    if(__sync_bool_compare_and_swap(&thread->cancel_status, val, newval))
      break;
  }
  return oldbit;
}

//------------------------------------------------------------------------------
// Set cancelation state
//------------------------------------------------------------------------------
int tbthread_setcancelstate(int state, int *oldstate)
{
  if(state != TBTHREAD_CANCEL_ENABLE && state != TBTHREAD_CANCEL_DISABLE)
    return -EINVAL;

  int olds = set_cancelation_bit(TB_CANCEL_ENABLED, state);
  if(oldstate)
    *oldstate = olds;

  if(state == TBTHREAD_CANCEL_ENABLE)
    tbthread_testcancel();

  return 0;
}

//------------------------------------------------------------------------------
// Set cancelation type
//------------------------------------------------------------------------------
int tbthread_setcanceltype(int type, int *oldtype)
{
  if(type != TBTHREAD_CANCEL_DEFERRED && type != TBTHREAD_CANCEL_ASYNCHRONOUS)
    return -EINVAL;

  int oldt = set_cancelation_bit(TB_CANCEL_DEFERRED, type);
  if(oldtype)
    *oldtype = oldt;

  tbthread_testcancel();

  return 0;
}

//------------------------------------------------------------------------------
// Check if the thread should be canceled
//------------------------------------------------------------------------------
void tbthread_testcancel()
{
  tbthread_t thread = tbthread_self();
  uint8_t val, newval;

  while(1) {
    newval = val = thread->cancel_status;
    if(!(val & TB_CANCEL_ENABLED) || !(val & TB_CANCELING) ||
       (val & TB_CANCELED))
      return;
    newval |= TB_CANCELED;
    if(__sync_bool_compare_and_swap(&thread->cancel_status, val, newval))
      break;
  }
  tbthread_exit(TBTHREAD_CANCELED);
}
