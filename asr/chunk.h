#ifndef _CHUNK_H
#define _CHUNK_H

#include <fftw3.h>
#include <list>

struct countable
{
	countable() : _refs(1) {}
	int _refs;
	int add_ref() { return ++_refs; }
	int release() { return --_refs; }
#if DEBUG_ALLOCATOR
	struct owner
	{
		owner(const char *c, const char *f, int l) :
			_class(c),
			file(f),
			line(l) {}
		const char *_class;
		const char *file;
		int line;
	};
	std::list<owner> _owners;
#endif
};

#if DEBUG_ALLOCATOR
#define chown(objp, klass) ((objp)->_owners.push_back(owner((klass), __FILE__, __LINE__)))
#endif

template <typename Sample_T>
class chunk_freq_domain;

template <typename Sample_T>
class chunk_time_domain;

template <typename Sample_T>
class chunk_base : public countable
{
public:
	chunk_base(Sample_T *input=0) :
	    countable(),
		_input_data(input)
	{
	}

	typedef Sample_T sample_t;

//protected:
	Sample_T *_data;
	Sample_T *_input_data;
//	fftwf_plan _default_plan;
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
		//	fftwf_execute_dft_r2c(
     //     const fftw_plan p,
     //     double *in, fftw_complex *out);
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
		_data = (Sample_T*)fftwf_malloc(size_as_bytes()+n0*sizeof(Sample_T)); // padding for inplace DFT
		if (!_plan)
		{
			_plan = fftwf_plan_dft_r2c_2d(4096, 2,
				(float*)_data, (fftwf_complex*)_data, FFTW_MEASURE);
			_iplan = fftwf_plan_dft_c2r_2d(4096, 2,
				(fftwf_complex*)_data, (float*)_data, FFTW_MEASURE);
		}
		_domain = time;
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

	enum { time, freq } _domain;

	void inplace_dft()
	{
		fftwf_execute_dft_r2c(_plan, (float*)_data, (fftwf_complex*)_data);
		_domain = freq;
	}

	void inplace_idft()
	{
		fftwf_execute_dft_c2r(_iplan, (fftwf_complex*)_data, (float*)_data);
		_domain = time;
	}

	static const int chunk_size = n0;
	static const int sample_size = sizeof(Sample_T);
	static fftwf_plan _plan;
	static fftwf_plan _iplan;
};

template <typename Sample_T, int n0>
fftwf_plan chunk_1d<Sample_T, n0>::_plan = 0;

template <typename Sample_T, int n0>
fftwf_plan chunk_1d<Sample_T, n0>::_iplan = 0;

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

	static const int chunk_size = n0*n1;
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
	static const int chunk_size = n0*n1*n2;
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

	template <typename Src_T>
	void load_from(Src_T *src);
};

template <int n0>
class chunk_time_domain_1d<SamplePairf, n0> : public chunk_time_domain<SamplePairf>, public chunk_1d<SamplePairf, n0>
{
public:
	template <typename Src_T>
	void load_from(Src_T *buf)
	{
		for (int r=0; r < chunk_size; ++r)
		{
			PairFromT<SamplePairf, Src_T>(_data[r], buf[r*2], buf[r*2+1]);
		}
	}
#if 0
	template <unsigned int bytes_per_sample>
	void load_from_buffer(char *buf)
	{
		char *b = buf;
		const unsigned int mask = (unsigned int)0xFFFFFFFF >> (4 - bytes_per_sample)*8;
		for (int r=0; r < chunk_size; ++r)
		{
			PairFromT<SamplePairf, int>(_data[r], *((int*)b) & mask, *((int*)(b+_bytes_per_sample)) & mask);
			b += 2*bytes_per_sample;
		}
	}
#endif
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

	static fftwf_plan _plan;
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

#endif // !defined(_CHUNK_H)
