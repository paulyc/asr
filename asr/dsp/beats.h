#ifndef _BEATS_H
#define _BEATS_H

#include <assert.h>

#include "fft.h"

class FilterSourceImpl : public FilterSource<chunk_t>
{
public:
	FilterSourceImpl(T_source<chunk_t> *src)
		: FilterSource<chunk_t>(src),
		_ringBuffer(100000)
	{
	//	chunk_t *chk = _src->next();
		_bufferReadOfs = -10000;
		SamplePairf values[10000] = {0.0,0.0};
		_ringBuffer.write(values, 10000);
	//	_ringBuffer.write(chk->_data, chunk_t::chunk_size);
		
	}

	virtual ~FilterSourceImpl()
	{
	}

	void seek_smp(smp_ofs_t smp_ofs)
	{
		printf("do not call\n");
		_src->seek_smp(smp_ofs - 500);
		_bufferReadOfs = smp_ofs - 500;
		chunk_t *chk = _src->next();
		_ringBuffer.clear();
		_ringBuffer.write(chk->_data, chunk_t::chunk_size);
	}
	virtual void seek_chk(int chk_ofs)
	{
	//	_src->seek_chk(chk_ofs);
	}
//	void process(bool freeme=true)
//	{
//		Chunk_T *chk = next();
//		T_allocator<T>::free(chk);
//	}

	SamplePairf *get_at_ofs(smp_ofs_t ofs, int n)
	{
		_ringBuffer.ignore(ofs - _bufferReadOfs);
		_bufferReadOfs += ofs - _bufferReadOfs;
		while (ofs + n > _bufferReadOfs + _ringBuffer.count())
		{
		//	int skip = _ringBuffer.count() - (n+100) ;
		//	if (skip <= 0) fprintf(stderr, "this is not supposed to happen!\n");
		//	_ringBuffer.ignore(skip);
		//	_bufferReadOfs += skip;

			chunk_t *chk = _src->next();
			_ringBuffer.write(chk->_data, chunk_t::chunk_size);
			T_allocator<chunk_t>::free(chk);
		}
		SamplePairf *atReadOfs = _ringBuffer.peek(ofs - _bufferReadOfs + n);
		return atReadOfs + (ofs - _bufferReadOfs);
	}
private:
	RingBuffer<SamplePairf> _ringBuffer;
	smp_ofs_t _bufferReadOfs;
};

class Differentiator
{
public:
	Differentiator(int size=3)
	{
		_size = size;
		_buffer = new double[size];
		_ptr = _buffer;
		for (int i=0; i<size; ++i)
			_buffer[i] = 0.0;
	}
	~Differentiator()
	{
		delete [] _buffer;
	}
	double dx()
	{
		if (_ptr == _buffer)
			return *_ptr - *(_ptr + _size -1);
		else
			return *_ptr - *(_ptr-1);
	}
	void next(double x)
	{
		++_ptr;
		if (_ptr >= _buffer + _size)
			_ptr = _buffer;
		*_ptr = x;
	}
private:
	int _size;
	double *_buffer;
	double *_ptr;
};

template <typename Chunk_T>
class BeatDetector : public T_sink_source<Chunk_T>
{
public:
	BeatDetector() : 
		T_sink_source(0),
		_my_src(0),
		_rectifier(&_s1),
		_passthrough_sink(&_s2),
		_lpf(2048, 44100.0, 100.0),
		_k(1024),
		_s1(_lpf, 2048, 1024, 20),
		_s2(_k, 1024, 512, 20)
	{
		_lpf.init();
		_k.init();
	}

	void reset_source(T_source<Chunk_T> *src)
	{
		_src = src;
		_my_src.reset_source(_src);
		_s1.reset_source(&_my_src);
		_s2.reset_source(&_rectifier);

		_t = 0.0;
		_pos = true;
		_start = false;
		_x_max = 0.0;
		_chk_ofs = 0;

		_dt_sum = 0.0;
		_dt_points = 0;
		_dt_avg = 0.0;

		_peak_list.clear();
		_beat_list.clear();
		_beat_avg_list.clear();

		_max_peak = point();
		_max = point();
		_maxs.clear();
		_last_beat = point();
	}

	struct point
	{
		point() : valid(false), t(0.0),x(0.0), dx(0.0){}
		point(double _t, double _x, double _dx) : valid(true), t(_t), x(_x),dx(_dx){}
		bool valid;
		double t;
		double x;
		double dx;
	};
	int _chk_ofs;

	virtual void seek_chk(int chk_ofs)
	{
		if (chk_ofs == _chk_ofs)
		{
			++_chk_ofs;
			if (_chk_ofs % 100 == 0)
			{
				printf("chk %d\n", _chk_ofs);
			}
		}
		else
		{
			printf("hello\n");
		}
	}

	const std::vector<double>& beats()
	{
		if (_beat_avg_list.empty())
		{
			std::list<point>::iterator i = _beat_list.begin();
			double last_t = i->t;
			_beat_avg_list.push_back(last_t);
			i++;
			while (i != _beat_list.end())
			{
				double dt = i->t - last_t;
				if (dt > 1.5*_dt_avg)
				{
					last_t += _dt_avg;
					printf("bpm = %f\n", 60.0/_dt_avg);
					_beat_avg_list.push_back(last_t);
					continue;
				}
				else if (dt < 0.5*_dt_avg)
				{
					i++;
					continue;
				}
				else
				{
					last_t += _dt_avg;
					double diff = 0.1 * (i->t - last_t);
					last_t += diff;
					printf("bpm = %f\n", 60.0/(_dt_avg+diff));
					_beat_avg_list.push_back(last_t);
					i++;
				}
			}
			for (int j=0; j<10; ++j)
			{
				printf("bpm = %f\n", 60.0/_dt_avg);
				last_t += _dt_avg;
				_beat_avg_list.push_back(last_t);
			}
		}
		return _beat_avg_list;
	}

	Chunk_T *next()
	{
		Chunk_T *process_chk = _passthrough_sink.next();
		Chunk_T *passthru_chk = _my_src.get_next();
		
		SamplePairf last = {0.0f, 0.0f};
		for (SamplePairf *smp = process_chk->_data, *end = smp + Chunk_T::chunk_size; smp != end; ++smp)
		{
			double x = (*smp)[0];
			_diff.next(x);
			double dx = _diff.dx();
			_diff2.next(dx);
			double ddx = _diff2.dx();
			if (x > _x_max)
			{
				_x_max = x;
				_start = true;
			}
			else if (x < 0.1 * _x_max)
			{
				if (_start)
				{
				//	printf("hello\n");
					_start = false;
					// pick point with highest dx
					for (std::list<point>::iterator i = _peak_list.begin(); i != _peak_list.end(); i++)
					{
						if (i->dx > _max.dx && i->x > 0.15 * _x_max)
						{
							_max = *i;
							_maxs.push_back(_max);
						}
						else if (!_maxs.empty() && i->x < 0.05 * _x_max)//_max.x)
						{
							// pick first
							if (!_last_beat.valid && _dt_points > 5)
							{
								_last_beat.t = _maxs.begin()->t - _dt_avg;
								_last_beat.valid = true;
							}
							if (_last_beat.valid)
							{
								double dt = _maxs.begin()->t - _last_beat.t;
								if (_dt_points < 5 || abs(dt-_dt_avg) / _dt_avg < 0.1)
								{
									_dt_sum += dt;
									++_dt_points;
									_dt_avg = _dt_sum / _dt_points;
								}
								printf("bpm %f avg %f\n", 60.0/dt, 60.0/_dt_avg);
							}
							_last_beat = *_maxs.begin();
							_last_t = _last_beat.t;
							_beat_list.push_back(*_maxs.begin());
							_max = point();
							_maxs.clear();
						}
					}
					_peak_list.clear();
				}
			}
			else if (x > 0.2 * _x_max)
			{
				_start = true;
			}

			if (dx < 0)
			{
				if (_pos)
				{
					_pos = false;
					// record max
					if (_max_peak.valid)
					{
						_peak_list.push_back(_max_peak);
						_max_peak = point();
					}
				}
			}
			else
			{
				if (!_pos)
					_pos = true;
				if (dx > _max_peak.dx)
				{
					_max_peak.t = _t - 2.0/44100.0;
					_max_peak.dx = dx;
					_max_peak.x = x;
					_max_peak.valid = true;
				}
			}
			_t += 1.0 / 44100.0;
		}
	//	avg[0] /= Chunk_T::chunk_size;
	//	avg[1] /= Chunk_T::chunk_size;
	//	printf("avg values of envelope %f %f\n", avg[0], avg[1]);
	
	//	T_allocator<Chunk_T>::free(passthru_chk);
	//	return process_chk;
		T_allocator<Chunk_T>::free(process_chk);
		return passthru_chk;
	}

private:
	std::list<point> _peak_list;
	std::list<point> _beat_list;
	std::vector<double> _beat_avg_list;

	double _t;
	QueueingSource<Chunk_T> _my_src;
	full_wave_rectifier<SamplePairf, Chunk_T> _rectifier;
	T_sink_source<Chunk_T> _passthrough_sink;
	bool _start, _pos;
	Differentiator _diff, _diff2;
	double _x_max;
	point _max_peak;
	point _max;
	std::list<point> _maxs;
	point _last_beat;

	LPFilter _lpf;
	KaiserWindowFilter _k;
	STFTStream _s1, _s2;
	double _dt_sum;
	int _dt_points;
	double _dt_avg;
	double _last_t;
};

#endif
