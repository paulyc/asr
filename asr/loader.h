#ifndef _LOADER_H
#define _LOADER_H

#include <queue>

#include <pthread.h>

#include "track.h"

class DataLoader
{
protected:
	pthread_t _tid;
	pthread_mutex_t _lock;
	pthread_cond_t _job_rdy;

	struct job
	{
		virtual void do_it() = 0;
	};

	struct load_track_job : public job
	{
		load_track_job(track_t *t):track(t){}
		track_t *track;
		void do_it()
		{
			track->_display->set_wav_heights();
		}
	};

	std::queue<job*> _jobs;
public:
	typedef SeekablePitchableFileSource<chunk_t> track_t;

	void spin()
	{
		pthread_mutex_init(&_lock, 0);
		pthread_cond_init(&_job_rdy, 0);
		pthread_create(&_tid, attr, threadproc, (void*)this);
	}

	void load_track_data(track_t *track)
	{
	//	pthread_mutex_lock
		_jobs.push(new load_track_job(track));
	//	pthread_cond_
	}

	static void threadproc(void *arg)
	{
		DataLoader *instance = (DataLoader*)arg;
		pthread_cond_wait(instance->_job_rdy, instance->_lock);
	}
};

#endif // !defined(LOADER_H)
