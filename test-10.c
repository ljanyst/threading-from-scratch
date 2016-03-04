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

#define THREADS 5

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
// Print policy
//------------------------------------------------------------------------------
void print_sched()
{
  tbthread_t self = tbthread_self();
  int policy, priority;
  tbthread_getschedparam(self, &policy, &priority);
  tbprint("[thread 0x%llx] Policy: %s, priority: %d\n", self, strpolicy(policy),
    priority);
}

void print_sched_n(int n)
{
  for(int i = 0; i < n; ++i) {
    print_sched();
    tbsleep(1);
  }
}

//------------------------------------------------------------------------------
// Thread function - PRIO PROTECT
//------------------------------------------------------------------------------
struct tharg {
  int num;
  int before;
  int inside;
  int after;
  tbthread_mutex_t *m[3];
};
void *thread_func_protect(void *arg)
{
  tbthread_t self = tbthread_self();
  struct tharg *a = arg;
  int priority;
  tbprint("[thread 0x%llx] Starting\n", self);
  print_sched();
  for(int i = 0; i < 3; ++i) {
    tbthread_mutex_lock(a->m[i]);
    tbthread_mutex_getprioceiling(a->m[i], &priority);
    tbprint("[thread 0x%llx] Mutex #%d (priority %d) locked\n", self, i,
      priority);
    print_sched();
  }
  tbsleep(1);
  for(int i = 2; i >= 0; --i) {
    tbthread_mutex_unlock(a->m[i]);
    tbprint("[thread 0x%llx] Mutex #%d unlocked\n", self, i);
    print_sched();
  }

  tbprint("[thread 0x%llx] Done\n", self);
  print_sched();
  return 0;
}

//------------------------------------------------------------------------------
// Thread function - PRIO INHERIT
//------------------------------------------------------------------------------
void *thread_func_inherit(void *arg)
{
  tbthread_t self = tbthread_self();
  struct tharg *a = arg;
  tbprint("[thread 0x%llx] Starting #%d\n", self, a->num);
  print_sched_n(a->before);
  tbthread_mutex_lock(a->m[0]);
  tbprint("[thread 0x%llx] Mutex locked\n", self);
  print_sched_n(a->inside);
  tbthread_mutex_unlock(a->m[0]);
  tbprint("[thread 0x%llx] Mutex unlocked\n", self);
  print_sched_n(a->after);
  tbprint("[thread 0x%llx] Done\n", self);
  return 0;
}

void *thread_func_inherit_0(void *arg)
{
  tbthread_t self = tbthread_self();
  struct tharg *a = arg;
  tbprint("[thread 0x%llx] Starting #0\n", self);
  print_sched_n(1);

  tbthread_mutex_lock(a->m[0]);
  tbprint("[thread 0x%llx] Mutex 0 locked\n", self);
  print_sched_n(1);

  tbthread_mutex_lock(a->m[1]);
  tbprint("[thread 0x%llx] Mutex 1 locked\n", self);
  print_sched_n(4);

  tbthread_mutex_lock(a->m[2]);
  tbprint("[thread 0x%llx] Mutex 2 locked\n", self);
  print_sched_n(1);

  tbthread_mutex_unlock(a->m[2]);
  tbprint("[thread 0x%llx] Mutex 2 unlocked\n", self);
  print_sched_n(1);

  tbthread_mutex_unlock(a->m[1]);
  tbprint("[thread 0x%llx] Mutex 1 unlocked\n", self);
  print_sched_n(1);

  tbthread_mutex_unlock(a->m[0]);
  tbprint("[thread 0x%llx] Mutex 0 unlocked\n", self);
  print_sched_n(1);

  tbprint("[thread 0x%llx] Done\n", self);
  return 0;
}

//------------------------------------------------------------------------------
// Run the threads
//------------------------------------------------------------------------------
int run(tbthread_attr_t attr[THREADS], struct tharg arg[THREADS],
  void *(*func[THREADS])(void *))
{
  tbthread_t thread[THREADS];
  int st = 0;
  for(int i = 0; i < THREADS; ++i) {
    st = tbthread_create(&thread[i], &attr[i], func[i], &arg[i]);
    if(st != 0) {
      tbprint("Failed to spawn thread %d: %s\n", i, tbstrerror(-st));
      return st;
    }
  }

  tbprint("[thread main] Threads spawned successfully\n");

  for(int i = 0; i < THREADS; ++i) {
    st = tbthread_join(thread[i], 0);
    if(st != 0) {
      tbprint("Failed to join thread %d: %s\n", i, tbstrerror(-st));
      return st;
    }
  }

  tbprint("[thread main] Threads joined\n");
  return 0;
}
//------------------------------------------------------------------------------
// Start the show
//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  tbthread_init();
  tbthread_attr_t  attr[THREADS];
  struct tharg     arg[THREADS];
  int              st = 0;

  //----------------------------------------------------------------------------
  // Warn about not being root
  //----------------------------------------------------------------------------
  if(SYSCALL0(__NR_getuid) != 0)
    tbprint("[!!!] You should run this test as root\n");

  //----------------------------------------------------------------------------
  // PRIO PROTECT
  //----------------------------------------------------------------------------
  for(int i = 0; i < THREADS; ++i)
    tbthread_attr_init(&attr[i]);
  memset(arg, 0, THREADS*sizeof(struct tharg));

  tbthread_mutex_t m_prot[THREADS+2];
  tbthread_mutexattr_t m_prot_attr[THREADS+2];
  uint32_t seed = tbtime();
  for(int i = 0; i < THREADS+2; ++i) {
    tbthread_mutexattr_init(&m_prot_attr[i]);
    tbthread_mutexattr_setprotocol(&m_prot_attr[i], TBTHREAD_PRIO_PROTECT);
    tbthread_mutexattr_setprioceiling(&m_prot_attr[i], 1+(tbrandom(&seed)%99));
    tbthread_mutex_init(&m_prot[i], &m_prot_attr[i]);
  }

  for(int i = 0; i < THREADS; ++i)
    for(int j = i; j < i+3; ++j)
      arg[i].m[j-i] = &m_prot[j];

  void *(*m_prot_func[THREADS])(void *);
  for(int i = 0; i < THREADS; ++i)
    m_prot_func[i] = thread_func_protect;

  tbprint("Testing PRIO_PROTECT mutexes\n");
  if((st = run(attr, arg, m_prot_func)))
    goto exit;
  tbprint("---\n");

  //----------------------------------------------------------------------------
  // PRIO INHERIT
  //----------------------------------------------------------------------------
  memset(arg, 0, THREADS*sizeof(struct tharg));
  for(int i = 0; i < THREADS; ++i) {
    tbthread_attr_init(&attr[i]);
    tbthread_attr_setinheritsched(&attr[i], TBTHREAD_EXPLICIT_SCHED);
    arg[i].num = i;
  }
  tbthread_attr_setschedpolicy(&attr[2], SCHED_FIFO);
  tbthread_attr_setschedpolicy(&attr[3], SCHED_RR);
  tbthread_attr_setschedpolicy(&attr[4], SCHED_RR);
  tbthread_attr_setschedpriority(&attr[2], 5);
  tbthread_attr_setschedpriority(&attr[3], 6);
  tbthread_attr_setschedpriority(&attr[4], 7);

  tbthread_mutex_t m_inh[3];
  tbthread_mutexattr_t m_inh_attr[3];
  for(int i = 0; i < 3; ++i) {
    tbthread_mutexattr_init(&m_inh_attr[i]);
    tbthread_mutexattr_setprotocol(&m_inh_attr[i], TBTHREAD_PRIO_INHERIT);
    tbthread_mutex_init(&m_inh[i], &m_inh_attr[i]);
  }

  void *(*m_inh_func[THREADS])(void *);
  m_inh_func[0] = thread_func_inherit_0;
  for(int i = 1; i < THREADS; ++i)
    m_inh_func[i] = thread_func_inherit;

  arg[0].m[0] = &m_inh[0];
  arg[0].m[1] = &m_inh[1];
  arg[0].m[2] = &m_inh[2];

  arg[1].m[0] = &m_inh[2]; arg[1].before = 1; arg[1].inside = 10; arg[1].after = 1;
  arg[2].m[0] = &m_inh[0]; arg[2].before = 4; arg[2].inside = 2; arg[2].after = 1;
  arg[3].m[0] = &m_inh[1]; arg[3].before = 4; arg[3].inside = 2; arg[3].after = 1;
  arg[4].m[0] = &m_inh[1]; arg[4].before = 4; arg[4].inside = 2; arg[4].after = 1;

  tbprint("Testing PRIO_INHERIT mutexes\n");
  if((st = run(attr, arg, m_inh_func)))
    goto exit;
  tbprint("---\n");


exit:
  tbthread_finit();
  return st;
};
