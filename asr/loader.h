#ifndef _LOADER_H
#define _LOADER_H

#include <queue>

#include <pthread.h>

#include "track.h"

class Worker
{
protected:
	static Worker *_instance;
	Worker()
	{
		pthread_mutex_init(&_job_lock, 0);
		pthread_cond_init(&_job_rdy, 0);
	}
	~Worker()
	{
		pthread_mutex_destroy(&_job_lock);
		pthread_cond_destroy(&_job_rdy);
	}

	pthread_t _tid;
	pthread_mutex_t _job_lock;
	pthread_cond_t _job_rdy;
	pthread_cond_t _job_done;

	struct job
	{
		job():done(false){}
		bool done;
		virtual void do_it() = 0;
	};

	template <typename Source_T>
	struct generate_chunk_job : public job
	{
		Source_T *src;
		Source_T::chunk_t *result;
	};

	struct load_track_job : public job
	{
		load_track_job(track_t *t, const wchar_t *f):track(t){}
		track_t *track;
		const wchar_t *file;
		void do_it()
		{
			track->set_source_file(file);
			done = true;
		}
	};

	std::queue<job*> _critical_jobs;
	std::queue<job*> _idle_jobs;
public:
	Worker* get()
	{
		if (!_instance)
		{
			_instance = new Worker;
		}
		return _instance;
	}
	typedef SeekablePitchableFileSource<chunk_t> track_t;

	void spin()
	{
		pthread_create(&_tid, attr, threadproc, (void*)this);
	}

	void load_track_async(track_t *track, const wchar_t *file)
	{
		job *j = new load_track_job(track, file);
		pthread_mutex_lock(&_job_lock);
		_jobs.push(j);
		pthread_cond_signal(&_job_rdy);
	//	while (!j->done)
	//		pthread_cond_wait(&_job_done, &_job_lock);
	}

	static void threadproc(void *arg)
	{
		dynamic_cast<Worker*>(arg)->thread();
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
				cr_j = 0;
				pthread_cond_signal(&_job_done);
				pthread_mutex_lock(&_job_lock);
			}
			
			if (id_j)
			{
				pthread_mutex_unlock(&_job_done);
				id_j->step();
				if (id_j->done)
				{
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
				}
			}
		}
	}
};

#endif // !defined(LOADER_H)
