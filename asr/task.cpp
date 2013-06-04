// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "task.h"

void ForkJoinTask::fork(ForkJoinTask *task)
{
    _executor->_mgr->invoke(task);
}

void ForkJoinTask::join()
{
check_done:
    asm ("xorl %eax, %eax");
    asm ("lock cmpxchg %eax, _finished");
    asm ("jnz not_done");
    asm ("ret");
not_done:
    //_executor->exec_next();
    asm ("jmp check_done");
}