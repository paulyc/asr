#ifndef _FILTER_H
#define _FILTER_H

#define _USE_MATH_DEFINES
#include <math.h>

#include "../util.h"
#include "../buffer.h"
#include "../tracer.h"

#include "gain.h"
#include "mixer.h"
#include "bus.h"

template <typename Sample_T, typename Chunk_T>
class full_wave_rectifier;

template <typename Chunk_T>
class full_wave_rectifier<SamplePairf, Chunk_T> : public T_sink_source<Chunk_T>
{
public:
	full_wave_rectifier(T_source<Chunk_T> *src) :
		T_sink_source<Chunk_T>(src)
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

	virtual void fill_coeff_tbl(Precision_T input_sampling_period)
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

template <typename Chunk_T, typename Precision_T=double, int Dim=1>
class resampling_filter_td : public T_source<Chunk_T>, public T_sink<Chunk_T>, public filter_coeff_table<>
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
	
	Precision_T _h(Precision_T t)
	{
		return sinc<Precision_T>(_impulse_response_frequency*t);
	}

	BufferedStream<Chunk_T> *_buffered_stream;

public:
	resampling_filter_td(BufferedStream<Chunk_T> *src, Precision_T input_rate=44100.0, Precision_T output_rate=48000.0) :
		T_sink<Chunk_T>(src),
		_input_sampling_rate(input_rate),
		_output_scale(Precision_T(1.0)),
		_output_time(Precision_T(0.0)),
		_buffered_stream(src)
	{
		_kwt = KaiserWindowTable<Precision_T>::get();

		_input_sampling_period = Precision_T(1.0) / _input_sampling_rate;
		set_output_sampling_frequency(output_rate);
	}

	virtual ~resampling_filter_td()
	{
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
	//	fill_coeff_tbl(_input_sampling_period);
	}

	Precision_T get_output_sampling_frequency()
	{
		return _output_sampling_rate;
	}

	Precision_T filter_h(Precision_T t, Precision_T t_diff) 
	{
		return _output_scale*_impulse_response_scale * _h(t) * _kwt->get(t/t_diff);
	}

	void set_output_scale(Precision_T s)
	{
		_output_scale = s;
		_impulse_response_scale = min(Precision_T(1.0), _rho);
	//	fill_coeff_tbl(_input_sampling_period);
	}

	void seek_time(Precision_T t)
	{
		_output_time = t;
	}

	Precision_T get_time()
	{
		return _output_time;
	}

/*	void set_cutoff(Precision_T f)
	{
		_cutoff = f;
		_impulse_response_frequency = 2*f;
		_impulse_response_period = Precision_T(1.0) / _impulse_response_frequency;
	//	_rho = _output_sampling_rate / _input_sampling_rate;
		_impulse_response_scale = min(_impulse_response_frequency / _input_sampling_rate, _rho);
		fill_coeff_tbl(_input_sampling_period);
	}
*/
	double _last_tm;

	/*
	
	Row-major ordering
	n0 x n1
	*/
	
	Chunk_T* next()
	{
		Chunk_T *chk = T_allocator<Chunk_T>::alloc();
		Precision_T t_input, t_diff = Precision_T(_sample_precision) * _input_sampling_period;
		Sample_T *smp, *end, *conv_ptr;

	//	printf("pos %d\n", pos_stream.size());

		for (smp = chk->_data, end = smp + Chunk_T::chunk_size; smp != end; ++smp)
		{
			t_input = _output_time - t_diff;
			smp_ofs_t ofs = smp_ofs_t(t_input * _input_sampling_rate) + 1;
			t_input = ofs / _input_sampling_rate;
			conv_ptr = _buffered_stream->get_at_ofs(ofs, 30);

			Precision_T acc[2] = {0.0, 0.0};
			Precision_T mul;
			for (Sample_T *conv_end = conv_ptr + (_sample_precision*2); conv_ptr != conv_end; ++conv_ptr)
			{
#if 1 //direct calculation instead of table. slower but more accurate
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
	
	typename ASIOProcessor::pos_info last;
	
	friend class Controller;
	typedef typename Chunk_T chunk_t;
};

class lowpass_filter : public T_source<chunk_t>
{
public:
	lowpass_filter(FilterSource<chunk_t> *src, double cutoff, double sampleRate) :
	  _src(src),
	  _fCutoff(cutoff),
	  _fSampleRate(sampleRate),
	  _tOutputSample(0.0)
	{
		KaiserWindowTable<double> * kwt = KaiserWindowTable<double>::get();
		_nCoeffs = int(sampleRate / cutoff);
		if (_nCoeffs > 2000)
			_nCoeffs = 2000;
	//	_nCoeffs = 100;
		_coeffs = new double[_nCoeffs*2 + 1];
		for (int k=-_nCoeffs; k <= _nCoeffs; ++k)
			_coeffs[k+_nCoeffs] = (2*_fCutoff/_fSampleRate)*sinc(2*_fCutoff*(k/_fSampleRate)) * kwt->get(k / _nCoeffs);
	}
  ~lowpass_filter()
  {
	  delete [] _coeffs;
  }

	 static double sinc(double x)
	 {
		 if (x == 0.0)
			 return 1.0;
		 else
			 return sin(M_PI*x)/(M_PI*x);
	 }

	chunk_t *next()
	{
		chunk_t * chk = T_allocator<chunk_t>::alloc();
		
		for (SamplePairf *smp = chk->_data; smp < chk->_data + chunk_t::chunk_size; ++smp)
		{
			double tInputSample = _tOutputSample - _nCoeffs / _fSampleRate;
			smp_ofs_t sampleOffset = (smp_ofs_t)(tInputSample * _fSampleRate);
			SamplePairf * inputSample = _src->get_at_ofs(sampleOffset, 3*_nCoeffs);
			tInputSample = sampleOffset / _fSampleRate;
		//	double t = -130. / _fSampleRate; // .000294784
			double acc[2] = {0.0, 0.0};

			for (int k = -_nCoeffs; k <= _nCoeffs; ++k)
			{
			//	double h = (2*_fCutoff/_fSampleRate)*sinc(2*_fCutoff*(k/_fSampleRate));
				acc[0] += (*inputSample)[0] * _coeffs[k+_nCoeffs] ;
				acc[1] += (*inputSample)[1] * _coeffs[k+_nCoeffs];
			//	t += 1. / _fSampleRate;
				++inputSample;
			}

			(*smp)[0] = (float32_t)acc[0];
			(*smp)[1] = (float32_t)acc[1];

			_tOutputSample += 1.0 / _fSampleRate;
		}
		return chk;
	}

	void seek_smp(smp_ofs_t smp_ofs)
	{
		_src->seek_smp(smp_ofs);
		_tOutputSample = smp_ofs / _fSampleRate;
	}

	T_source<chunk_t>::pos_info& len()
	{
		return _src->len();
	}

	bool eof()
	{
		return _src->eof();
	}

	FilterSource<chunk_t> *_src;
	double _fCutoff;
	double _fSampleRate;
	int _nCoeffs;
	double *_coeffs;

	double _tOutputSample;
};


/*
template <typename Chunk_T, typename Precision_T=double, int Dim=1>
class highpass_filter_td : public filter_td_base<Chunk_T, Precision_T, Dim>
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
};

template <typename Chunk_T, typename Precision_T=double, int Dim=1>
class bandpass_filter_td : public filter_td_base<Chunk_T, Precision_T, Dim>
{
public:
	bandpass_filter_td(BufferedStream<Chunk_T> *src, Precision_T low_cutoff, Precision_T high_cutoff, Precision_T input_rate=44100.0, Precision_T output_rate=44100.0) :
		filter_td_base(src, low_cutoff, input_rate, output_rate)
	{
		_h_f_lo = 2*low_cutoff;
		_h_f_hi = 2*high_cutoff;
	}
protected:
	Precision_T _h(Precision_T t)
	{
		Precision_T sum = Precision_T(0.0);
		for (Precision_T t1=t; t1 < Precision_T(_sample_precision+1); t1 += Precision_T(1.0))
		{
			sum += _h_lp(t1) * _h_hp(t-t1);
		}
		for (Precision_T t2 = t - Precision_T(1.0); t2 > -Precision_T(_sample_precision+1); t2 -= Precision_T(1.0))
		{
			sum += _h_lp(t2) * _h_hp(t-t2);
		}
		return sum;
	}

	Precision_T _h_lp(Precision_T t)
	{
		return sinc<Precision_T>(_h_f_lo*t);
	}

	Precision_T _h_hp(Precision_T t) // could be an "n" integer -13 <-> 13 or w/e
	{
		if (t == Precision_T(0.0))
			return Precision_T(1.0) - sinc<Precision_T>(_h_f_hi*t); // wtf check this?
		else
			return -sinc<Precision_T>(_h_f_hi*t);
		//return delta(t)-sinc<Precision_T>(_impulse_response_frequency*t)
	}


	Precision_T _h_f_hi;
	Precision_T _h_f_lo;
};*/

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
