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

#ifndef _CHUNK_H
#define _CHUNK_H

#include <fftw3.h>
#include <list>
#include <cassert>
#include "type.h"
#include "util.h"

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
		_data(input)
	{
	}

	virtual ~chunk_base()
	{
		fftwf_free(_data);
	}

	typedef Sample_T sample_t;

//protected:
	
	Sample_T *_data;
//	fftwf_plan _default_plan;
//	static bool _initd;
};

template <typename Sample_T>
class chunk_base_dim : virtual public chunk_base<Sample_T>
{
public:
	virtual int dim() = 0;
	virtual const int* sizes_as_array() = 0;
	virtual size_t size_as_bytes() = 0;
};

//template <typename Sample_T>
//bool chunk_base<Sample_T>::_initd = false;

template <typename Sample_T>
class chunk_base_domain : virtual public chunk_base<Sample_T>
{
public:
	chunk_base_domain() :
		chunk_base<Sample_T>(0)
	{
	}

	chunk_base_domain(chunk_freq_domain<Sample_T> *chk) :
		chunk_base<Sample_T>(chk->_data)
	{
		init_from_chunk(chk);
		assert(chk->_data);
	}

	chunk_base_domain(chunk_time_domain<Sample_T> *chk) :
		chunk_base<Sample_T>(chk->_data)
	{
		init_from_chunk(chk);
		assert(chk->_data);
	}
	
	virtual void init_from_chunk(chunk_freq_domain<Sample_T> *chk) = 0;
	virtual void init_from_chunk(chunk_time_domain<Sample_T> *chk) = 0;

protected:
	void copy_from_chunk_transform(chunk_base_domain<Sample_T> *chk, int sign=FFTW_FORWARD)
	{
		//	fftwf_execute_dft_r2c(
	 //		const fftw_plan p,
	 //		double *in, fftw_complex *out);
	}

	void copy_from_chunk_direct(chunk_base_domain<Sample_T> *chk)
	{
		chunk_base_dim<Sample_T> *chk_dim = dynamic_cast<chunk_base_dim<Sample_T>*>(chk);
		memcpy(this->_data, chk->_data, chk_dim->size_as_bytes());
	}
};

template <typename Sample_T>
class chunk_time_domain : public chunk_base_domain<Sample_T>
{
public:
	void init_from_chunk(chunk_freq_domain<Sample_T> *chk)
	{
		this->copy_from_chunk_transform(chk, FFTW_BACKWARD);
	}

	void init_from_chunk(chunk_time_domain<Sample_T> *chk)
	{
		this->copy_from_chunk_direct(chk);
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

template <typename Sample_T, int n0>
class chunk_1d : public chunk_base_dim<Sample_T>
{
public:
	chunk_1d()
	{
		this->_data = (Sample_T*)fftwf_malloc(size_as_bytes()+n0*sizeof(Sample_T)); // padding for inplace DFT
		if (!_plan)
		{
			_plan = fftwf_plan_dft_r2c_2d(n0, 2,
				(float*)this->_data, (fftwf_complex*)this->_data, FFTW_MEASURE);
			_iplan = fftwf_plan_dft_c2r_2d(n0, 2,
				(fftwf_complex*)this->_data, (float*)this->_data, FFTW_MEASURE);
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
		return this->_data[m0];
	}

	enum { time, freq } _domain;

	void inplace_dft()
	{
		fftwf_execute_dft_r2c(_plan, (float*)this->_data, (fftwf_complex*)this->_data);
		_domain = freq;
	}

	void inplace_idft()
	{
		fftwf_execute_dft_c2r(_iplan, (fftwf_complex*)this->_data, (float*)this->_data);
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
		this->_data = (Sample_T*)fftwf_malloc(size_as_bytes());
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
		return this->_data[m0*n1+m1];
	}

	static const int chunk_size = n0*n1;
};

template <typename Sample_T, int n0, int n1, int n2>
class chunk_3d : public chunk_base_dim<Sample_T>
{
public:
	chunk_3d()
	{
		this->_data = (Sample_T*)fftwf_malloc(size_as_bytes());
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
		return this->_data[m0*n0+m1*n1+m2];
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

	// assuming interleaved stereo samples
	template <typename Src_T>
	void load_from(Src_T *buf)
	{
		for (int r=0; r < this->chunk_size; ++r)
		{
			PairFromT(this->_data[r], buf[r*2], buf[r*2+1]);
		}
	}

	// assuming interleaved stereo samples
	// doesnt care about larger range of negative integers
	void load_from_bytes(uint8_t *buf, int bytes_per_sample)
	{
		double multiplier = 1.0 / (0x7FFFFFFF >> (8*(4-bytes_per_sample)));
		for (int r=0; r < this->chunk_size; ++r)
		{
			int32_t val;
			switch (bytes_per_sample)
			{
			case 1:
				val = *buf++;
				break;
			case 2:
				val = *(int16_t*)buf;
				buf += 2;
				break;
			case 3:
				val = (buf[2] << 16) | (buf[1] << 8) | buf[0];
				if (buf[2] & 0x80)
					val |= 0xFF000000;
				buf += 3;
				break;
			case 4:
				val = *(int32_t*)buf;
				buf += 4;
				break;
			}
			this->_data[r][0] = float32_t(val * multiplier);
			switch (bytes_per_sample)
			{
			case 1:
				val = *buf++;
				break;
			case 2:
				val = *(int16_t*)buf;
				buf += 2;
				break;
			case 3:
				val = (buf[2] << 16) | (buf[1] << 8) | buf[0];
				if (buf[2] & 0x80)
					val |= 0xFF000000;
				buf += 3;
				break;
			case 4:
				val = *(int32_t*)buf;
				buf += 4;
				break;
			}
			this->_data[r][1] = float32_t(val * multiplier);
		}
	}

	void load_from_bytes_x(uint8_t *buf)
	{
		double multiplier = 1.0 / (0x7FFFFFFF >> (8*(4-2)));
		for (int r=0; r < this->chunk_size; ++r)
		{
			uint16_t val;
			val = *(uint16_t*)buf;
			val = ntohs(val);
			buf += 2;
			this->_data[r][0] = (float)(((int16_t)val) * multiplier);
			val = *(uint16_t*)buf;
			val = ntohs(val);
			buf += 2;
			this->_data[r][1] =	 (float)(((int16_t)val) * multiplier);
		}
	}

#if 0
	template <unsigned int bytes_per_sample>
	void load_from_buffer(char *buf)
	{
		char *b = buf;
		const unsigned int mask = (unsigned int)0xFFFFFFFF >> (4 - bytes_per_sample)*8;
		for (int r=0; r < this->chunk_size; ++r)
		{
			PairFromT<SamplePairf, int>(this->_data[r], *((int*)b) & mask, *((int*)(b+_bytes_per_sample)) & mask);
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
