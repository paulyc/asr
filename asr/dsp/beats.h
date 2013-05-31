#ifndef _BEATS_H
#define _BEATS_H

#include <assert.h>
#include <dirent.h>
#include <sys/types.h>

#include "fft.h"
#include "buffer.h"
#include "wavfile.h"

template <typename T>
class LengthFindingSource : public T_sink_source<T>
{
public:
    LengthFindingSource(T_source<T> *src) : T_sink_source<T>(src), _readIndx(0)
    {
        if (this->_src->len().chunks != -1)
        {
            const int chks = this->_src->len().chunks;
            _chunks = new std::vector<T*>(chks);
            for (int i=0; i<chks; ++i)
            {
                _chunks->operator[](i) = this->_src->next();
            }
        }
        else
        {
            _chunks = new std::vector<T*>;
            _chunks->reserve(40000);
            while (this->_src->len().chunks == -1)
            {
                _chunks->push_back(this->_src->next());
            }
        }
    }
    
    ~LengthFindingSource()
    {
        for (int i=0; i<_chunks->size(); ++i)
        {
            T_allocator<T>::free(_chunks->operator[](i));
        }
        delete _chunks;
    }
    
    T *next()
    {
        T *chk = _chunks->operator[](_readIndx++);
        T_allocator<T>::add_ref(chk);
        return chk;
    }
    
    void reset_ptr()
    {
        _readIndx = 0;
    }
    
private:
    std::vector<T*> *_chunks;
    int _readIndx;
};

template <typename Chunk_T>
class BeatDetector : public T_sink_source<Chunk_T>
{
public:
	BeatDetector();
    ~BeatDetector();

	void reset_source(T_source<Chunk_T> *src, Lock_T *lock)
	{
		//this->_src = src;
#if !PARALLEL_PROCESS
		_my_src.reset_source(this->_src);
		_s1.reset_source(&_my_src);
		_s2.reset_source(&_rectifier);
#endif

		reset_stats();
        process_all_from_source(src, lock);
	}
    
    void reset_stats()
    {
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
			//	printf("chk %d\n", _chk_ofs);
			}
		}
		else
		{
			printf("hello\n");
		}
	}
    
    void calc_beats()
    {
        std::list<double>::iterator i = _final_bpm_list.begin();
        double last_t = *i;
        
        while (last_t > 0.0)
        {
            last_t -= _dt_avg;
            _final_bpm_list.push_front(last_t);
        }
        i = _final_bpm_list.begin();
        last_t = *i;
        _beat_avg_list.push_back(last_t);
        i++;
        while (i != _final_bpm_list.end())
        {
            const double dt = *i - last_t;
            if (dt > 1.5*_dt_avg)
            {
                last_t += _dt_avg;
                //					printf("bpm = %f\n", 60.0/_dt_avg);
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
                //					printf("bpm = %f\n", 60.0/(_dt_avg+diff));
                _beat_avg_list.push_back(last_t);
                i++;
            }
        }
        for (int j=0; j<50; ++j)
        {
            //				printf("bpm = %f\n", 60.0/_dt_avg);
            last_t += _dt_avg;
            _beat_avg_list.push_back(last_t);
        }
    }

	const std::vector<double>& beats()
	{
		return _beat_avg_list;
	}
    
    double analyze();
    double filter(double avg, double stddev, int count);
    
    const static int NUM_JOBS = 4;
    
    struct process_beats_job : public Worker::job
    {
        process_beats_job(const FFTWindowFilter &f1, const FFTWindowFilter &f2) :
            _vecSrc(0),
            _s1(f1, 2048, 1024, 20),
            _s2(f2, 1024, 512, 20),
            _rectifier(&_s1)
        {
        }
        void reset_source(T_source<Chunk_T> *_src, int nChunks, Lock_T *lock)
        {
            _lock.acquire();
            delete _vecSrc;
            _vecSrc = new VectorSource<Chunk_T>(nChunks);
            _outputs.resize(nChunks);
            _guardLock = lock;
            for (int i=0; i<nChunks; ++i)
            {
                if (i%10==0)
                {
                    CRITICAL_SECTION_GUARD(_guardLock);
                }
                Chunk_T *chk = _src->next();
                _vecSrc->add(chk);
            }
            _s1.reset_source(_vecSrc);
            _s2.reset_source(&_rectifier);
            _lock.release();
        }
        void do_it()
        {
            _lock.acquire();
            for (int i=0; i<_vecSrc->size(); ++i)
            {
                if (i%10==0)
                {
                    CRITICAL_SECTION_GUARD(_guardLock);
                }
                _outputs[i] = _s2.next();
            }
            
            done = true;
            _done.signal();
            _lock.release();
        }
        void step()
        {
            do_it();
        }
        void wait_for()
        {
            _lock.acquire();
            while (!done)
            {
                _done.wait(_lock);
            }
            _lock.release();
        }
        VectorSource<Chunk_T> *_vecSrc;
        STFTStream _s1, _s2;
        full_wave_rectifier<SamplePairf, Chunk_T> _rectifier;
        std::vector<Chunk_T*> _outputs;
        
        Lock_T _lock;
        Condition_T _done;
        Lock_T *_guardLock;
    };
    
    void process_all_from_source(T_source<Chunk_T> *src, Lock_T *lock=0);
    void process_chunk(Chunk_T *process_chk);

	Chunk_T *next()
	{
		/*Chunk_T *process_chk = _passthrough_sink.next();
		Chunk_T *passthru_chk = _my_src.get_next();
		
        process_chunk(process_chk);
		
		return passthru_chk;*/
    //    return _thissrc->next();
        return 0;
	}
    
  //  virtual typename T_source<Chunk_T>::pos_info& len()
//	{
//		return T_source<Chunk_T>::pos_info();
//	}
    
    static void test_main();

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
    BPFilter _bpf;
    KaiserWindowFilter _kf;
    
	STFTStream *_s1, *_s2;
	double _dt_sum;
	int _dt_points;
	double _dt_avg;
	double _last_t;
    std::list<new_beat> _bpm_list;
    std::list<double> _final_bpm_list;
    
    bool _running;
    process_beats_job *_jobs[NUM_JOBS];
};

#include "beats.cpp"

#endif
