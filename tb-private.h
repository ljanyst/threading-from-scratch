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

void tb_tls_call_destructors();
