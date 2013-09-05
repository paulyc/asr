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

template <typename Source_T, int channels=2>
class mixer : public T_source<typename Source_T::chunk_t>
{
};

template <typename Source_T>
class mixer<Source_T, 2> : public T_source<typename Source_T::chunk_t>
{
public:
	mixer(Source_T *src1, Source_T *src2) :
		_clip(false),
		_src1(src1),
		_src2(src2),
		_src1_mul(1.0),
		_src2_mul(1.0),
		_src1_gain(1.0),
		_src2_gain(1.0)
	{
	}

	typename Source_T::chunk_t *next(typename Source_T::chunk_t *chk1, typename Source_T::chunk_t *chk2)
	{
		typename Source_T::chunk_t //*chk1 = _src1->next(),
			//*chk2 = _src2->next(), 
			*chk_out = T_allocator<typename Source_T::chunk_t>::alloc();
		typename Source_T::chunk_t::sample_t *s1 = chk1->_data,
			*s2 = chk2->_data, *sout = chk_out->_data, 
			*send = sout + Source_T::chunk_t::chunk_size;
		_clip = false;
		for (; sout != send; ++s1, ++s2, ++sout)
		{
			(*sout)[0] = float32_t((*s1)[0]*_src1_mul + (*s2)[0]*_src2_mul);
			(*sout)[1] = float32_t((*s1)[1]*_src1_mul + (*s2)[1]*_src2_mul);
			if (fabs((*sout)[0]) > 1.0 || fabs((*sout)[1]) > 1.0)
				_clip = true;
		}
		return chk_out;
	}

	virtual void set_gain(int src, double gain)
	{
		if (src == 1)
			_src1_gain = gain;
		else
			_src2_gain = gain;
	}

	bool _clip;
protected:
	Source_T *_src1;
	Source_T *_src2;

	double _src1_mul;
	double _src2_mul;
	double _src1_gain;
	double _src2_gain;
};

template <typename Source_T>
class xfader : public mixer<Source_T, 2>
{
public:
	typedef typename Source_T::chunk_t chunk_t;
	xfader(Source_T *src1, Source_T *src2) :
		mixer<Source_T, 2>(src1, src2)
	{
		set_mix(0);
	}

	// 0 <= m <= 1000
	// -1 means keep it
	void set_mix(int m=-1)
	{
		if (m == -1)
			m = _m;
		else
			_m = m;

		if (m == 0)
		{
			this->_src1_mul = this->_src1_gain;
			this->_src2_mul = 0.0;
		}
		else if (m == 500)
		{
			this->_src1_mul = this->_src1_gain;
			this->_src2_mul = this->_src2_gain;
		}
		else if (m < 500)
		{
			this->_src1_mul = this->_src1_gain;
			this->_src2_mul = pow(10.0, (-30.0 + m*(30./500.))/20.0)*this->_src2_gain;
		}
		else if (m != 1000)
		{
			this->_src2_mul = this->_src2_gain;
			this->_src1_mul = pow(10.0, (-30.0 + (1000-m)*(30./500.))/20.0)*this->_src1_gain;
		}
		else
		{
			this->_src2_mul = this->_src2_gain;
			this->_src1_mul = 0.0;
		}
	}
	void set_gain(int src, double gain)
	{
		mixer<Source_T, 2>::set_gain(src, gain);
		set_mix();
	}
	
protected:
	int _m;
};

template <typename Chunk_T>
class static_mixer
{
public:
	struct input
	{
		Chunk_T *chk;
		double mul;
	};
	static void mix2(Chunk_T *out, Chunk_T *in1, Chunk_T *in2, double mul1=1.0, double mul2=1.0)
	{
		typename Chunk_T::sample_t *s1 = in1->_data,
			*s2 = in2->_data, *sout = out->_data, 
			*send = sout + Chunk_T::chunk_size;
		for (; sout != send; ++s1, ++s2, ++sout)
		{
			(*sout)[0] = (*s1)[0]*mul1 + (*s2)[0]*mul2;
			(*sout)[1] = (*s1)[1]*mul1 + (*s2)[1]*mul2;
		}
	}
};