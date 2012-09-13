#ifndef _FILTER_H
#define _FILTER_H

#include "util.h"
#include "buffer.h"
#include "tracer.h"

template <typename Source_T>
class gain : public T_source<typename Source_T::chunk_t>, public T_sink<typename Source_T::chunk_t>
{
public:
	gain(Source_T *src) :
		T_sink(src),
		_gain(1.0)
		{
		}

	void set_gain(double g)
	{
		_gain = g;
	}

	void set_gain_db(double g_db)
	{
		dBm<double>::calc(_gain, g_db);
	}

	typename Source_T::chunk_t *next()
	{
		typename Source_T::chunk_t *chk = _src->next();
		for (typename Source_T::chunk_t::sample_t *s = chk->_data, 
			*end = s + Source_T::chunk_t::chunk_size; s != end; ++s)
		{
			(*s)[0] *= _gain;
			(*s)[1] *= _gain;
		}
		return chk;
	}
protected:
	double _gain;
};


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
			(*sout)[0] = (*s1)[0]*_src1_mul + (*s2)[0]*_src2_mul;
			(*sout)[1] = (*s1)[1]*_src1_mul + (*s2)[1]*_src2_mul;
			if (fabs((*sout)[0]) > 1.0 || fabs((*sout)[1]) > 1.0)
				_clip = true;
		//	Product<typename Source_T::chunk_t::sample_t>::calc(*s1, *s1, _src1_mul);
		//	Product<typename Source_T::chunk_t::sample_t>::calc(*s2, *s2, _src2_mul);
		//	Sum<typename Source_T::chunk_t::sample_t>::calc(*sout, *s1, *s2);
		}
		T_allocator<typename Source_T::chunk_t>::free(chk1);
		T_allocator<typename Source_T::chunk_t>::free(chk2);
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
		mixer(src1, src2)
	{
		set_mix(0);
	}

	// 0 <= m <= 100
	// -1 means keep it
	void set_mix(int m=-1)
	{
		if (m == -1)
			m = _m;
		else
			_m = m;

		if (m == 0)
		{
			_src1_mul = _src1_gain;
			_src2_mul = 0.0;
		}
		else if (m == 500)
		{
			_src1_mul = _src1_gain;
			_src2_mul = _src2_gain;
		}
		else if (m < 500)
		{
			_src1_mul = _src1_gain;
			_src2_mul = pow(10.0, (-30.0 + m*(30./500.))/20.0)*_src2_gain;
		}
		else if (m != 1000)
		{
			_src2_mul = _src2_gain;
			_src1_mul = pow(10.0, (-30.0 + (1000-m)*(30./500.))/20.0)*_src1_gain;
		}
		else
		{
			_src2_mul = _src2_gain;
			_src1_mul = 0.0;
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

template <typename Source_T, typename Sink_T>
class io_matrix : public T_sink<typename Source_T::chunk_t>
{
public:
	io_matrix():T_sink(0){}
	void map(Source_T *input, Sink_T *output)
	{
		_src_set.insert(input);
		_io_map[output].insert(input);
		if (_gain_map.find(std::pair<Source_T*, Sink_T*>(input, output)) != _gain_map.end())
			set_gain(input, output, 1.0);
	}
	void set_gain(Source_T *input, Sink_T *output, double gain=1.0)
	{
		_gain_map[std::pair<Source_T*, Sink_T*>(input, output)] = gain;
	}
	void unmap_input(Source_T *input)
	{
		_src_set.erase(input);
		for (std::map<Sink_T*, std::set<Source_T*> >::iterator i = _io_map.begin();
			i != _io_map.end(); ++i)
		{
			i->second.erase(input);
		}
		_chk_map.erase(input);
	}
	void unmap_output(Sink_T *output)
	{
		_io_map.erase(output);
		remap_inputs();
	}
	void remap_inputs()
	{
		_chk_map.clear();
		_src_set.clear();
		for (std::map<Sink_T*, std::set<Source_T*> >::iterator mi = _io_map.begin(); mi != _io_map.end(); ++mi)
		{
			for (std::set<Source_T*>::iterator i = mi->second.begin(); i != mi->second.end(); ++i)
			{
				_src_set.insert(*i);
			}
		}
	}
	// 0 <= m <= 100
	void xfade_2(int m, Source_T *in1, Source_T *in2, Sink_T *out)
	{
		if (m == 0)
		{
			set_gain(in1, out, 1.0);
			set_gain(in2, out, 0.0);
		}
		else if (m == 50)
		{
			set_gain(in1, out, 1.0);
			set_gain(in2, out, 1.0);
		}
		else if (m < 50)
		{
			set_gain(in1, out, 1.0);
			set_gain(in2, out, pow(10.0, (-30.0 + m/1.633)/20.0));
		}
		else
		{
			set_gain(in2, out, 1.0);
			set_gain(in1, out, pow(10.0, (-30.0 + (100-m)/1.633)/20.0));
		}
	}
	void unmap(Source_T *input, Sink_T *output)
	{
		bool still_has_input = false;
		for (std::map<Sink_T*, std::set<Source_T*> >::iterator mi = _io_map.begin(); mi != _io_map.end(); ++mi)
		{
			bool has = false;
			for (std::set<Source_T*>::iterator li = mi->second.begin();
				li != mi->second.end(); ++li)
			{
				if (*li == input)
				{
					has = true;
					break;
				}
			}
			if (has)
			{
				if (mi->first == output)
					mi->second.erase(input);
				else
					still_has_input = true;
			}
		}
		if (!still_has_input)
		{
			_src_set.erase(input);
			_chk_map.erase(input);
		}
	}
	void process(bool freeme=true)
	{
		//for each input, process chunk on each output
		for (std::set<Source_T*>::iterator i = _src_set.begin(); i != _src_set.end(); ++i)
		{
			_chk_map[*i] = (*i)->next(this);
		}

		for (std::map<Sink_T*, std::set<Source_T*> >::iterator mi = _io_map.begin(); mi != _io_map.end(); ++mi)
		{
			typename Source_T::chunk_t *out = T_allocator<typename Source_T::chunk_t>::alloc();
			for (int ofs = 0; ofs < Source_T::chunk_t::chunk_size; ++ofs)
			{
				out->_data[ofs][0] = 0.0;
				out->_data[ofs][1] = 0.0;
			}
			for (std::set<Source_T*>::iterator si = mi->second.begin(); si != mi->second.end(); ++si)
			{
				double gain = _gain_map[std::pair<Source_T*, Sink_T*>(*si, mi->first)];
				for (int ofs = 0; ofs < Source_T::chunk_t::chunk_size; ++ofs)
				{
					out->_data[ofs][0] += _chk_map[*si]->_data[ofs][0] * gain;
					out->_data[ofs][1] += _chk_map[*si]->_data[ofs][1] * gain;
				}
			}
			mi->first->process(out);
		//	_out_q[mi->first].push_back(out);
		}

		for (std::set<Source_T*>::iterator i = _src_set.begin(); i != _src_set.end(); ++i)
		{
			T_allocator<typename Source_T::chunk_t>::free(_chk_map[*i]);
		}
	}
/*	typename Source_T::chunk_t* next(Sink_T *_this)
	{
		typename Source_T::chunk_t* chk = _out_q[_this].front();
		_out_q[_this].pop();
		return chk;
	}*/
protected:
	std::set<Source_T*> _src_set;
	std::map<Sink_T*, std::set<Source_T*> > _io_map;
	std::map<Source_T*, typename Source_T::chunk_t*> _chk_map;
	std::map<std::pair<Source_T*,Sink_T*>, double> _gain_map;
	//std::map<Sink_T*, std::queue<typename Source_T::chunk_t*> > _out_q;
};

template <typename Chunk_T>
class bus : public T_source<Chunk_T>, public T_sink<Chunk_T>
{
public:
	typedef Chunk_T chunk_t;
	bus(T_sink<Chunk_T> *p) :
	  _parent(p) {}
	void process(Chunk_T *chk)
	{
		for (std::map<T_sink<Chunk_T>*, std::queue<Chunk_T*> >::iterator i = _chks_map.begin();
			i != _chks_map.end(); ++i)
		{
			i->second.push(chk);
			chk->add_ref();
		}
		T_allocator<Chunk_T>::free(chk);
	}
	void map_output(T_sink<Chunk_T> *out)
	{
		if (_chks_map.find(out) == _chks_map.end())
			_chks_map[out] = std::queue<Chunk_T*>();
	}
	void unmap_output(T_sink<Chunk_T> *out)
	{
		_chks_map.erase(out);
	}
	void unmap_outputs()
	{
		_chks_map.clear();
	}
	Chunk_T *next(T_sink<Chunk_T> *caller)
	{
		Chunk_T *chk;
		if (_chks_map[caller].empty())
			_parent->process();
		chk = _chks_map[caller].front();
		_chks_map[caller].pop();
		return chk;
	}
protected:
	T_sink<Chunk_T> *_parent;
	std::map<T_sink<Chunk_T>*, std::queue<Chunk_T*> > _chks_map;
};

template <typename Precision_T, int TblSz=512*27>
class coeff_tbl
{
};

template <typename Precision_T, int TblSz=512*27>
class lowpass_coeff_tbl : public coeff_tbl<Precision_T, TblSz>
{
};

template <typename Sample_T, typename Chunk_T>
class full_wave_rectifier;

template <typename Chunk_T>
class full_wave_rectifier<SamplePairf, Chunk_T> : public T_source<Chunk_T>, public T_sink<Chunk_T>
{
public:
	full_wave_rectifier(T_source<Chunk_T> *src) :
		T_sink<Chunk_T>(src)
	{
	}
	Chunk_T *next()
	{
		Chunk_T *chk = _src->next();
		for (SamplePairf *ptr = chk->_data, *end = ptr + Chunk_T::chunk_size; ptr != end; ++ptr)
		{
			(*ptr)[0] = fabs((*ptr)[0]);
			(*ptr)[1] = fabs((*ptr)[1]);
		}
		return chk;
	}
};

template <typename Precision_T=double, int Sample_Precision=13, int Table_Precision=512>
class filter_coeff_table
{
private:
	Precision_T _input_sampling_period;
protected:
	const static int _sample_precision = Sample_Precision;
	const static int _default_tbl_size = 2*Table_Precision*(Sample_Precision+1);
	Precision_T _coeff_tbl[_default_tbl_size];

	virtual Precision_T filter_h(Precision_T, Precision_T) = 0;

	void fill_coeff_tbl(Precision_T input_sampling_period)
	{
		_input_sampling_period = input_sampling_period;
		Precision_T t_diff = Precision_T(Sample_Precision+1) * _input_sampling_period;
		Precision_T t = -t_diff;
		for (int i=0; i < _default_tbl_size; ++i)
		{
			_coeff_tbl[i] = filter_h(t, t_diff);
			t += 2.0*t_diff / (_default_tbl_size);
		}
	}

	Precision_T get_coeff(Precision_T tm) 
	{
		const Precision_T t_diff = Precision_T(Sample_Precision+1) * _input_sampling_period;
		int indx = (tm+t_diff)/(2.0*t_diff / (_default_tbl_size));		
		assert(indx >= 0 && indx < _default_tbl_size);
		return _coeff_tbl[indx];
	}
};

template <typename Chunk_T, typename Precision_T=double, int Dim=1, int SampleBufSz=0x400>
class filter_td_base : public T_source<Chunk_T>, public T_sink<Chunk_T>, public filter_coeff_table<>
{
protected:
	typedef typename Chunk_T::sample_t Sample_T;

	Precision_T _cutoff;

	Precision_T _input_sampling_rate;
	Precision_T _output_sampling_rate;
	Precision_T _output_time;
	Precision_T _input_sampling_period;
	Precision_T _output_sampling_period;

	Precision_T _impulse_response_frequency;
	Precision_T _impulse_response_period;
	Precision_T _rho;
	Precision_T _impulse_response_scale;

	Precision_T _output_scale;

	//int _stride;
	//int _channels;
	int _rows;
	int _cols;
	//bool _row_ly;

	KaiserWindowTable<Precision_T> *_kwt;
	
	virtual Precision_T _h(Precision_T t)
	{
		return Precision_T(1.0);
	}

	virtual Precision_T _h_n(int n)
	{
		return Precision_T(1.0);
	}

	BufferedStream<Chunk_T> *_buffered_stream;
	BufferMgr<BufferedStream<Chunk_T> > *_buffer_mgr;

public:
	filter_td_base(BufferedStream<Chunk_T> *src, Precision_T cutoff=22050.0, Precision_T input_rate=44100.0, Precision_T output_rate=44100.0) :
		T_sink<Chunk_T>(src),
		_cutoff(cutoff),
		_input_sampling_rate(input_rate),
		_output_scale(Precision_T(1.0)),
		_output_time(Precision_T(0.0))
	{
		_kwt = KaiserWindowTable<Precision_T>::get();

		_buffer_mgr = new BufferMgr<BufferedStream<Chunk_T> >(src);
		_input_sampling_period = Precision_T(1.0) / _input_sampling_rate;
		set_output_sampling_frequency(output_rate);
	}

	virtual ~filter_td_base()
	{
		delete _buffer_mgr;
	}

	void set_output_sampling_frequency(Precision_T f)
	{
		_output_sampling_rate = f;
		_output_sampling_period = Precision_T(1.0) / _output_sampling_rate;

		_impulse_response_frequency = min(_input_sampling_rate, _output_sampling_rate);
		_impulse_response_period = Precision_T(1.0) / _impulse_response_frequency;
		//_impulse_response_frequency = _output_sampling_rate;
		//_impulse_response_period = Precision_T(1.0) / _impulse_response_frequency;
		
		_rho = _output_sampling_rate / _input_sampling_rate;
		_impulse_response_scale = min(Precision_T(1.0), _rho);
		fill_coeff_tbl();
	}

	Precision_T get_output_sampling_frequency()
	{
		return _output_sampling_rate;
	}

	Precision_T filter_h(Precision_T t, Precision_T t_diff) 
	{
		return _output_scale*_impulse_response_scale * _h(t) * _kwt->get(t/t_diff);
	}

	void fill_coeff_tbl()
	{
		filter_coeff_table::fill_coeff_tbl(_input_sampling_period);
	}

	void set_output_scale(Precision_T s)
	{
		_output_scale = s;
		_impulse_response_scale = min(Precision_T(1.0), _rho);
		fill_coeff_tbl();
	}

	void seek_time(Precision_T t)
	{
		_output_time = t;
	}

	Precision_T get_time()
	{
		return _output_time;
	}

	void set_cutoff(Precision_T f)
	{
		_cutoff = f;
		_impulse_response_frequency = 2*f;
		_impulse_response_period = Precision_T(1.0) / _impulse_response_frequency;
	//	_rho = _output_sampling_rate / _input_sampling_rate;
		_impulse_response_scale = min(_impulse_response_frequency / _input_sampling_rate, _rho);
		fill_coeff_tbl();
	}

	/*
	
	Row-major ordering
	n0 x n1
	*/
	
	Chunk_T* next()
	{
		Chunk_T *chk = T_allocator<Chunk_T>::alloc();
		Precision_T t_input, t_diff = Precision_T(_sample_precision) * _input_sampling_period;
		Sample_T *smp, *end, *conv_ptr;

		for (smp = chk->_data, end = smp + Chunk_T::chunk_size; smp != end; ++smp)
		{
			t_input = _output_time - t_diff;
			smp_ofs_t ofs = smp_ofs_t(t_input * _input_sampling_rate) + 1;
			t_input = ofs / _input_sampling_rate;
			conv_ptr = _buffer_mgr->get_at_ofs(ofs);

			Precision_T acc[2] = {0.0, 0.0};
			Precision_T mul;
			for (Sample_T *conv_end = conv_ptr + (_sample_precision*2); conv_ptr != conv_end; ++conv_ptr)
			{
#if 0 //direct calculation instead of table. slower but more accurate
				mul = _output_scale*_impulse_response_scale * _h(t_input-_output_time) * _kwt->get((t_input-_output_time)/t_diff);
#else
				
			//	int indx = _default_tbl_size/2+int((_default_tbl_size/2)*(t_input-_output_time)/t_diff);
			/*	t_diff = Precision_T(_sample_precision+1) * _input_sampling_period;
				int indx = (t_input-_output_time+t_diff)/(2.0*t_diff / (_default_tbl_size));		
				assert(indx >= 0 && indx < _default_tbl_size);*/
			//	mul = _output_scale*_impulse_response_scale * _h(t_input-_output_time) * _kwt->get((t_input-_output_time)/t_diff);
			//	printf("%f\n", mul-_coeff_tbl[indx]
			//		//,(t_input-_output_time)-Precision_T(_sample_precision) * _input_sampling_period + indx*2.0*Precision_T(_sample_precision) * _input_sampling_period / (_default_tbl_size)
			//		);
			//	mul = _coeff_tbl[indx];
				mul = get_coeff(t_input-_output_time);
#endif
				acc[0] += (*conv_ptr)[0] * mul;
				acc[1] += (*conv_ptr)[1] * mul;
				
				t_input += _input_sampling_period;
			}

			(*smp)[0] = (float)acc[0];
			(*smp)[1] = (float)acc[1];
			_output_time += _output_sampling_period;
		}
		
		return chk;
	}

	friend class Controller;
	typedef typename Chunk_T chunk_t;
};

template <typename Chunk_T, typename Precision_T=double, int Dim=1, int SampleBufSz=0x400>
class lowpass_filter_td : public filter_td_base<Chunk_T, Precision_T, Dim, SampleBufSz>
{
public:
	lowpass_filter_td(BufferedStream<Chunk_T> *src, Precision_T cutoff=22050.0, Precision_T input_rate=44100.0, Precision_T output_rate=44100.0) :
		filter_td_base(src, cutoff, input_rate, output_rate)
	{
		__asm
		{
			push 0;offset string "ASIOProcessor::BufferSwitch"
			mov eax, next
			push eax
			call Tracer::hook
			add esp, 8
		}
	}
protected:
	Precision_T _h(Precision_T t)
	{
		return sinc<Precision_T>(_impulse_response_frequency*t);
	}
};

template <typename Chunk_T, typename Precision_T=double, int Dim=1, int SampleBufSz=0x400>
class highpass_filter_td : public filter_td_base<Chunk_T, Precision_T, Dim, SampleBufSz>
{
public:
	highpass_filter_td(BufferedStream<Chunk_T> *src, Precision_T cutoff=22050.0, Precision_T input_rate=44100.0, Precision_T output_rate=44100.0) :
		filter_td_base(src, cutoff, input_rate, output_rate)
	{
	}
protected:
	Precision_T _h(Precision_T t) // could be an "n" integer -13 <-> 13 or w/e
	{
		if (t == Precision_T(0.0))
			return Precision_T(1.0) - sinc<Precision_T>(_impulse_response_frequency*t); // wtf check this?
		else
			return -sinc<Precision_T>(_impulse_response_frequency*t);
		//return delta(t)-sinc<Precision_T>(_impulse_response_frequency*t)
	}
	/*
	Precision_T _h(int n)
	{
		if (n == 0)
			return Precision_T(2.0);
		else
			return ???
	}*/
};

template <typename Sample_T=double, typename Precision_T=double>
class dumb_resampler : public filter_coeff_table<>
{
public:
	dumb_resampler(int taps) :
		_taps(taps)
	{
		_kwt = KaiserWindowTable<Precision_T>::get();
		fill_coeff_tbl(Precision_T(1.0));
		_tap_buffer = new Sample_T[_taps];
	}
	virtual ~dumb_resampler()
	{
		delete [] _tap_buffer;
	}
	Sample_T *get_tap_buffer() { return _tap_buffer; }
	Precision_T apply(Precision_T taps_time)
	{
		Precision_T acc = Precision_T(0.0);
		Precision_T t_diff = Precision_T(1.0) * _taps;
		for (Sample_T *p = _tap_buffer; p < _tap_buffer + _taps; ++p)
		{
			//acc += *p * _h(taps_time) * _kwt->get(taps_time/Precision_T(_taps));
			acc += *p * get_coeff(taps_time);
			taps_time += Precision_T(1.0);
		}
		return acc;
	}
	virtual Precision_T _h(Precision_T t)
	{
		return sinc<Precision_T>(t);
	}
	Precision_T filter_h(Precision_T t, Precision_T t_diff) 
	{
		return _h(t) * _kwt->get(t/t_diff);
	}
protected:
	int _taps;
	Sample_T *_tap_buffer;
	KaiserWindowTable<Precision_T> *_kwt;
};

#endif // !defined(_FILTER_H)
