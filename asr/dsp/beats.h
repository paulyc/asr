#ifndef _BEATS_H
#define _BEATS_H

#include <assert.h>

class FilterSourceImpl : public FilterSource<chunk_t>
{
public:
	FilterSourceImpl(T_source<chunk_t> *src)
		: FilterSource<chunk_t>(src),
		_ringBuffer(100000)
	{
		chunk_t *chk = _src->next();
		_bufferReadOfs = -10000;
		SamplePairf values[10000] = {0.0,0.0};
		_ringBuffer.write(values, 10000);
		_ringBuffer.write(chk->_data, chunk_t::chunk_size);
		
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
	BeatDetector(T_source<Chunk_T> *src) : T_sink_source(src) 
	{
		_my_src = new QueueingSource<Chunk_T>(src);
		_filterSource2 = new FilterSourceImpl(_my_src);
		_lpf1 = new lowpass_filter(_filterSource2, 100.0, 44100.0);
		_rectifier = new full_wave_rectifier<SamplePairf, Chunk_T>(_lpf1); 
		_filterSource = new FilterSourceImpl(_rectifier);
		_lpf2 = new lowpass_filter(_filterSource, 20.0, 44100.0);
		_passthrough_sink = new T_sink_source<Chunk_T>(_lpf2);
		_d_dt_max[0] = 0.0f;
		_d_dt_max[1] = 0.0f;
		_t_max[0] = 0.0;
		_t_max[1] = 0.0;
		_d_t_max_sum[0] = 0.0;
		_d_t_max_sum[1] = 0.0;
		_t_max_last[0] = 0.0;
		_t_max_last[1] = 0.0;
		_t_max_points = 0;
		_t = 0.0;
		_t_points = 0;
		_pos = true;
		_start = false;
		_x_max = 0.0;
		_chk_ofs = 0;
	}

	~BeatDetector()
	{
		delete _lpf2;
		delete _lpf1;
		delete _filterSource;
		delete _filterSource2;
		delete _rectifier;
		delete _my_src;
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
	
/*
	Chunk_T *next()
	{
		Chunk_T *process_chk = _passthrough_sink->next();
		Chunk_T *passthru_chk = _my_src->get_next();
		
		SamplePairf last = {0.0f, 0.0f};
		for (SamplePairf *smp = process_chk->_data, *end = smp + Chunk_T::chunk_size; smp != end; ++smp)
		{
			double x = (*smp)[0];
			/*if (x > _x_max)
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

					++_t_points;
					
					double d_tmax0 = _t_max[0] - _t_max_last[0];
					double d_tmax1 = _t_max[1] - _t_max_last[1];
					_t_max_last[0] = _t_max[0];
					_t_max_last[1] = _t_max[1];
					_d_dt_max[0] = 0.0f;
					_d_dt_max[1] = 0.0f;
				//	printf("d_dtmax %f %f dt_max %f %f\n", _d_dt_max[0], _d_dt_max[1], d_tmax0, d_tmax1);

					// good point of beat
						if (_t_points > 1) // dont include first in average
						{
							++_t_max_points;
							_d_t_max_sum[0] += d_tmax0;
							_d_t_max_sum[1] += d_tmax1;
						//	printf("avg d/t_max %f %f now d_t_max %f %f\n", _d_t_max_sum[0] / _t_max_points, _d_t_max_sum[1] / _t_max_points, d_tmax0, d_tmax1);
							printf("beat at t=%f bpm = %f avg = %f\n", _t_max[0], 60.0/(d_tmax0), 60.0/(_d_t_max_sum[0] / _t_max_points));
						}

						//_d_dt_max[0] = 0.0;
				}
			//	continue;
			}
			else
			{
				_start = true;
				_d.next((*smp)[0]);
				float d_dt = _d.dx();
				if (d_dt > _d_dt_max[0])
				{
					_d_dt_max[0] = d_dt;
					_t_max[0] = _t - 2.0 / 44100.0;
				}
				_d2.next((*smp)[1]);
				d_dt = _d2.dx();
				if (d_dt > _d_dt_max[1])
				{
					_d_dt_max[1] = d_dt;
					_t_max[1] = _t - 2.0 / 44100.0;
				}
			}
			_t += 1.0 / 44100.0;
		}
		//	avg[0] /= Chunk_T::chunk_size;
	//	avg[1] /= Chunk_T::chunk_size;
	//	printf("avg values of envelope %f %f\n", avg[0], avg[1]);
		T_allocator<Chunk_T>::free(passthru_chk);

		return process_chk;
	}
	Differentiator _d, _d2;
	*/

	std::list<point> _peak_list;
	std::list<point> _beat_list;

	const std::list<point>& beats()
	{
		return _beat_list;
	}

	Chunk_T *next()
	{
		Chunk_T *process_chk = _passthrough_sink->next();
		Chunk_T *passthru_chk = _my_src->get_next();
		
		SamplePairf last = {0.0f, 0.0f};
		for (SamplePairf *smp = process_chk->_data, *end = smp + Chunk_T::chunk_size; smp != end; ++smp)
		{
			double x = (*smp)[0];
			_diff.next(x);
			double dx = _diff.dx();
			_diff2.next(dx);
			double ddx = _diff2.dx();
			(*smp)[0] = x;//3000*dx;
			(*smp)[1] = x;//3000*dx;
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
						//	point m = *_maxs.begin();
							if (_last_beat.valid)
							{
								double dt = _maxs.begin()->t - _last_beat.t;
								printf("bpm %f\n", 60.0/dt);
							}
							_last_beat = *_maxs.begin();
							_beat_list.push_back(*_maxs.begin());
							if (_beat_list.size() == 9)
							{
								printf("hello\n");
							}
							_max = point();
							_maxs.clear();
						}
						
					}
					_peak_list.clear();
					/*
					if (max.valid)
					{
						
						printf("beat at t=%f x=%f dx=%f ", max.t, max.x, max.dx);
						
						if (_max_last.valid)
						{
							double dt =  max.t - _max_last.t;
							double bpm_last =60.0/(dt);
							if (_t_max_points > 5)
							{
								double dt_avg = _d_t_max_sum[0]/_t_max_points;
								if (abs(dt_avg - dt) <= 0.05)
								{
									_d_t_max_sum[0] += dt;
									_t_max_points++;
									dt_avg = _d_t_max_sum[0]/_t_max_points;
									double bpm_avg = 60.0/(dt_avg);
									printf(" dt=%f bpm=%f avg=%f\n", dt, bpm_last, bpm_avg);
									_max_last = max;
									_beat_list.push_back(max);
								}
								else
								{
									printf("rejected %f\n", 60.0/dt);
								}
							}
							else
							{
								_d_t_max_sum[0] += dt;
								_t_max_points++;
								double dt_avg = _d_t_max_sum[0]/_t_max_points;
								double bpm_avg = 60.0/(dt_avg);
								printf(" dt=%f bpm=%f avg=%f\n", dt, bpm_last, bpm_avg);
							//	_max_last = max;
								_beat_list.push_back(max);
							}
						//	_d_t_max_sum[0] += dt;
						//	_t_max_points++;
						//	bpm_avg = 60.0/(_d_t_max_sum[0]/_t_max_points);
						//	printf(" new avg bpm=%f\n", bpm_avg);
						}
						else
						{
							printf(" no last\n");
						//	_max_last = max;
							_beat_list.push_back(max);
						}
						_max_last = max;
					}
					//_d_dt_max[0] = 0.0;
					*/
				}
			//	continue;
			}
			else if (x > 0.2 * _x_max)
			{
				_start = true;
			}

/*
			if (ddx > 0.0)
			{
				if (!_pos)
				{
				//	printf("max dx = %f, x = %f, t = %f\n", dx, x, _t);
				//	if (x < 0.1*_x_max)
					//if (_start)
						
				//	if (dx > _d_dt_max[0])
				//		_d_dt_max[0] = dx;

					_pos = !_pos;
				}
				
			}
			else
			{
				if (_pos) // min
				{
				//	printf("min dx = %f, x = %f, t = %f\n", dx, x, _t);
					_peak_list.push_back(point(_t-2.0/44100.0, x, dx));
					_pos = !_pos;
				}
			}*/

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
		T_allocator<Chunk_T>::free(passthru_chk);

		return process_chk;
	}

	void set_source(T_source<Chunk_T> *src)
	{
		_src = src;
	}
private:
	SamplePairf _d_dt_max;
	double _t;
	SamplePaird _t_max_last;
	SamplePaird _t_max;
	SamplePaird _d_t_max_sum;
	int _t_max_points;
	QueueingSource<Chunk_T> *_my_src;
	T_sink_source<Chunk_T> *_passthrough_sink;
	full_wave_rectifier<SamplePairf, Chunk_T> *_rectifier;
	FilterSourceImpl *_filterSource, *_filterSource2;
	lowpass_filter *_lpf1,*_lpf2;
	bool _start, _pos;
	int _t_points;
	Differentiator _diff, _diff2;
	double _x_max;
	double _dx_max;
	point _max_last;
	point _max_peak;
	point _max;
	std::list<point> _maxs;
	point _last_beat;
};

#endif
