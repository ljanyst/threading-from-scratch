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

#include <string.h>

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
// Schedule a protected mutex
//------------------------------------------------------------------------------
static int mutex_prio_smaller(void *this, void *next)
{
  tbthread_mutex_t *t = this;
  tbthread_mutex_t *n = next;
  if(SCHED_INFO_PRIORITY(n->sched_info) < SCHED_INFO_PRIORITY(t->sched_info))
    return 1;
  return 0;
}

void tb_protect_mutex_sched(tbthread_mutex_t *mutex)
{
  tbthread_t owner = mutex->owner;
  tb_futex_lock(&owner->lock);

  list_t *node = malloc(sizeof(list_t));
  if(!node)
    goto exit;

  node->element = mutex;
  list_add_here(&owner->protect_mutexes, node, mutex_prio_smaller);

  if(node->prev == &owner->protect_mutexes)
    tb_compute_sched(owner);

exit:
  tb_futex_unlock(&owner->lock);
}

//------------------------------------------------------------------------------
// Un-schedule a protected mutex
//------------------------------------------------------------------------------
void tb_protect_mutex_unsched(tbthread_mutex_t *mutex)
{
  tbthread_t owner = mutex->owner;
  tb_futex_lock(&owner->lock);
  int reschedule = 0;
  list_t *node = list_find_elem(&owner->protect_mutexes, mutex);
  if(node) {
    if(node->prev == &owner->protect_mutexes)
      reschedule = 1;
    list_rm(node);
    free(node);
  }

  if(reschedule)
    tb_compute_sched(owner);
  tb_futex_unlock(&owner->lock);
}

//------------------------------------------------------------------------------
// Add an inherit mutex
//------------------------------------------------------------------------------
struct inh_mutex {
  tbthread_mutex_t *mutex;
  uint16_t sched_info;
};

void tb_inherit_mutex_add(tbthread_mutex_t *mutex)
{
  tbthread_t owner = mutex->owner;
  tb_futex_lock(&owner->lock);

  list_t *node = malloc(sizeof(list_t));
  if(!node)
    goto exit;

  struct inh_mutex *el = malloc(sizeof(struct inh_mutex));
  if(!el) {
    free(node);
    goto exit;
  }
  memset(el, 0, sizeof(struct inh_mutex));

  el->mutex = mutex;
  node->element = el;
  list_add(&owner->inherit_mutexes, node, 0);

exit:
  tb_futex_unlock(&owner->lock);
}

//------------------------------------------------------------------------------
// Un-schedule an inherit mutex
//------------------------------------------------------------------------------
static int mutex_equal(void *this, void *other)
{
  struct inh_mutex *o = other;
  return this == o->mutex;
}

void tb_inherit_mutex_unsched(tbthread_mutex_t *mutex)
{
  tbthread_t owner = mutex->owner;
  tb_futex_lock(&owner->lock);

  list_t *node = list_find_elem_func(&owner->inherit_mutexes, mutex,
    mutex_equal);

  if(!node)
    goto exit;

  struct inh_mutex *im = node->element;
  list_rm(node);
  free(node);

  if(im->sched_info == owner->sched_info)
    tb_compute_sched(owner);

  free(im);

exit:
  tb_futex_unlock(&owner->lock);
}

//------------------------------------------------------------------------------
// Schedule an inherit mutex
//------------------------------------------------------------------------------
void tb_inherit_mutex_sched(tbthread_mutex_t *mutex, tbthread_t thread)
{
  tb_futex_lock(&thread->lock);
  int th_sched_info = thread->sched_info;
  tb_futex_unlock(&thread->lock);

  tbthread_t owner = mutex->owner;
  tb_futex_lock(&owner->lock);

  list_t *node = list_find_elem_func(&owner->inherit_mutexes, mutex,
    mutex_equal);

  if(!node)
    goto exit;

  struct inh_mutex *im = node->element;
  int reschedule = 0;

  if(SCHED_INFO_PRIORITY(th_sched_info) > SCHED_INFO_PRIORITY(im->sched_info)) {
    im->sched_info = th_sched_info;
    reschedule = 1;
  }
  else
    if(SCHED_INFO_PRIORITY(th_sched_info) == SCHED_INFO_PRIORITY(im->sched_info)
       && SCHED_INFO_POLICY(im->sched_info) == SCHED_RR) {
      im->sched_info = th_sched_info;
      reschedule = 1;
    }

  if(reschedule)
    tb_compute_sched(owner);

exit:
  tb_futex_unlock(&owner->lock);
}

//------------------------------------------------------------------------------
// Compute scheduler
//------------------------------------------------------------------------------
int tb_compute_sched(tbthread_t thread)
{
  //----------------------------------------------------------------------------
  // Take the user set scheduler into account
  //----------------------------------------------------------------------------
  uint16_t sched_info = thread->user_sched_info;
  uint8_t policy = SCHED_INFO_POLICY(sched_info);
  uint8_t priority = SCHED_INFO_PRIORITY(sched_info);

  //----------------------------------------------------------------------------
  // Look at priority protect mutexes owned by the thread
  //----------------------------------------------------------------------------
  if(thread->protect_mutexes.next)
  {
    tbthread_mutex_t *protect_mutex = thread->protect_mutexes.next->element;
    uint16_t prot_sched_info = protect_mutex->sched_info;
    uint8_t prot_policy = SCHED_INFO_POLICY(prot_sched_info);
    uint8_t prot_priority = SCHED_INFO_PRIORITY(prot_sched_info);

    if(prot_priority > priority) {
      priority = prot_priority;
      policy = prot_policy;
    }
    else if(prot_priority == priority && policy == SCHED_RR)
      policy = prot_policy;
  }

  //----------------------------------------------------------------------------
  // Look at priority inherit mutexes owned by the thread
  //----------------------------------------------------------------------------
  if(thread->inherit_mutexes.next)
  {
    list_t *cursor = thread->inherit_mutexes.next;
    for( ; cursor; cursor = cursor->next) {
      struct inh_mutex *im = cursor->element;
      uint8_t im_policy = SCHED_INFO_POLICY(im->sched_info);
      uint8_t im_priority = SCHED_INFO_PRIORITY(im->sched_info);

      if(im_priority > priority) {
        priority = im_priority;
        policy = im_policy;
      }
      else if(im_priority == priority && policy == SCHED_RR)
        policy = im_policy;
    }
  }

  return tb_set_sched(thread, policy, priority);
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

  thread->user_sched_info = SCHED_INFO_PACK(policy, priority);
  tb_futex_lock(&thread->lock);
  ret = tb_compute_sched(thread);
  tb_futex_unlock(&thread->lock);

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
