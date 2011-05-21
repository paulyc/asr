#ifndef _ASR_H
#define _ASR_H

#include <map>
#include <queue>
#include <cassert>
#include <exception>

#include <fftw3.h>

#ifdef WIN32
#include <windows.h>
#endif

template <typename T> 
const char* gettype();

#define stringify(x) #x
#define typable(type) template <> const char* gettype<type>() { return stringify(type); }
#define define_template_type_n(base,n,type) typedef base<n> type; typable(type)
#define define_template_type_T(base,T,type) typedef base<T> type; typable(type)
#define define_template_type_T_n(base,T,n,type) typedef base<T,n> type; typable(type)
#define define_template_type_T_n_T(base,T1,n,T2,type) typedef base<T,n,T2> type; typable(type)

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

template <typename Sample_T>
class chunk_freq_domain;

template <typename Sample_T>
class chunk_time_domain;

template <typename Sample_T>
class chunk_base
{
public:
	chunk_base(Sample_T *input=0) :
		_input_data(input),
		_default_plan(0)
	{
	}

//protected:
	Sample_T *_data;
	Sample_T *_input_data;
	fftwf_plan _default_plan;
//	static bool _initd;
};

//template <typename Sample_T>
//bool chunk_base<Sample_T>::_initd = false;

template <typename Sample_T>
class chunk_base_domain : virtual public chunk_base<Sample_T>
{
public:
	chunk_base_domain() :
		chunk_base()
	{
		assert(_data);
	}

	chunk_base_domain(chunk_freq_domain<Sample_T> *chk) :
		chunk_base(chk->_data)
	{
		assert(_data);
		init_from_chunk(chk);
	}

	chunk_base_domain(chunk_time_domain<Sample_T> *chk) :
		chunk_base(chk->_data)
	{
		assert(_data);
		init_from_chunk(chk);
	}
	
	virtual void init_from_chunk(chunk_freq_domain<Sample_T> *chk) = 0;
	virtual void init_from_chunk(chunk_time_domain<Sample_T> *chk) = 0;

protected:
	void copy_from_chunk_transform(chunk_base_domain<Sample_T> *chk, int sign=FFTW_FORWARD)
	{
		assert(0);
		/*
		chunk_base_dim<Sample_T> *chk_dim = dynamic_cast<chunk_base_dim<Sample_T>*>(chk);
		fftwf_plan p = fftwf_plan_dft(chk_dim->dim(), chk_dim->sizes_as_array(), (Sample_T*)chk->_data, _data, sign, FFTW_ESTIMATE);
		fftwf_execute(p);
		*/
	}

	void copy_from_chunk_direct(chunk_base_domain<Sample_T> *chk)
	{
		chunk_base_dim<Sample_T> *chk_dim = dynamic_cast<chunk_base_dim<Sample_T>*>(chk);
		memcpy(_data, chk->_data, chk_dim->size_as_bytes());
	}
};

template <typename Sample_T>
class chunk_time_domain : public chunk_base_domain<Sample_T>
{
public:
	void init_from_chunk(chunk_freq_domain<Sample_T> *chk)
	{
		copy_from_chunk_transform(chk, FFTW_BACKWARD);
	}

	void init_from_chunk(chunk_time_domain<Sample_T> *chk)
	{
		copy_from_chunk_direct(chk);
	}
};

template <typename Sample_T>
class chunk_freq_domain : public chunk_base_domain<Sample_T>
{
public:
	void init_from_chunk(chunk_freq_domain<Sample_T> *chk)
	{
		copy_from_chunk_direct(chk);
	}

	void init_from_chunk(chunk_time_domain<Sample_T> *chk)
	{
		copy_from_chunk_transform(chk, FFTW_FORWARD);
	}
};

template <typename Sample_T>
class chunk_base_dim : virtual public chunk_base<Sample_T>
{
public:
	virtual ~chunk_base_dim()
	{
		fftwf_free(_data);
	}
	virtual int dim() = 0;
	virtual const int* sizes_as_array() = 0;
	virtual size_t size_as_bytes() = 0;
};

template <typename Sample_T, int n0>
class chunk_1d : public chunk_base_dim<Sample_T>
{
public:
	chunk_1d()
	{
		_data = (Sample_T*)fftwf_malloc(size_as_bytes());
	}
	int dim()
	{
		return 1;
	}
	const int* sizes_as_array()
	{
		static int n[] = {n0};
		return n;
	}
	size_t size_as_bytes()
	{
		return sizeof(Sample_T)*n0;
	}
	Sample_T& dereference(int m0)
	{
		return _data[m0];
	}
};

template <typename Sample_T, int n0, int n1>
class chunk_2d : public chunk_base_dim<Sample_T>
{
public:
	chunk_2d()
	{
		_data = (Sample_T*)fftwf_malloc(size_as_bytes());
	}
	int dim()
	{
		return 2;
	}
	const int* sizes_as_array()
	{
		static int n[] = {n0, n1};
		return n;
	}
	size_t size_as_bytes()
	{
		return sizeof(Sample_T)*n0*n1;
	}
	Sample_T& dereference(int m0, int m1)
	{
		return _data[m0*n1+m1];
	}
};

template <typename Sample_T, int n0, int n1, int n2>
class chunk_3d : public chunk_base_dim<Sample_T>
{
public:
	chunk_3d()
	{
		_data = (Sample_T*)fftwf_malloc(size_as_bytes());
	}
	int dim()
	{
		return 3;
	}
	const int* sizes_as_array()
	{
		static int n[] = {n0, n1, n2};
		return n;
	}
	size_t size_as_bytes()
	{
		return sizeof(Sample_T)*n0*n1*n2;
	}
	Sample_T& dereference(int m0, int m1, int m2)
	{
		return _data[m0*n0+m1*n1+m2];
	}
};

template <typename Sample_T, int n0>
class chunk_time_domain_1d : public chunk_time_domain<Sample_T>, public chunk_1d<Sample_T, n0>
{
public:
	chunk_time_domain_1d() :
		chunk_1d<Sample_T, n0>(),
		chunk_time_domain<Sample_T>()
	{
	}
};

template <typename Sample_T, int n0>
class chunk_freq_domain_1d : public chunk_freq_domain<Sample_T>, public chunk_1d<Sample_T, n0>
{
public:
	chunk_freq_domain_1d() :
		chunk_1d<Sample_T, n0>(),
		chunk_freq_domain<Sample_T>()
	{
	}
};

template <typename Sample_T, int n0, int n1>
class chunk_time_domain_2d : public chunk_time_domain<Sample_T>, public chunk_2d<Sample_T, n0, n1>
{
public:
	chunk_time_domain_2d() :
		chunk_2d<Sample_T, n0, n1>(),
		chunk_time_domain<Sample_T>()
	{
	}
};

template <typename Sample_T, int n0, int n1>
class chunk_freq_domain_2d : public chunk_freq_domain<Sample_T>, public chunk_2d<Sample_T, n0, n1>
{
public:
	chunk_freq_domain_2d() :
		chunk_2d<Sample_T, n0, n1>(),
		chunk_freq_domain<Sample_T>()
	{
	}
};

template <typename Sample_T, int n0, int n1, int n2>
class chunk_time_domain_3d : public chunk_time_domain<Sample_T>, public chunk_3d<Sample_T, n0, n1, n2>
{
public:
	chunk_time_domain_3d() :
		chunk_3d<Sample_T, n0, n1, n2>(),
		chunk_time_domain<Sample_T>()
	{
	}
};

template <typename Sample_T, int n0, int n1, int n2>
class chunk_freq_domain_3d : public chunk_freq_domain<Sample_T>, public chunk_3d<Sample_T, n0, n1, n2>
{
public:
	chunk_freq_domain_3d() :
		chunk_3d<Sample_T, n0, n1, n2>(),
		chunk_freq_domain<Sample_T>()
	{
	}
};

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
	static std::queue<T*> _T_queue;
public:
	~T_allocator<T>() // no virtual destruct?
	{
		while (!_T_queue.empty())
		{
			delete _T_queue.front();
			_T_queue.pop();
		}
	}

	static T* alloc()
	{
		if (_T_queue.empty())
			return new T;
		else
		{
			T* t = _T_queue.front();
			_T_queue.pop();
			return t;
		}
	}

	static void free(T* t)
	{
		if (t)
			_T_queue.push(t);
	}
};


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


template <typename T>
std::queue<T*> T_allocator<T>::_T_queue;

template <typename T, int N>
std::map<INT_PTR, INT_PTR> T_allocator_N_16ba<T, N>::_T_align_map;

template <typename T, int N>
std::map<T*, size_t> T_allocator_N_16ba<T, N>::_T_size_map;

template <typename T>
class T_source
{
public:
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
	virtual int length_samples()
	{
		return -1;
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

	virtual bool eof()
	{
		return _src->eof();
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

typedef float SamplePairf[2];
typedef double SamplePaird[2];

typedef int smp_ofs_t;

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
