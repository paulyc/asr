#ifndef _TRACER_H
#define _TRACER_H

#if WINDOWS

#if MAC
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <stack>

#if WINDOWS
#include <windows.h>
#endif
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
	struct func_entry
	{
		const char *name;
		char data[5];
	};

	struct trace_entry
	{
		trace_entry() : _total_tm(0.0), _calls(0){}
		double _total_tm;
		int _calls;
		stdext::hash_map<void*, trace_entry*> _trace_map;
	};

	struct frame
	{
		frame(void *f, void *r, trace_entry *e=0) : 
			funcAddr(f),
			retAddr(r),
			ent(e)
		{
			QueryPerformanceCounter(&entryTime);
		}
		LARGE_INTEGER entryTime;
		void *funcAddr;
		void *retAddr;
		trace_entry *ent;
		bool operator<(const frame &rhs)
		{
			return this->funcAddr < rhs.funcAddr;
		}
	};

	
public:
	static void hook(void *f, const char *name=0)
	{
#if TRACE
		if (_func_map.find(f) != _func_map.end())
			return;
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		QueryPerformanceFrequency(&_perf_freq);
		DWORD pageSize = si.dwPageSize, oldProtect;
		void *baseAddr = (void*)((INT_PTR)f - (INT_PTR)f % pageSize);
		VirtualProtectEx(GetCurrentProcess(), baseAddr, pageSize,
			PAGE_EXECUTE_READWRITE, &oldProtect);
		
		_func_map[f].name = name;
		memcpy(_func_map[f].data, f, 5);
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
		memcpy(funcAddr, _func_map[funcAddr].data, 5);
		memcpy(_func_map[funcAddr].data, tmp, 5);

		pthread_t tid = pthread_self();
		trace_entry *e = 0;
		if (_thread_stacks[tid].size() > 0)
		{
			e = _thread_stacks[tid].top().ent;
			if (e->_trace_map.find(funcAddr) != e->_trace_map.end())
				e = e->_trace_map[funcAddr];
			else
			{
				e->_trace_map[funcAddr] = new trace_entry;
				e = e->_trace_map[funcAddr];
			}
		}
		else
		{
			if (_trace_map.find(funcAddr) != _trace_map.end())
				e = _trace_map[funcAddr];
			else
			{
				_trace_map[funcAddr] = new trace_entry;
				e = _trace_map[funcAddr];
			}
		}
		_thread_stacks[tid].push(frame(funcAddr, retAddr, e));
		return funcAddr;
	}

	static void* endTrace()
	{
		char tmp[5];
		pthread_t tid = pthread_self();
		frame &f = _thread_stacks[tid].top();
		void *retAddr = f.retAddr;
		void *funcAddr = f.funcAddr;
		LARGE_INTEGER tm;
		double tm_s;
		QueryPerformanceCounter(&tm);
		tm.QuadPart -= f.entryTime.QuadPart;
		tm_s = double(tm.QuadPart)/double(_perf_freq.QuadPart);
		f.ent->_total_tm += tm_s;
		f.ent->_calls++;
		_thread_stacks[tid].pop();

	//	_thread_stacks[tid]._Get_container().

	//	printf("call to %p took %f\n", funcAddr, tm_s);

		memcpy(tmp, funcAddr, 5);
		memcpy(funcAddr, _func_map[funcAddr].data, 5);
		memcpy(_func_map[funcAddr].data, tmp, 5);

		return retAddr;
	}

	static void printStack(void *f, trace_entry *e, int level=0)
	{
		for (int i = 0; i < level; ++i)
			printf("-");
		printf("%p total %f avg %f\n", f, e->_total_tm, e->_total_tm/e->_calls);
		for (stdext::hash_map<void*, trace_entry*>::iterator j = e->_trace_map.begin();
			j != e->_trace_map.end(); ++j)
		{
			printStack(j->first, j->second, level+1);
		}
	}

	static void printTrace()
	{
		for (stdext::hash_map<void*, trace_entry*>::iterator i = _trace_map.begin();
			i != _trace_map.end(); ++i)
		{
			printStack(i->first, i->second);
		}
	}
protected:
	static stdext::hash_map<void*, trace_entry*> _trace_map;
	static stdext::hash_map<pthread_t, std::stack<frame> > _thread_stacks;
	static stdext::hash_map<void*, func_entry> _func_map;
	static LARGE_INTEGER _perf_freq;

	
};
/*
#define QUOTE(str) #str
#define EXPAND_AND_QUOTE(str) QUOTE(str)

#define TRACE_FUNCTION(name) __asm { \
__asm push offset string QUOTE(name)  \
__asm mov eax, name \
__asm push eax \
__asm call Tracer::hook \
__asm add esp, 8 \
}
*/
    
#endif // WINDOWS
    
#endif // !defined(_TRACER_H)
