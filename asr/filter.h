#ifndef _FILTER_H
#define _FILTER_H

#include "util.h"
#include "buffer.h"
#include "tracer.h"

#include "dsp/gain.h"
#include "dsp/mixer.h"
#include "dsp/bus.h"

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
	filter_td_base(BufferedStream<Chunk_T> *src, Precision_T cutoff=22050.0, Precision_T input_rate=44100.0, Precision_T output_rate=48000.0) :
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
	//	fill_coeff_tbl();
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

		Sample_T *key_smp = 0;
	//	printf("pos %d\n", pos_stream.size());
		ASIOProcessor *io = ASR::get_io_instance();
		GenericUI *ui = ASR::get_ui_instance();
		const std::deque<ASIOProcessor::pos_info>& pos_stream = io->get_pos_stream();
		std::deque<ASIOProcessor::pos_info>::const_iterator it = pos_stream.begin();
		
		const int track_id = io->get_track_id_for_filter(this);
		const bool vinyl_control_enabled = ui->vinyl_control_enabled(track_id);
		const bool sync_time_enabled = ui->get_sync_time(track_id);
		const bool add_pitch_enabled = ui->get_add_pitch(track_id);

		if (vinyl_control_enabled && it != pos_stream.end())
		{
			key_smp = chk->_data + it->chk_ofs;
		}

		for (smp = chk->_data, end = smp + Chunk_T::chunk_size; smp != end; ++smp)
		{
			if (smp == key_smp)
			{
				//if (abs(_output_time - pos_stream.front().tm) > 0.01)
				{
				//	printf("sync time %d\n", pos_stream.front().sync_time);
					if (sync_time_enabled && abs(_last_tm - it->tm) > 1.0)
					{
				//		printf("key_samp output time %f\n", _output_time);
						_output_time = it->tm;
					}
					_last_tm = it->tm;
					double freq = it->freq;
					if (freq != 0.0)
					{
						if (add_pitch_enabled)
						{
							const double mod = it->mod + ui->get_track_pitch(track_id); 
							freq = 48000.0 / mod;
						}
						double df = get_output_sampling_frequency() - freq;
						if (abs(df) > 0.01)
							printf("set sampling freq = %f.02 df = %f.02\n", freq, df);
						set_output_sampling_frequency(freq);
						
						ui->set_filters_frequency(this, freq); 
					}
					
				}
			//	if (last.tm != 0.0) {
			//		double record_time = (pos_stream.front().tm - last.tm);
			//		double real_time = (pos_stream.front().smp - last.smp) / 48000.0;
			//		set_output_sampling_frequency(48000. / (real_time / record_time));
				//	printf("sampling freq %f\n", this->get_output_sampling_frequency());
			//	}
			//	else
			//	{
				//	set_output_sampling_frequency(48000);
			//	}
				it++;
				if (it != pos_stream.end())
				{
					key_smp = chk->_data + it->chk_ofs;
				} else
				{
					key_smp = 0;
				}
			}
			t_input = _output_time - t_diff;
			smp_ofs_t ofs = smp_ofs_t(t_input * _input_sampling_rate) + 1;
			t_input = ofs / _input_sampling_rate;
			conv_ptr = _buffer_mgr->get_at_ofs(ofs);

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
