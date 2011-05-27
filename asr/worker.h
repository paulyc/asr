#ifndef _LOADER_H
#define _LOADER_H

#include <queue>

#include <pthread.h>

//#include "track.h"

class Worker
{
	//friend class Track;
public:
	static Worker *_instance;
	Worker()
	{
		pthread_mutex_init(&_job_lock, 0);
		pthread_cond_init(&_job_rdy, 0);
		pthread_cond_init(&_job_done, 0);
	}
	~Worker()
	{
		pthread_cond_destroy(&_job_done);
		pthread_cond_destroy(&_job_rdy);
		pthread_mutex_destroy(&_job_lock);
	}

	pthread_t _tid;
	pthread_mutex_t _job_lock;
	pthread_cond_t _job_rdy;
	pthread_cond_t _job_done;

	struct job
	{
		job():done(false){}
		bool done;
		virtual void do_it(){}
		virtual void step(){}
	};

	/*template <typename Source_T>
	struct generate_chunk_job : public job
	{
		Source_T *src;
		Source_T::chunk_t *result;
	};*/

	template <typename Track_T>
	struct load_track_job : public job
	{
		load_track_job(Track_T *t):track(t){
			pthread_mutex_init(&_l, 0);
			pthread_cond_init(&_next_step, 0);
		}
		Track_T *track;
		//const wchar_t *file;
		//void do_it()
		//{
		//	track->set_source_file(file);
		//	done = true;
		//}
		pthread_mutex_t _l;
		pthread_cond_t _next_step;
		void step()
		{
			pthread_mutex_lock(&track->_config_lock);
			if (track->len().samples < 0)
			{
				track->_src_buf->load_next();
				pthread_mutex_unlock(&track->_config_lock);
				return;
			}
			if (!track->_display->set_next_height())
			{
				done = true;
				track->render();
				printf("job done %p\n", this);
			}
			pthread_mutex_unlock(&track->_config_lock);
		}
	};

	std::queue<job*> _critical_jobs;
	std::queue<job*> _idle_jobs;
public:
	static Worker* get()
	{
		if (!_instance)
		{
			_instance = new Worker;
			_instance->spin();
		}
		return _instance;
	}
	//typedef SeekablePitchableFileSource<chunk_t> track_t;

	void spin()
	{
		pthread_create(&_tid, 0, threadproc, (void*)this);
	}

	void do_job(job *j, bool sync=false, bool critical=false)
	{
		printf("doing job %p\n", j);
		pthread_mutex_lock(&_job_lock);
		if (critical)
			_critical_jobs.push(j);
		else
			_idle_jobs.push(j);

		pthread_cond_signal(&_job_rdy);
		if (sync)
			while (!j->done)
				pthread_cond_wait(&_job_done, &_job_lock);
		pthread_mutex_unlock(&_job_lock);
	}

	static void* threadproc(void *arg)
	{
		static_cast<Worker*>(arg)->thread();
		return 0;
	}

	void thread()
	{
		job *id_j=0, *cr_j=0;
		while (true)
		{
			pthread_mutex_lock(&_job_lock);
			while (!_critical_jobs.empty())
			{
				cr_j = _critical_jobs.front();
				_critical_jobs.pop();
				pthread_mutex_unlock(&_job_lock);
				cr_j->do_it();
				delete cr_j;
				cr_j = 0;
				pthread_cond_signal(&_job_done);
				pthread_mutex_lock(&_job_lock);
			}
			
			if (id_j)
			{
				pthread_mutex_unlock(&_job_lock);
				id_j->step();
				if (id_j->done)
				{
					delete id_j;
					id_j = 0;
					pthread_cond_signal(&_job_done);
				}
			}
			else 
			{
				if (!_idle_jobs.empty())
				{
					id_j = _idle_jobs.front();
					_idle_jobs.pop();
					pthread_mutex_unlock(&_job_lock);
				}
				else
				{
					pthread_cond_wait(&_job_rdy, &_job_lock);
					pthread_mutex_unlock(&_job_lock);
				}
			}
		}
	}
};

#endif // !defined(LOADER_H)
