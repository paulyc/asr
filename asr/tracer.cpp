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
