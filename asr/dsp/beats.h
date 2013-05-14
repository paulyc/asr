#ifndef _BEATS_H
#define _BEATS_H

#include <assert.h>
#include <dirent.h>
#include <sys/types.h>

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
		_rectifier(_s1),
		_passthrough_sink(_s2),
        _diff(10),
        _lpf(2048, 44100.0, 100.0),
        _bpf(2048, 44100.0, 20.0, 200.0),
        _kf(1024),
        _jobs(NUM_JOBS),
        _running(true)
	{
        _lpf.init();
        _bpf.init();
        _kf.init();
        
#if PARALLEL_PROCESS
        for (int i=0; i<NUM_JOBS; ++i)
        {
            _jobs[i] = new job(_lpf, _kf);
            spin_processor(i);
        }
#else
        _s1 = new STFTStream(_lpf, 2048, 1024, 20);
		_s2 = new STFTStream(_kf, 1024, 512, 20);
#endif
	}
    
    ~BeatDetector()
    {
#if PARALLEL_PROCESS
        _proc_lock.acquire();
        _running = false;
        for (int i=0; i<NUM_JOBS; ++i)
        {
            _do_proc.signal();
        }
        _proc_lock.release();
        
        void *ret;
        for (int i=0; i<NUM_JOBS; ++i)
        {
            pthread_join(_jobs[i]->thread, &ret);
            delete _jobs[i];
        }
#else
        delete _s1;
		delete _s2;
#endif
    }

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
    
    double analyze()
    {
     /*   double square_sum = 0.0;
        int square_count = 0;
        for (typename std::list<new_beat>::iterator i = _bpm_list.begin();
             i != _bpm_list.end();
             i++)
        {
            square_sum += i->bpm * i->bpm;
            ++square_count;
        }*/
        const double avg = 60.0/_dt_avg;
        const double stddev = avg * 0.1; //sqrt(square_sum/square_count - avg*avg);//avg * 0.1;
     //   printf("avg = %f stddev = %f\n", avg, stddev);
        
        double bpm = filter(avg, stddev, 1);
        
        double final_sum = 0.0;
        int final_count = 0;
        for (typename std::list<new_beat>::iterator i = _bpm_list.begin();
             i != _bpm_list.end();
             i++)
        {
            const double dt = i->bpm;
            if (fabs(dt - bpm) / bpm < 0.2)
            {
                final_sum += dt;
                ++final_count;
                _final_bpm_list.push_back(i->t);
            }
        }
        const double final_bpm = final_sum / final_count;
      //  printf("final avg %f\n", final_bpm);
        _dt_avg = 60.0 / final_bpm;
        beats();
        return final_bpm;
    }
    
    double filter(double avg, double stddev, int count)
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
        if (new_stddev < new_avg * 0.05 || count >= 10)
            return new_avg;
        else
            return filter(new_avg, new_stddev, count+1);
    }
    
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
    
    void process_all_from_source(T_source<Chunk_T> *src)
    {
        std::vector<Chunk_T*> chunks;
        chunks.reserve(40000);
        
        reset_source(src);
        while (this->len().chunks == -1)
        {
            chunks.push_back(this->_src->next());
        }
        
        if (chunks.size() == this->len().chunks)
        {
            // all have been loaded
            const int chks_to_process = this->len().chunks;
            const int division_size = chks_to_process / 4;
            const int division_rem = chks_to_process % 4;
            _jobs[0]->init(division_size);
            _jobs[1]->init(division_size);
            _jobs[2]->init(division_size);
            _jobs[3]->init(division_size+division_rem);
            int i = 0;
            for (int indx = 0; indx < 4; ++indx)
            {
                for (int chk = 0; chk < _jobs[indx]->vecSrc->size(); ++chk)
                {
                    _jobs[indx]->vecSrc->add(chunks[i++]);
                }
                this->process_job(indx);
            }
        }
        else if (chunks.size() == 0)
        {
            // we know the length and havent loaded them
            const int chks_to_process = this->len().chunks;
            const int division_size = chks_to_process / 4;
            const int division_rem = chks_to_process % 4;
            _jobs[0]->init(division_size);
            _jobs[1]->init(division_size);
            _jobs[2]->init(division_size);
            _jobs[3]->init(division_size+division_rem);
            for (int indx = 0; indx < 4; ++indx)
            {
                _jobs[indx]->lock.acquire();
                for (int chk = 0; chk < _jobs[indx]->vecSrc->size(); ++chk)
                {
                    _jobs[indx]->vecSrc->add(this->_src->next());
                }
                _jobs[indx]->lock.release();
                this->process_job(indx);
            }
        }
        else
        {
            printf("WTF happened?\n");
            throw std::exception();
        }
        
        _jobs[0]->wait();
        for (int i=0; i<_jobs[0]->outputs.size(); ++i)
            process_chunk(_jobs[0]->outputs[i]);
        _jobs[1]->wait();
        for (int i=0; i<_jobs[1]->outputs.size(); ++i)
            process_chunk(_jobs[1]->outputs[i]);
        _jobs[2]->wait();
        for (int i=0; i<_jobs[2]->outputs.size(); ++i)
            process_chunk(_jobs[2]->outputs[i]);
        _jobs[3]->wait();
        for (int i=0; i<_jobs[3]->outputs.size(); ++i)
            process_chunk(_jobs[3]->outputs[i]);
        
        std::cout << analyze() << std::endl;
    }
    
    
    
    void process_chunk(Chunk_T *process_chk)
    {
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
								const double dt = _maxs.begin()->t - _last_beat.t;
                                const double bpm = 60.0/dt;
                                // todo revise this: pick first 5 better
								if (bpm > 80.0 && bpm < 180.0 && (_dt_points < 25 || fabs(dt-_dt_avg) / _dt_avg < 0.1))
								{
									_dt_sum += dt;
									++_dt_points;
									_dt_avg = _dt_sum / _dt_points;
								}
                                _bpm_list.push_back(new_beat(bpm, _last_beat.t));
                            //    printf("bpm %f avg %f\n", bpm, 60.0/_dt_avg);
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
    }

	Chunk_T *next()
	{
		Chunk_T *process_chk = _passthrough_sink.next();
		Chunk_T *passthru_chk = _my_src.get_next();
		
        process_chunk(process_chk);
		
		return passthru_chk;
	}
    
    static void test_main()
    {
        T_source<chunk_t> *src = 0;
        BeatDetector<chunk_t> detector;
        std::string filenamestr;
        dirent *entry;
        
        char filename[256];
        
        const char *dir_name = "/Users/paulyc/Downloads/";
        
      //  while (FileOpenDialog::OpenSingleFile(filenamestr))
        DIR *d = opendir(dir_name);
        while ((entry = readdir(d)) != NULL)
        {
            
            strcpy(filename, dir_name);
            strcat(filename, entry->d_name);
         //   strcpy(filename, filenamestr.c_str());
            
            if (strstr(filename, ".mp3") == filename + strlen(filename) - 4)
            {
                continue;
                src = new mp3file_chunker<Chunk_T>(filename);
            }
            else if (strstr(filename, ".wav") == filename + strlen(filename) - 4)
            {
                src = new wavfile_chunker<Chunk_T>(filename);
                //src = new ifffile_chunker<Chunk_T>(filename);
            }
            else if (strstr(filename, ".flac") == filename + strlen(filename) - 5)
            {
                continue;
                src = new flacfile_chunker<Chunk_T>(filename);
            }
            else if (strstr(filename, ".aiff") == filename + strlen(filename) - 5)
            {
                src = new ifffile_chunker<Chunk_T>(filename);
            }
            else
            {
                continue;
            }
            
        //   std::cout << filename << " ";
            
#if PARALLEL_PROCESS
            detector.process_all_from_source(src);
#else
            
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
            std::cout <<  detector.analyze() << std::endl;
#endif
            delete src;
            src = 0;
         //   break;
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

#endif
