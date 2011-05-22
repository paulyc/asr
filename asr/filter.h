#ifndef _FILTER_H
#define _FILTER_H

#include "util.h"
#include "buffer.h"

#include <fstream>
#include <iostream>

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

template <typename Chunk_T, typename Precision_T=double, int Dim=1, int SampleBufSz=0x400>
class filter_td_base : public T_source<Chunk_T>, public T_sink<Chunk_T>
{
protected:
	typedef typename Chunk_T::sample_t Sample_T;

	Precision_T _cutoff;
	
	const static int _default_sample_precision = 13;
	int _sample_precision;
	Precision_T _input_sampling_rate;
	Precision_T _output_sampling_rate;
	Precision_T _output_time;
	Precision_T _input_sampling_period;
	Precision_T _output_sampling_period;

	Precision_T _impulse_response_frequency;
	Precision_T _impulse_response_period;
	Precision_T _rho;
	Precision_T _impulse_response_scale;

	Precision_T _t_diff;

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
	filter_td_base(T_source<Chunk_T> *src, Precision_T cutoff=22050.0, Precision_T input_rate=44100.0, Precision_T output_rate=44100.0) :
		T_sink<Chunk_T>(src),
		_cutoff(cutoff),
		_sample_precision(_default_sample_precision),
		_input_sampling_rate(input_rate),
		_output_time(Precision_T(0.0))
	{
		Sample_T *end;

		_kwt = KaiserWindowTable<Precision_T>::get();

		_buffered_stream = new BufferedStream<Chunk_T>(src);
		_buffer_mgr = new BufferMgr<BufferedStream<Chunk_T> >(_buffered_stream);
		_input_sampling_period = Precision_T(1.0) / _input_sampling_rate;
		set_output_sampling_frequency(output_rate);
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
	}

	void seek_time(Precision_T t)
	{
		_output_time = t;
	}

	void set_cutoff(Precision_T f)
	{
		_cutoff = f;
		_impulse_response_frequency = 2*f;
		_impulse_response_period = Precision_T(1.0) / _impulse_response_frequency;
	//	_rho = _output_sampling_rate / _input_sampling_rate;
		_impulse_response_scale = min(_impulse_response_frequency / _input_sampling_rate, _rho);
	}

	/*
	
	Row-major ordering
	n0 x n1
	*/
	
	Chunk_T* next()
	{
		Chunk_T *chk = T_allocator<Chunk_T>::alloc();
		int dim = chk->dim();
		assert(dim == Dim);

		Precision_T window_len = Precision_T(2*_sample_precision) * _impulse_response_period;
		_t_diff = Precision_T(_sample_precision) * _input_sampling_period;
		
		convolution_loop(chk);
		
		return chk;
	}

	void convolution_loop(Chunk_T *chk)
	{
		Precision_T t_input;
		Sample_T *smp, *end, *conv_ptr;

		for (smp = chk->_data, end = smp + Chunk_T::chunk_size; smp != end; ++smp)
		{
			t_input = _output_time - _t_diff;
			conv_ptr = _buffer_mgr->get_at(t_input);

			Precision_T acc[2] = {0.0, 0.0};
			Precision_T mul;
			for (Sample_T *conv_end = conv_ptr + (_sample_precision*2); conv_ptr != conv_end; ++conv_ptr)
			{
				mul = _impulse_response_scale * _h(t_input-_output_time) * _kwt->get((t_input-_output_time)/_t_diff);
				acc[0] += (*conv_ptr)[0] * mul;
				acc[1] += (*conv_ptr)[1] * mul;
				
				t_input += _input_sampling_period;
			}

			(*smp)[0] = (float)acc[0];
			(*smp)[1] = (float)acc[1];
			_output_time += _output_sampling_period;
		}
	}
	friend class Controller;
};

template <typename Chunk_T, typename Precision_T=double, int Dim=1, int SampleBufSz=0x400>
class lowpass_filter_td : public filter_td_base<Chunk_T, Precision_T, Dim, SampleBufSz>
{
public:
	lowpass_filter_td(T_source<Chunk_T> *src, Precision_T cutoff=22050.0, Precision_T input_rate=44100.0, Precision_T output_rate=44100.0) :
		filter_td_base(src, cutoff, input_rate, output_rate)
	{
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
	highpass_filter_td(T_source<Chunk_T> *src, Precision_T cutoff=22050.0, Precision_T input_rate=44100.0, Precision_T output_rate=44100.0) :
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
class dumb_resampler
{
public:
	dumb_resampler(int taps) :
		_taps(taps)
	{
		_kwt = KaiserWindowTable<Precision_T>::get();
	}
	Sample_T *get_tap_buffer() { return _tap_buffer; }
	Precision_T apply(Precision_T taps_time)
	{
		Precision_T acc = Precision_T(0.0);
		Precision_T t_diff = taps_period * taps;
		for (Sample_T *p = _tap_buffer; p < _tap_buffer + taps; ++p)
		{
			acc += *p * _h(taps_time) * _kwt->get(taps_time/Precision_T(_taps));
			taps_time += Precision_T(1.0);
		}
		return acc;
	}
	virtual Precision_T _h(Precision_T t)
	{
		return sinc<Precision_T>(t);
	}
protected:
	int _taps;
	Sample_T *_tap_buffer;
	KaiserWindowTable<Precision_T> *_kwt;
};

// legacy ;-)
#if 0
template <typename Chunk_T, typename Precision_T=double>
class filter_td_unmanaged : public filter_td_base<Chunk_T>
{
	typedef typename Chunk_T::sample_t Sample_T;

	Sample_T *_smp_buf;
	
	Sample_T *_smp_buf_write;
	Precision_T _t_smp_buf_write;
	
	Sample_T *_smp_buf_read;
	Precision_T _t_smp_buf_rd;
	
	Chunk_T *_input_chk;
	Sample_T *_input_read;
public:
	~filter_td_unmanaged()
	{
		T_allocator<Chunk_T>::free(_input_chk);
		delete [] _smp_buf;
	}

	void init() // call after parent constructor
	{
		load_chunk();

		_smp_buf = new Sample_T[SampleBufSz];
		_smp_buf_read = _smp_buf;
		_smp_buf_write = _smp_buf;

		// fill input buffer
		//for (int ch = 0; ch < _channels; ++ch)
		{
			//Zero beginning
			//if (_row_ly)
			{
				for (end = _smp_buf_write + 4 * _sample_precision; _smp_buf_write != end; ++_smp_buf_write)
				{
					(*_smp_buf_write)[0] = 0.0f;
					(*_smp_buf_write)[1] = 0.0f;
				}
			}
			/*else
			{
				for (ptr = _smp_buf + ch * _cols, end = ptr + 4*
					_sample_precision; ptr != end; )
				{
					(*ptr)[0] = Zero<Sample_T>::val;
					(*ptr++)[0] = Zero<Sample_T>::val;
				}
			}*/
		}

		//_t_smp_buf_write = Precision_T(_sample_precision) * _input_sampling_period;
		_t_smp_buf_write = 0;
		_t_smp_buf_rd = - Precision_T(_sample_precision) * 4* _input_sampling_period;
		//printf("write ofs %d read ofs %d\n", _smp_buf_write_ofs, _smp_buf_read_ofs);
		check_buffer();
		//printf("write ofs %d read ofs %d\n", _smp_buf_write_ofs, _smp_buf_read_ofs);
		
		//set_cutoff(cutoff);
	}

	void load_chunk()
	{
		T_allocator<Chunk_T>::free(_input_chk);
		_input_chk = 0;
		_input_chk = _src->next();
		_input_read = _input_chk->_data;

		assert(_input_chk);
		_rows = Chunk_T::chunk_size;
		_cols = 1;
	//	_row_ly = _rows > _cols;
	//	_stride = _row_ly ? _cols : 1;
	//	_channels = _row_ly ? _cols : _rows;
	}

	void load_from_input(size_t to_write)
	{
		Sample_T *end_write;
		size_t loop_write;
		// consume from input chunk
	//	if (_row_ly)
		{
			loop_write = min(_rows - (_input_read - _input_chk->_data), to_write);
		}
	//	else
	//	{
	//		loop_write = min(_cols - _input_read_ofs, to_write);
	//	}
		if (loop_write == 0)
		{
			load_chunk();
			loop_write = min(_rows - (_input_read - _input_chk->_data), to_write);
		}
		//for (int ch = 0; ch < _channels; ++ch)
		{
			//if (_row_ly)
			{
				assert(loop_write > 0);
				for (end_write = _smp_buf_write + loop_write; _smp_buf_write != end_write; )
				{
					(*_smp_buf_write)[0] = (*_input_read)[0];
					(*_smp_buf_write++)[1] = (*_input_read++)[1];
				}
			}
			/*else
			{
				assert(0);
				for (write = _smp_buf + _smp_buf_write_ofs + _rows*ch, 
					read = _input_chk->_data + _input_read_ofs + _rows*ch,
					end_write = write + loop_write;
					write != end_write; )
				{
					*write++ = *read++;
				}
			}*/
		}
		//_smp_buf_write_ofs += loop_write;
		//_input_read_ofs += loop_write;
		_t_smp_buf_write += _input_sampling_period * Precision_T(loop_write);
		if (loop_write < to_write)
		{
			load_chunk();
			load_from_input(to_write - loop_write);
		}
	}

	void check_buffer()
	{
	//	Precision_T t_end_need = _t_smp_buf_rd + 16*_sample_precision * _input_sampling_period;
		size_t to_write;
		//printf("check_buffer 
		Precision_T tm_stop_rding = _output_time - 4 *_sample_precision * _input_sampling_period;
		assert(_t_smp_buf_rd < _t_smp_buf_write);
		
		while (_t_smp_buf_rd < tm_stop_rding && _smp_buf_read < _smp_buf_write)
		{
			++_smp_buf_read;
			_t_smp_buf_rd += _input_sampling_period;
			
		}
		Precision_T tm_stop_writing = _output_time + 4 *_sample_precision* _input_sampling_period;
		Precision_T tm_foo = _t_smp_buf_rd + _input_sampling_period*(_smp_buf_write-_smp_buf_read);
//		assert(_smp_buf_read_ofs <= _smp_buf_write_ofs - 26);
		//printf("tm_stop_rding %f tm_stop_writing %f\n", tm_stop_rding, tm_stop_writing);
		while (_t_smp_buf_write < tm_stop_writing)
		{
			
			// check if there is any space left in sample buffer
			to_write = SampleBufSz - (_smp_buf_write - _smp_buf);
			/*printf("loading smp_buf at _output_time %f (_t_smp_buf_write - _t_smp_buf_read) %f (_smp_buf_write_ofs-_smp_buf_read_ofs) %d to_write %d _smp_buf_read_ofs %d _smp_buf_write_ofs %d t_x %f\n", 
				_output_time, _t_smp_buf_write - _t_smp_buf_rd, 
				(_smp_buf_write_ofs-_smp_buf_read_ofs), to_write, _smp_buf_read_ofs, _smp_buf_write_ofs , 
				_t_smp_buf_write-_t_smp_buf_rd-(_smp_buf_write_ofs-_smp_buf_read_ofs)*_input_sampling_period);
			*/
			assert(to_write >= 0);
			if (to_write > 0)
			{
				load_from_input(to_write);
			}
			else // buffer is full, move end to beginning
			{
				//printf("moving %d-%d\n", _smp_buf_read_ofs, _smp_buf_write_ofs);
				//if (_row_ly)
				{
					Sample_T *to=_smp_buf, *from=_smp_buf_read;
					size_t bytes = (_smp_buf_write-_smp_buf_read)*sizeof(Sample_T);
					//assert(_cols==_channels);
					memmove(to, from, bytes);
				}
				/*else
				{
					for (int ch = 0; ch < _channels; ++ch)
					{
						assert(0); //bad, fix as above
						//memmove(_smp_buf + ch*_cols, 
						//_smp_buf + (ch+1)*_cols - 2*_sample_precision, 
						//2*_sample_precision*sizeof(Sample_T));
					}
				}
				printf("write ofs was %d now %d\n", _smp_buf_write_ofs, _smp_buf_write_ofs-_smp_buf_read_ofs);
				printf("read ofs was %d now 0\n", _smp_buf_read_ofs);
				*/
				_smp_buf_write = _smp_buf+(_smp_buf_write-_smp_buf_read);
				_smp_buf_read = _smp_buf;
			}
		}
	}

	void seek_time(Precision_T t)
	{
		// dump sample buffer and fill again
		filter_td_base::seek_time(t);

		Precision_T tm_start = t - 4 *_sample_precision * _input_sampling_period;
		smp_ofs_t smp_start = (int)(tm_start * _input_sampling_rate);
		_src->seek_smp(smp_start);
		load_chunk();
		_smp_buf_read = _smp_buf;
		_t_smp_buf_rd = tm_start;
		int to_write = min(SampleBufSz, _rows);
		memcpy(_smp_buf, _input_read, to_write*sizeof(Sample_T));
		_smp_buf_write = _smp_buf + to_write;
		_input_read += to_write;
		_t_smp_buf_write = _t_smp_buf_rd + _input_sampling_period * to_write;
	}

	Chunk_T* next()
	{
		Precision_T t_diff, t_input;
		Chunk_T *chk = T_allocator<Chunk_T>::alloc();
		const int *n = chk->sizes_as_array();
		int dim = chk->dim();

		assert(dim == Dim);
		Sample_T *smp, *end, *conv_ptr, *conv_ptr_tmp;
		size_t samples;
		Precision_T window_len = Precision_T(2*_sample_precision) * _impulse_response_period;
		//if (dim == 2)
		{
			//_row_ly ? samples = n[0] : samples = n[1];
			samples = Chunk_T::chunk_size;
		}
		t_diff = Precision_T(_sample_precision) * _input_sampling_period;
		
//		if (_row_ly)
		{
			for (smp = chk->_data, end = smp + samples; smp != end; ++smp)
			{
				//float smp_buf_end = _t_smp_buf_rd + (_smp_buf_write_ofs-_smp_buf_read_ofs) * _input_sampling_period;
				//assert(smp_buf_end == _t_smp_buf_write);
				check_buffer();
				

				tm = _output_time - t_diff;
				t_input = _t_smp_buf_rd;
				assert(t_input <= tm);
				conv_ptr = _smp_buf_read;
				while (t_input < tm)
				{
					++conv_ptr;
					t_input += _input_sampling_period;
				}

				//assert (tm >= _t_smp_buf_rd);
				
/*
				while (_t_smp_buf_rd < t_input - 10*_input_sampling_period)
				{
					++_smp_buf_read_ofs;
					_t_smp_buf_rd += _input_sampling_period;
				}*/
				assert(t_input > _t_smp_buf_rd);
				assert(conv_ptr >= _smp_buf_read);
				/*while (tm > _t_smp_buf_rd)
				{
					++_smp_buf_read_ofs;
					_t_smp_buf_rd += _input_sampling_period;
				}*/

				
				//conv_ptr_tmp = conv_ptr;
				//t_input_tmp = t_input;
				
				//for (int ch = 0; ch < _channels; ++ch)
				{
					Precision_T acc[2] = {0.0, 0.0};
				//	acc = Sample_T(0.0);
				//	assert(_t_smp_buf_rd < _output_time);
				//	tm = _t_smp_buf_rd - _output_time;
				//	conv_ptr = conv_ptr_tmp+ch;
				//	t_input = t_input_tmp;
				//	int before =0,same=0,after=0;
				//	printf("ch\n");
					Precision_T mul;
					for (Sample_T *conv_end = conv_ptr + (_sample_precision*2); conv_ptr != conv_end; ++conv_ptr)
					{
						//printf("t_input %f t_output %f %f += %f * %f * %f * %f\n", t_input, _output_time, acc, *conv_ptr, _impulse_response_scale, sinc<Precision_T>(_impulse_response_frequency*(t_input-_output_time)), _kwt.get((t_input-_output_time)/t_diff));
						mul = _impulse_response_scale * _h(t_input-_output_time) * _kwt->get((t_input-_output_time)/t_diff);
						acc[0] += (*conv_ptr)[0] * mul;
						acc[1] += (*conv_ptr)[1] * mul;
					
						t_input += _input_sampling_period;
					}

					(*smp)[0] = (float)acc[0];
					(*smp)[1] = (float)acc[1];
				}
				_output_time += _output_sampling_period;
				
			}
		}
	//	else
	//	{
	//		assert(0);
	//	}
		return chk;
	}
};
#endif

#endif // !defined(_FILTER_H)
