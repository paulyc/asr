// ASR - Digital Signal Processor
// Copyright (C) 2002-2013	Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.	 If not, see <http://www.gnu.org/licenses/>.

#include "tracer.h"

stdext::hash_map<void*, Tracer::trace_entry*> Tracer::_trace_map;
stdext::hash_map<pthread_t, std::stack<Tracer::frame> > Tracer::_thread_stacks;
stdext::hash_map<void*, Tracer::func_entry> Tracer::_func_map;
LARGE_INTEGER Tracer::_perf_freq;

__declspec(naked) void stub()
{
	__asm
	{
	;	mov eax, dword ptr[esp+4]
	;	push eax ; retAddr
	;	mov eax, dword ptr[esp+4]
	;	sub eax, 5
	;	push eax ; funcAddr
		push ecx ; save this for __thiscall
		call Tracer::beginTrace
		mov ecx, dword ptr[esp]
		add esp, 12
		call eax ; call original function restored
		push eax ; save return value
		call Tracer::endTrace ; restore function
		mov ecx, eax ; save stub return address
		pop eax ; restore return value
		push ecx ; push return address
		ret
	}
}
