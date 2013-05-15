#ifndef _BEATS_H
#define _BEATS_H

#include <assert.h>
#include <dirent.h>
#include <sys/types.h>

#include "fft.h"
#include "buffer.h"
#include "wavfile.h"

template <typename Chunk_T>
class BeatDetector : public T_sink_source<Chunk_T>
{
public:
	BeatDetector();
    ~BeatDetector();

	void reset_source(T_source<Chunk_T> *src)
	{
		this->_src = src;
#if !PARALLEL_PROCESS
		_my_src.reset_source(this->_src);
		_s1.reset_source(&_my_src);
		_s2.reset_source(&_rectifier);
#endif

		reset_stats();
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
			for (int j=0; j<10; ++j)
			{
//				printf("bpm = %f\n", 60.0/_dt_avg);
				last_t += _dt_avg;
				_beat_avg_list.push_back(last_t);
			}
		}
		return _beat_avg_list;
	}
    
    double analyze();
    double filter(double avg, double stddev, int count);
    
    struct job {
        job(const FFTWindowFilter &f1, const FFTWindowFilter &f2) :
        _done(false),
	//	_s1(new LPFilter(2048, 44100.0, 100.0), 2048, 1024, 20),
        _s1(f1, 2048, 1024, 20),
		_s2(f2, 1024, 512, 20),
        _rectifier(&_s1)
        {
        }
        
        // 65536 * 8 = 524kb / filter
        // comb filter 130 => 60/130 * 2 * 44100 = 40707
        //             129 =>                      41023
        
        void do_filter()
        {
            for (int i=0; i<vecSrc->size(); ++i)
            {
                outputs.push_back(_s2.next());
            }
        }
        
        void init(int n) {
            lock.acquire();
            vecSrc = new VectorSource<Chunk_T>(n);
            lock.release();
        }
        
        void process() {
            lock.acquire();
            
            outputs.clear();
            _s1.reset_source(vecSrc);
            _s2.reset_source(&_rectifier);
            do_filter();
            
            delete vecSrc;
            vecSrc = 0;
            _done = true;
            job_done.signal();
            lock.release();
        }
        
        void wait()
        {
            lock.acquire();
            while (!_done)
                job_done.wait(lock);
            lock.release();
        }

        bool _done;
        VectorSource<Chunk_T> *vecSrc;
        STFTStream _s1, _s2;
        full_wave_rectifier<SamplePairf, Chunk_T> _rectifier;
        
        Condition_T job_done;
        Lock_T lock;
        std::vector<Chunk_T*> outputs;
        pthread_t thread;
    };
    const static int NUM_JOBS = 4;
    
    Lock_T _proc_lock;
    Condition_T _do_proc;
    
    void spin_processor(int j)
    {
        pthread_create(&_jobs[j]->thread, NULL, processor_thread, this);
    }
    
    static void* processor_thread(void* p)
    {
        ((BeatDetector<Chunk_T>*)p)->do_processing();
        return 0;
    }
    
    std::queue<int> _jobs_to_process;
    
    void process_job(int indx)
    {
        _proc_lock.acquire();
        _jobs_to_process.push(indx);
        _do_proc.signal();
        _proc_lock.release();
    }
    
    void do_processing()
    {
        while (true)
        {
            int j;
            _proc_lock.acquire();
            while (_running && _jobs_to_process.empty())
                _do_proc.wait(_proc_lock);
            j = _jobs_to_process.front();
            _jobs_to_process.pop();
            _proc_lock.release();
            this->_jobs[j]->process();
        }
    }
    
    void process_all_from_source(T_source<Chunk_T> *src);
    void process_chunk(Chunk_T *process_chk);

	Chunk_T *next()
	{
		Chunk_T *process_chk = _passthrough_sink.next();
		Chunk_T *passthru_chk = _my_src.get_next();
		
        process_chunk(process_chk);
		
		return passthru_chk;
	}
    
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
    
    std::vector<job*> _jobs;
    bool _running;
    
};

#include "beats.cpp"

#endif
