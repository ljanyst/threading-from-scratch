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

#pragma once

// the thread is detached
#define TB_DETACHED TBTHREAD_CREATE_DETACHED

// the thread is joinable
#define TB_JOINABLE TBTHREAD_CREATE_JOINABLE

// the thread is joinable and its status cannot be changed anymore
#define TB_JOINABLE_FIXED 2

#define TB_ONCE_NEW 0
#define TB_ONCE_IN_PROGRESS 1
#define TB_ONCE_DONE 2

#define TB_CANCEL_ENABLED  0x01
#define TB_CANCEL_DEFERRED 0x02
#define TB_CANCELING       0x04
#define TB_CANCELED        0x08

#define SIGCANCEL SIGRTMIN

#define TB_START_OK   0
#define TB_START_WAIT 1
#define TB_START_EXIT 2

#define SCHED_INFO_PACK(policy, priority) (((uint16_t)policy << 8) | priority)
#define SCHED_INFO_POLICY(info) (info >> 8)
#define SCHED_INFO_PRIORITY(info) (info & 0x00ff)

void tb_tls_call_destructors();
void tb_cancel_handler(int sig, siginfo_t *si, void *ctx);
void tb_call_cleanup_handlers();
void tb_clear_cleanup_handlers();

int tb_set_sched(tbthread_t thread, int policy, int priority);
int tb_compute_sched(tbthread_t thread);

void tb_protect_mutex_sched(tbthread_mutex_t *mutex);
void tb_protect_mutex_unsched(tbthread_mutex_t *mutex);
void tb_inherit_mutex_add(tbthread_mutex_t *mutex);
void tb_inherit_mutex_unsched(tbthread_mutex_t *mutex);
void tb_inherit_mutex_sched(tbthread_mutex_t *mutex, tbthread_t thread);

void tb_futex_lock(int *futex);
int tb_futex_trylock(int *futex);
void tb_futex_unlock(int *futex);

extern tbthread_mutex_t desc_mutex;
extern list_t used_desc;
extern int tb_pid;
