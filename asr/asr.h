#include <map>
#include <queue>

template <typename T> const char* gettype() { return "T"; }
#define stringify(x) #x
#define typable(type) template <> const char* gettype<type>() { return stringify(type); }
#define define_template_type_n(base,n,type) typedef base<n> type; typable(type)
#define define_template_type_T(base,T,type) typedef base<T> type; typable(type)
#define define_template_type_T_n(base,T,n,type) typedef base<T,n> type; typable(type)
#define define_template_type_T_n_T(base,T1,n,T2,type) typedef base<T,n,T2> type; typable(type)

typable(float)
typable(double)

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
	virtual T* next()
	{
		return T_allocator<T>::alloc();
	}
};

template <typename T>
class T_sink
{
public:
	virtual void next(T* t)
	{
		T_allocator<T>::free(t);
	}
};
