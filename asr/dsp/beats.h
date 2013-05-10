#ifndef _BEATS_H
#define _BEATS_H

#include <assert.h>

#include "fft.h"
#include "buffer.h"
#include "wavfile.h"

class FilterSourceImpl : public FilterSource<chunk_t>
{
public:
	FilterSourceImpl(T_source<chunk_t> *src)
		: FilterSource<chunk_t>(src),
		_ringBuffer(100000)
	{
	//	chunk_t *chk = _src->next();
		_bufferReadOfs = -10000;
		SamplePairf values[10000] = {{0.0,0.0}};
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
		T_sink_source<Chunk_T>(0),
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
		this->_src = src;
		_my_src.reset_source(this->_src);
		_s1.reset_source(&_my_src);
		_s2.reset_source(&_rectifier);

		_t = 0.0;
		_pos = true;
		_start = false;
		_x_max = 10.0;
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
        
        _bpm_list.clear();
        _final_bpm_list.clear();
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
    struct new_beat
    {
        new_beat() : bpm(0.0), t(0.0){}
        new_beat(double bpm_, double t_) : bpm(bpm_), t(t_) {}
        double bpm;
        double t;
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
			std::list<double>::iterator i = _final_bpm_list.begin();
			double last_t = *i;
			_beat_avg_list.push_back(last_t);
			i++;
			while (i != _final_bpm_list.end())
			{
				const double dt = *i - last_t;
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
					double diff = 0.1 * (*i - last_t);
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
    
    void analyze()
    {
        const double avg = 60.0/_dt_avg;
        const double stddev = avg * 0.1;
        printf("avg = %f stddev = %f\n", avg, stddev);
        
        double bpm = filter(avg, stddev);
        
        double final_sum = 0.0;
        int final_count = 0;
        for (typename std::list<new_beat>::iterator i = _bpm_list.begin();
             i != _bpm_list.end();
             i++)
        {
            const double dt = i->bpm;
            if (fabs(dt - bpm) / bpm < 0.1)
            {
                final_sum += dt;
                ++final_count;
                _final_bpm_list.push_back(i->t);
            }
        }
        printf("final avg %f\n", final_sum / final_count);
        _dt_avg = 60.0 / (final_sum / final_count);
        beats();
    }
    
    double filter(double avg, double stddev)
    {
        double sum = 0.0;
        double square_sum = 0.0;
        int valid_count = 0;
        for (typename std::list<new_beat>::iterator i = _bpm_list.begin();
             i != _bpm_list.end();
             i++)
        {
            const double dt = i->bpm;
            if (fabs(dt - avg) < stddev)
            {
                sum += dt;
                square_sum += dt*dt;
                ++valid_count;
            }
        }
        const double new_avg = sum / valid_count;
        const double square_avg = square_sum / valid_count;
        const double new_stddev = sqrt(square_avg - new_avg*new_avg);
        printf("filter avg = %f filter stddev = %f\n", new_avg, new_stddev);
        if (new_stddev < new_avg * 0.05)
            return new_avg;
        else
            return filter(new_avg, new_stddev);
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
					for (typename std::list<point>::iterator i = _peak_list.begin(); i != _peak_list.end(); i++)
					{
						if (i->dx > _max.dx && i->x > 0.15 * _x_max)
						{
							_max = *i;
							_maxs.push_back(_max);
						}
						else if (!_maxs.empty() && i->x < 0.05 * _x_max)//_max.x)
						{
							// pick first
							if (!_last_beat.valid && _dt_points > 25)
							{
								_last_beat.t = _maxs.begin()->t - _dt_avg;
								_last_beat.valid = true;
							}
							if (_last_beat.valid)
							{
								double dt = _maxs.begin()->t - _last_beat.t;
                                // todo revise this: pick first 5 better
								if (60.0/dt > 60.0 && 60.0/dt < 180.0 && (_dt_points < 25 || fabs(dt-_dt_avg) / _dt_avg < 0.1))
								{
									_dt_sum += dt;
									++_dt_points;
									_dt_avg = _dt_sum / _dt_points;
								}
                                _bpm_list.push_back(new_beat(60.0/dt, _last_beat.t));
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
    //    printf("xmax %f\n", _x_max);
		T_allocator<Chunk_T>::free(process_chk);
		return passthru_chk;
	}
    
    static void test_main()
    {
        T_source<chunk_t> *src = 0;
        BeatDetector<chunk_t> detector;
        std::string filenamestr;
        
        while (FileOpenDialog::OpenSingleFile(filenamestr))
        {
             std::cout << filenamestr << std::endl;
            const char *filename = filenamestr.c_str();
            if (strstr(filename, ".mp3") == filename + strlen(filename) - 4)
            {
                src = new mp3file_chunker<Chunk_T>(filename);
            }
            else if (strstr(filename, ".wav") == filename + strlen(filename) - 4)
            {
                src = new wavfile_chunker<Chunk_T>(filename);
                //src = new ifffile_chunker<Chunk_T>(filename);
            }
            else if (strstr(filename, ".flac") == filename + strlen(filename) - 5)
            {
                src = new flacfile_chunker<Chunk_T>(filename);
            }
            else
            {
                src = new ifffile_chunker<Chunk_T>(filename);
            }
            detector.reset_source(src);
            int chks = 0;
            while (detector.len().chunks == -1)
            {
                ++chks;
                T_allocator<chunk_t>::free(detector.next());
            }
            while (chks < detector.len().chunks)
            {
                ++chks;
                T_allocator<chunk_t>::free(detector.next());
            }
            detector.analyze();
            delete src;
            src = 0;
        }
        
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
    std::list<new_beat> _bpm_list;
    std::list<double> _final_bpm_list;
};

#endif
