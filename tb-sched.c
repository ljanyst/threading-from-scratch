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
// Set scheduler
//------------------------------------------------------------------------------
struct tb_sched_param
{
  int sched_priority;
};

int tb_set_sched(tbthread_t thread, int policy, int priority)
{
  struct tb_sched_param p; p.sched_priority = priority;
  int ret = SYSCALL3(__NR_sched_setscheduler, thread->exit_futex, policy, &p);
  if(!ret)
    thread->sched_info = SCHED_INFO_PACK(policy, priority);
  return ret;
}

//------------------------------------------------------------------------------
// Set scheduling parameters
//------------------------------------------------------------------------------
int tbthread_setschedparam(tbthread_t thread, int policy, int priority)
{
  if(policy != SCHED_NORMAL && policy != SCHED_FIFO && policy != SCHED_RR)
    return -EINVAL;

  if(priority < 0 || priority > 99)
    return -EINVAL;

  int ret = 0;
  tbthread_mutex_lock(&desc_mutex);

  if(!list_find_elem(&used_desc, thread)) {
    ret = -ESRCH;
    goto exit;
  }

  ret = tb_set_sched(thread, policy, priority);

exit:
  tbthread_mutex_unlock(&desc_mutex);
  return ret;
}

//------------------------------------------------------------------------------
// Get scheduling parameters
//------------------------------------------------------------------------------
int tbthread_getschedparam(tbthread_t thread, int *policy, int *priority)
{
  int ret = 0;
  tbthread_mutex_lock(&desc_mutex);

  if(!list_find_elem(&used_desc, thread)) {
    ret = -ESRCH;
    goto exit;
  }

  uint16_t si = thread->sched_info;
  *policy = SCHED_INFO_POLICY(si);
  *priority = SCHED_INFO_PRIORITY(si);

exit:
  tbthread_mutex_unlock(&desc_mutex);
  return ret;
}

//------------------------------------------------------------------------------
// Set attribute scheduling policy
//------------------------------------------------------------------------------
int tbthread_attr_setschedpolicy(tbthread_attr_t *attr, int policy)
{
  if(policy != SCHED_NORMAL && policy != SCHED_FIFO && policy != SCHED_RR)
    return -EINVAL;
  attr->sched_policy = policy;
  return 0;
}

//------------------------------------------------------------------------------
// Get attribute scheduling priority
//------------------------------------------------------------------------------
int tbthread_attr_setschedpriority(tbthread_attr_t *attr, int priority)
{
  if(priority < 0 || priority > 99)
    return -EINVAL;
  attr->sched_priority = priority;
}

//------------------------------------------------------------------------------
// Inherit scheduler attributes
//------------------------------------------------------------------------------
int tbthread_attr_setinheritsched(tbthread_attr_t *attr, int inheritsched)
{
  if(inheritsched != TBTHREAD_INHERIT_SCHED &&
     inheritsched != TBTHREAD_EXPLICIT_SCHED)
    return -EINVAL;
  attr->sched_inherit = inheritsched;
  return 0;
}
