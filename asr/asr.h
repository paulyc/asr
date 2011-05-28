#ifndef _ASR_H
#define _ASR_H

#include <map>
#include <queue>
#include <cassert>
#include <exception>
#define _USE_MATH_DEFINES
#include <cmath>

#include <fftw3.h>
#include <pthread.h>

#ifdef WIN32
#include <windows.h>
#endif

typedef float SamplePairf[2];
typedef double SamplePaird[2];

typedef int smp_ofs_t;

template <typename T> 
const char* gettype();

#define stringify(x) #x
#define typable(type) template <> const char* gettype<type>() { return stringify(type); }
#define define_template_type_n(base,n,type) typedef base<n> type; typable(type)
#define define_template_type_T(base,T,type) typedef base<T> type; typable(type)
#define define_template_type_T_n(base,T,n,type) typedef base<T,n> type; typable(type)
#define define_template_type_T_n_T(base,T1,n,T2,type) typedef base<T,n,T2> type; typable(type)

/*
template <typename T, int data_size_n, int pad_size_bytes>
struct chunk_T_T_var_size
{
	T _data[data_size_n];
	int _data_stored;
	char _padding[pad_size_bytes];
};

template <typename T, int data_size_n>
struct chunk_T_T_size
{
	T _data[data_size_n];
};

*/

#include "chunk.h"

/*
struct chunk_interleave_base
{
};

struct chunk_interleaved : public chunk_interleave_base
{
};

struct chunk_noninterleaved : public chunk_interleave_base
{
};

template <typename T, int data_size_n>
struct chunk_T_T_size_noninterleaved : public chunk_T_T_size<T, data_size_n>
{
};
*/

template <typename T>
class T_allocator
{
protected:
	T_allocator()
	{
		pthread_mutex_init(&_lock, 0);
	}
	static T_allocator* _instance;
	std::queue<T*> _T_queue;
	pthread_mutex_t _lock;
public:
	static T_allocator* get()
	{
		if (!_instance)
			_instance = new T_allocator;
		return _instance;
	}
	~T_allocator<T>() // no virtual destruct?
	{
		pthread_mutex_lock(&_lock);
		while (!_T_queue.empty())
		{
			delete _T_queue.front();
			_T_queue.pop();
		}
		pthread_mutex_unlock(&_lock);
		pthread_mutex_destroy(&_lock);
	}

	static T* alloc()
	{
		T_allocator<T> *i = get();
		pthread_mutex_lock(&i->_lock);
		if (i->_T_queue.empty())
		{
			T *t = new T;
			pthread_mutex_unlock(&i->_lock);
			return t;
		}
		else
		{
			T* t = i->_T_queue.front();
			i->_T_queue.pop();
			pthread_mutex_unlock(&i->_lock);
			return t;
		}
	}

	static void free(T* t)
	{
		if (t)
		{
			T_allocator<T> *i = get();
			pthread_mutex_lock(&i->_lock);
			i->_T_queue.push(t);
			pthread_mutex_unlock(&i->_lock);
		}
	}
};
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
T_allocator<T>* T_allocator<T>::_instance = 0;

//template <typename T>
//std::queue<T*> T_allocator<T>::_T_queue;

//template <typename T>
//pthread_mutex_t T_allocator<T>::_lock = 0;



template <typename T>
class T_source
{
public:
	typedef T type;
	virtual ~T_source()
	{
	}
	virtual T* next()
	{
		return T_allocator<T>::alloc();
	}
	virtual void seek_chk(int chk_ofs)
	{
		throw std::exception("not implemented");
	}
	virtual void seek_smp(int smp_ofs)
	{
		throw std::exception("not implemented");
	}
	virtual bool eof()
	{
		return false;
	}
	struct pos_info
	{
		pos_info() : samples(-1), chunks(-1), smp_ofs_in_chk(-1), time(HUGE_VAL) {}
		smp_ofs_t samples;
		int chunks;
		int smp_ofs_in_chk;
		double time;
	} _len;

	virtual const pos_info& len()
	{
		return _len;
	}

	virtual float maxval()
	{
		return 1.0f;
	}
};

template <typename T>
class T_sink
{
protected:
	T_source<T> *_src;
public:
	T_sink(T_source<T> *src=0) :
	  _src(src)
	{
	}

	virtual ~T_sink()
	{
	}

	virtual void process()
	{
		T_allocator<T>::free(_src->next());
	}
};

template <typename T>
class T_sink_source : public T_source<T>, public T_sink<T>
{
public:
	T_sink_source(T_source<T> *src=0) :
	  T_sink(src)
	{
	}
	virtual bool eof()
	{
		return _src->eof();
	}
	virtual int length_samples()
	{
		return _src->length_samples();
	}
};

template <typename T>
class zero_source : public T_source<T>
{
protected:
	zero_source() {}
	static zero_source *_the_src;
public:
	static zero_source* get()
	{
		if (!_the_src)
			_the_src = new zero_source;
		return _the_src;
	}

	T* next()
	{
		T* chk = T_allocator<T>::alloc();
		for (typename T::sample_t *smp = chk->_data, *end=smp+T::chunk_size;
			smp != end; ++smp)
		{
			Zero<typename T::sample_t>::set(*smp);
		}
		return chk;
	}
	typedef typename T chunk_t;
};

template <typename T>
zero_source<T>* zero_source<T>::_the_src = 0;

/*
SamplePairf& operator=(SamplePairf &lhs, float rhs)
{
	lhs[0] = rhs;
	lhs[1] = rhs;
	return lhs;
}

SamplePairf operator*(SamplePairf &lhs, float rhs)
{
	SamplePairf val;
	val[0] = lhs[0] * rhs;
	val[1] = lhs[1] * rhs;
	return val;
}

SamplePairf& operator*=(SamplePairf &lhs, float rhs)
{
	lhs[0] *= rhs;
	lhs[1] *= rhs;
	return lhs;
}
*/

#endif // !defined(_ASR_H)
