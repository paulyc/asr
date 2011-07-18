#ifndef _TRACER_H
#define _TRACER_H

#include <hash_map>
#include <stack>

#include <windows.h>
#include <pthread.h>

#include "config.h"

void stub();

inline bool operator<(const pthread_t &lhs, const pthread_t &rhs)
{
	return lhs.p < rhs.p;
}

inline size_t hash_value(const pthread_t& _Keyval)
{	// hash _Keyval to size_t value one-to-one
	return ((size_t)_Keyval.p ^ _HASH_SEED);
}

class Tracer
{
protected:
	struct frame
	{
		frame(void *f, void *r) : 
			funcAddr(f),
			retAddr(r)
		{
			QueryPerformanceCounter(&entryTime);
		}
		LARGE_INTEGER entryTime;
		void *funcAddr;
		void *retAddr;
		bool operator<(const frame &rhs)
		{
			return this->funcAddr < rhs.funcAddr;
		}
	};
public:
	static void hook(void *f)
	{
#if TRACE
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		QueryPerformanceFrequency(&_perf_freq);
		DWORD pageSize = si.dwPageSize, oldProtect;
		void *baseAddr = (void*)((INT_PTR)f - (INT_PTR)f % pageSize);
		VirtualProtectEx(GetCurrentProcess(), baseAddr, pageSize,
			PAGE_EXECUTE_READWRITE, &oldProtect);
		
		_func_map[f] = new char[5];
		memcpy(_func_map[f], f, 5);
		*(char*)f = 0xE8; //call
		INT_PTR ofs = (INT_PTR)stub - (INT_PTR)f - sizeof(void*) - 1;
		memcpy((void*)((INT_PTR)f+1), &ofs, sizeof(void*));
#endif
	}

	static void* beginTrace(void *_this, void *funcAddr, void *retAddr)
	{
		funcAddr = (void*)((INT_PTR)funcAddr-5);
		
		// restore function
		char tmp[5];
		memcpy(tmp, funcAddr, 5);
		memcpy(funcAddr, _func_map[funcAddr], 5);
		memcpy(_func_map[funcAddr], tmp, 5);

		pthread_t tid = pthread_self();
		_thread_stacks[tid].push(frame(funcAddr, retAddr));
		return funcAddr;
	}

	static void* endTrace()
	{
		char tmp[5];
		pthread_t tid = pthread_self();
		void *retAddr = _thread_stacks[tid].top().retAddr;
		void *funcAddr = _thread_stacks[tid].top().funcAddr;
		LARGE_INTEGER tm;
		QueryPerformanceCounter(&tm);
		tm.QuadPart -= _thread_stacks[tid].top().entryTime.QuadPart;
		_thread_stacks[tid].pop();

		printf("call to %p took %f\n", funcAddr, double(tm.QuadPart)/double(_perf_freq.QuadPart));

		memcpy(tmp, funcAddr, 5);
		memcpy(funcAddr, _func_map[funcAddr], 5);
		memcpy(_func_map[funcAddr], tmp, 5);

		return retAddr;
	}
protected:
	static stdext::hash_map<const char *, int> _map;
	static stdext::hash_map<pthread_t, std::stack<frame> > _thread_stacks;
	static stdext::hash_map<void*, char*> _func_map;
	static LARGE_INTEGER _perf_freq;
};

#endif // !defined(_TRACER_H)
