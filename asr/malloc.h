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

#ifndef _MALLOC_H
#define _MALLOC_H

#include <exception>
#include <queue>
#include <unordered_map>

#ifdef WIN32
#include <windows.h>
#endif

#define DEBUG_MALLOC 0

#if DEBUG_MALLOC
extern bool destructing_hash;

void dump_malloc();
void *operator new(size_t by, const char *f, int l);
void *operator new[](size_t by, const char *f, int l);
void operator delete(void *m);
void operator delete[](void *m);

#define new new(__FILE__, __LINE__)
#endif

template <int PageSize=0x1000>
struct Page {
	Page(void *ptr) : p(ptr) {}
	const static int size = PageSize;
	void *p;
};

template <int PageSize=0x1000>
class PageAllocator
{
public:
	static Page<PageSize> alloc() {
		return Page<PageSize>(new char[PageSize]);
	}
	static void free(Page<PageSize> p) {
		delete[] (char*)p.p;
	}
};

template <int PageSize=0x1000>
struct PageListNode {
	PageListNode(Page<PageSize> p, PageListNode<PageSize> *prev_ptr, PageListNode<PageSize> *next_ptr) : 
		page(p),
		prev(prev_ptr),
		next(next_ptr) {}
	Page<PageSize> page;
	PageListNode<PageSize> *prev, *next;
};

class StackAllocator
{
public:
	StackAllocator() {
		Page<> p = PageAllocator<>::alloc();
		//_first = &p;
		//_last = _first;
		_top = (char*)p.p;

	}
	void* alloc(int n) {
		if (n > Page<>::size - sizeof(PageListNode<>))
			throw std::runtime_error("StackAllocator size too large");
		if (_top + n >= (char*)_last->page.p + Page<>::size)
		{
			Page<> new_page = PageAllocator<>::alloc();
			char *top = (char*)new_page.p;
			PageListNode<> *new_node = (PageListNode<> *)top;
			top += sizeof(PageListNode<>);
			new_node->page = new_page;
			new_node->prev = this->_last;
			new_node->next = 0;
			this->_last->next = new_node;
			this->_last = new_node;
			this->_top = top;
		}
		void *ptr = (void*)_top;
		_top += n;
		return ptr;
	}
	static StackAllocator* create() {
		Page<> p = PageAllocator<>::alloc();
		StackAllocator *the_alloc = (StackAllocator*)p.p;
		char *top = (char*)p.p;
		top += sizeof(StackAllocator);
		PageListNode<> *first = (PageListNode<> *)top;
		first->page = p;
		first->prev = 0;
		first->next = 0;
		top += sizeof(PageListNode<>);
		the_alloc->_first = first;
		the_alloc->_last = first;
		the_alloc->_top = top;
		return the_alloc;
	}
private:
	PageListNode<> *_first;
	PageListNode<> *_last;
	char *_top;
};

template <int PageSize=0x1000>
class PageList
{
public:
	PageList() : _first(0), _last(0) {}
	void append(Page<PageSize> p)
	{
	}
private:
	PageListNode<PageSize> *_first;
	PageListNode<PageSize> *_last;
};


template <int N>
class Pooled_N_Allocator
{
public:
	Pooled_N_Allocator() :
	  _first(PageAllocator<>::alloc(), 0, 0)
	{
		_last = &_first;
		_top_ptr = _first.page.p;
	}
	void* alloc()
	{
		return 0;
	}
	static Pooled_N_Allocator<N>* create() {
		Page<> p = PageAllocator<>::alloc();
		Pooled_N_Allocator<N> *the_alloc = (Pooled_N_Allocator<N>*)p.p;
		char *top = (char*)p.p + sizeof(Pooled_N_Allocator<N>);
		PageListNode<> *first = (PageListNode<> *)top;
		first->page = p;
		first->prev = 0;
		first->next = 0;
		top += sizeof(PageListNode<>);
		the_alloc->_first = first;
		the_alloc->_last = first;
		the_alloc->_top = top;
		return the_alloc;
	}
private:
	PageListNode<> _first;
	PageListNode<> *_last;
	char *_top_ptr;
};

template <typename T>
class Pooled_T_Allocator
{
public:
private:
};

#include "lock.h"

template <typename T>
class T_allocator
{
protected:
	T_allocator(){}
	static std::queue<T*> _T_queue;
	static FastUserSpinLock _lock;
#if DEBUG_ALLOCATOR
	struct t_info
	{
		const char *file;
		int line;
	};
	typedef typename std::unordered_map<T*, typename T_allocator<T>::t_info> info_map_t;
	static info_map_t _info_map;
#endif
public:
	~T_allocator<T>() // no virtual destruct?
	{
		gc();
	}
	
	static void lock()
	{
		_lock.acquire();
	}
	
	static void unlock()
	{
		_lock.release();
	}

#if DEBUG_ALLOCATOR
	static T* alloc_from(const char *file, int line)
	{
		//printf("%s %d\n", file, line);
		_lock.acquire();
		T *t = alloc(false);
	 //	  printf("Alloc-ing %p from %s:%d\n", t, file, line);
		_info_map[t].file = file;
		_info_map[t].line = line;
		_lock.release();
		return t;
	}
	static void free_from(T *t, const char *file, int line)
	{
		//printf("%s %d\n", file, line);
		_lock.acquire();
	//	  printf("Free-ing %p from %s:%d\n", t, file, line);
		free(t, false);
		_lock.release();
	}
	static void add_ref_from(T *t, const char *file, int line)
	{
		_lock.acquire();
	 //	  printf("Add-ref-ing %p from %s:%d\n", t, file, line);
		t->add_ref();
		_lock.release();
	}
#endif

	static T* alloc(bool lock=true)
	{
		if (lock) _lock.acquire();
		if (_T_queue.empty())
		{
			T *t = new T;
			if (lock) _lock.release();
			return t;
		}
		else
		{
			T* t = _T_queue.front();
			_T_queue.pop();
			t->add_ref();
			if (lock) _lock.release();
			return t;
		}
	}
	
	static void add_refx(T* t)
	{
		_lock.acquire();
		t->add_ref();
		_lock.release();
	}

	static void free(T* t, bool lock=true)
	{
		if (lock) _lock.acquire();
		if (t && t->release() == 0)
		{
			_T_queue.push(t);
		}
		if (lock) _lock.release();
	}

	static void gc()
	{
		_lock.acquire();
		while (!_T_queue.empty())
		{
#if DEBUG_ALLOCATOR
			_info_map.erase(_T_queue.front());
#endif
	   //	  printf("Delete-ing %p\n", _T_queue.front());
			T* t = _T_queue.front();
			_T_queue.pop();
			_lock.release();
			delete t;
			sched_yield();
			_lock.acquire();
		}
		_lock.release();
	}

#if DEBUG_ALLOCATOR
	static void dump_leaks()
	{
		for (typename info_map_t::iterator i = _info_map.begin(); i != _info_map.end(); i++)
		{
			if (i->first->_refs > 0)
			{
				printf("%p allocated %s:%d count %d\n", i->first, 
					i->second.file, 
					i->second.line, 
					i->first->_refs);
			}
		}
		_info_map.clear();
	}
	static void print_info(T* t)
	{
		printf("%p allocated %s:%d count %d\n", t,
			   _info_map[t].file,
			   _info_map[t].line,
			   t->_refs);
	}
#else
	static void dump_leaks()
	{
	}
#endif
};
#if DEBUG_ALLOCATOR
template <typename T>
typename T_allocator<T>::info_map_t T_allocator<T>::_info_map;

#define alloc() alloc_from(__FILE__, __LINE__)
#define free(x) free_from((x), __FILE__, __LINE__)
#define add_refx(x) add_ref_from((x), __FILE__, __LINE__)
#endif

#if 0

template <typename T, int N>
class T_allocator_N : public T_allocator<T>
{
	~T_allocator_N<T, N>() // no virtual destruct?
	{
		while (!_T_queue.empty())
		{
			delete[] _T_queue.front();
			_T_queue.pop();
		}
	}

	static T* alloc()
	{
		if (_T_queue.empty())
			return new T[N];
		else
		{
			T* t = _T_queue.front();
			_T_queue.pop();
			return t;
		}
	}
};

template <typename T, int N>
class T_allocator_N_16ba : public T_allocator_N<T, N>
{
protected:
	static std::map<INT_PTR, INT_PTR> _T_align_map;
	static std::map<T*, size_t> _T_size_map;
public:
	struct alloc_info
	{
		T *ptr;
		size_t sz; // expect bytes, should always be multiple of 16
	};

	~T_allocator_N_16ba<T, N>() // no virtual destruct?
	{
		while (!_T_queue.empty())
		{
			INT_PTR del = (INT_PTR)_T_queue.front();
			delete[] (char*)_T_align_map[del];
			_T_info_map.erase((T*)del);
			_T_align_map.erase(del);
			_T_queue.pop();
		}
	}

	static T* alloc()
	{
		alloc_info info;
		alloc(&info);
		return info.ptr;
	}

	static void alloc(alloc_info *info)
	{
		if (_T_queue.empty())
		{
			size_t N_bytes = sizeof(T) * N + 16;
			char* buf = new char[N_bytes];
			INT_PTR buf_align = (INT_PTR)buf;
			while (buf_align & 0xF)
			{
				++buf_align;
				--N_bytes;
			}
			_T_align_map[buf_align] = (INT_PTR)buf;
			info->ptr = (T*)buf_align;
			info->sz = N_bytes;
			_T_size_map[info->ptr] = info->sz;
		}
		else
		{
			T* t = _T_queue.front();
			_T_queue.pop();
			info->ptr = t;
			info->sz = _T_size_map[t];
		}
		//invalid?assert(info->sz % 16 == 0);
	}
};

template <typename T, int N>
std::map<INT_PTR, INT_PTR> T_allocator_N_16ba<T, N>::_T_align_map;

template <typename T, int N>
std::map<T*, size_t> T_allocator_N_16ba<T, N>::_T_size_map;

#endif

template <typename T>
std::queue<T*> T_allocator<T>::_T_queue;

template <typename T>
FastUserSpinLock T_allocator<T>::_lock;

//template <typename T>
//std::queue<T*> T_allocator<T>::_T_queue;

//template <typename T>
//pthread_mutex_t T_allocator<T>::_lock = 0;

#endif // !defined(MALLOC_H)
