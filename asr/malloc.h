#ifndef _MALLOC_H
#define _MALLOC_H

#include <queue>

#if DEBUG_MALLOC
void dump_malloc();
void *operator new(size_t by, const char *f, int l);
void *operator new[](size_t by, const char *f, int l);
void operator delete(void *m);
void operator delete[](void *m);

#define new new(__FILE__, __LINE__)
#endif

template <typename T>
class T_allocator
{
protected:
	T_allocator(){}
	static void init()
	{
		pthread_mutex_init(&_lock, 0);
	}
	static std::queue<T*> _T_queue;
	static pthread_mutex_t _lock;
	static pthread_once_t once_control; 
#if DEBUG_ALLOCATOR
	struct t_info
	{
		const char *file;
		int line;
	};
	static stdext::hash_map<T*, t_info> _info_map;
#endif
public:
	~T_allocator<T>() // no virtual destruct?
	{
		gc();
		pthread_mutex_destroy(&_lock);
	}

	static T* alloc_from(const char *file, int line)
	{
		//printf("%s %d\n", file, line);
		pthread_mutex_lock(&_lock);
		T *t = alloc(false);
		_info_map[t].file = file;
		_info_map[t].line = line;
		pthread_mutex_unlock(&_lock);
		return t;
	}

	static T* alloc(bool lock=true)
	{
		pthread_once(&once_control, init);
		if (lock) pthread_mutex_lock(&_lock);
		if (_T_queue.empty())
		{
			T *t = new T;
			if (lock) pthread_mutex_unlock(&_lock);
			return t;
		}
		else
		{
			T* t = _T_queue.front();
			_T_queue.pop();
			t->add_ref();
			if (lock) pthread_mutex_unlock(&_lock);
			return t;
		}
	}

	static void free(T* t)
	{
		if (t && t->release() == 0)
		{
			pthread_mutex_lock(&_lock);
			_T_queue.push(t);
			pthread_mutex_unlock(&_lock);
		}
	}

	static void gc()
	{
		pthread_mutex_lock(&_lock);
		while (!_T_queue.empty())
		{
#if DEBUG_ALLOCATOR
			_info_map.erase(_T_queue.front());
#endif
			delete _T_queue.front();
			_T_queue.pop();
		}
		pthread_mutex_unlock(&_lock);
	}

#if DEBUG_ALLOCATOR
	static void dump_leaks()
	{
		for (stdext::hash_map<T*, t_info>::iterator i = _info_map.begin();
			i != _info_map.end(); ++i)
		{
			if (i->first->_refs > 0)
			{
				printf("%p allocated %s:%d count %d\n", i->first, 
					i->second.file, 
					i->second.line, 
					i->first->_refs);
			}
		}
	}
#else
	static void dump_leaks()
	{
	}
#endif
};
#if DEBUG_ALLOCATOR
template <typename T>
stdext::hash_map<T*, typename T_allocator<T>::t_info> T_allocator<T>::_info_map;

#define alloc() alloc_from(__FILE__, __LINE__)
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
pthread_mutex_t T_allocator<T>::_lock;

template <typename T>
pthread_once_t T_allocator<T>::once_control = PTHREAD_ONCE_INIT;

//template <typename T>
//std::queue<T*> T_allocator<T>::_T_queue;

//template <typename T>
//pthread_mutex_t T_allocator<T>::_lock = 0;

#if DEBUG_MALLOC
struct alloc_info
{
	alloc_info(){}
	alloc_info(size_t b, const char *f, int l) : 
		bytes(b), file(f), line(l){}
	size_t bytes;
	const char *file;
	int line;
};

bool destructing_hash = false;

class my_hash_map : public stdext::hash_map<void*, alloc_info>
{
public:
	~my_hash_map()
	{
		destructing_hash = true;
	}
};

my_hash_map _malloc_map;
pthread_mutex_t malloc_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
pthread_once_t once_control = PTHREAD_ONCE_INIT; 
pthread_mutexattr_t malloc_attr;

void init_lock()
{
	pthread_mutexattr_init(&malloc_attr);
	pthread_mutexattr_settype(&malloc_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&malloc_lock, &malloc_attr);
}

void *operator new(size_t by, const char *f, int l)
{
	pthread_once(&once_control, init_lock);
	pthread_mutex_lock(&malloc_lock);
	void *m = malloc(by);
	_malloc_map[m].bytes = by;
	_malloc_map[m].file = f;
	_malloc_map[m].line = l;
	pthread_mutex_unlock(&malloc_lock);
	return m;
}

void *operator new[](size_t by, const char *f, int l)
{
	pthread_once(&once_control, init_lock);
	pthread_mutex_lock(&malloc_lock);
	void *m = malloc(by);
	_malloc_map[m].bytes = by;
	_malloc_map[m].file = f;
	_malloc_map[m].line = l;
	pthread_mutex_unlock(&malloc_lock);
	return m;
}

void operator delete(void *m)
{
	pthread_mutex_lock(&malloc_lock);
	if (!destructing_hash)
		_malloc_map.erase(m);
	free(m);
	pthread_mutex_unlock(&malloc_lock);
}

void operator delete[](void *m)
{
	pthread_mutex_lock(&malloc_lock);
	if (!destructing_hash)
		_malloc_map.erase(m);
	free(m);
	pthread_mutex_unlock(&malloc_lock);
}

void dump_malloc()
{
	for (stdext::hash_map<void*, alloc_info>::iterator i = _malloc_map.begin();
		i != _malloc_map.end(); ++i)
	{
		printf("%p allocated %s:%d bytes %d\n", i->first, 
			i->second.file, 
			i->second.line,
			i->second.bytes);
	}
}
#endif

#endif // !defined(MALLOC_H)
