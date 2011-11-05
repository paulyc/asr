#ifndef _LOADER_H
#define _LOADER_H

#include <queue>

#include <pthread.h>

//#include "track.h"
#include "util.h"

class Worker
{
	//friend class Track;
public:
	static Worker *_instance;
	Worker() :
		_blockOnCritical(true)
	{
		
	}
	~Worker()
	{
		
	}

	static void init()
	{
		pthread_mutex_init(&_job_lock, 0);
		pthread_cond_init(&_cr_job_rdy, 0);
		pthread_cond_init(&_job_rdy, 0);
		pthread_cond_init(&_job_done, 0);

		(new Worker)->spin(true);
		(new Worker)->spin(false);
		(new Worker)->spin(false);
		(new Worker)->spin(false);
		(new Worker)->spin(false);
	}

	static void destroy()
	{
		pthread_cond_destroy(&_job_done);
		pthread_cond_destroy(&_job_rdy);
		pthread_cond_destroy(&_cr_job_rdy);
		pthread_mutex_destroy(&_job_lock);
	}

	pthread_t _tid;

	struct job
	{
		job():done(false){}
		bool done;
		bool _deleteme;
		const char *_name;
		virtual void do_it(){}
		virtual void step(){}
		pthread_t _thread;
	};

	template <typename Source_T>
	struct generate_chunk_job : public job
	{
		Source_T *src;
		typename Source_T::chunk_t **result;
		pthread_mutex_t *lock;
		generate_chunk_job(Source_T *s, 
			typename Source_T::chunk_t **r, pthread_mutex_t *l) :
			src(s), result(r), lock(l) {_name="generate chunk";}

		void do_it()
		{
			*result = src->next();
		}
	};

	template <typename Obj_T>
	struct callback_job : public job
	{
		functor0<Obj_T> func;
		callback_job(functor0<Obj_T> f) : func(f) {}
		void do_it()
		{
			func();
		}
	};

	template <typename Obj_T>
	struct callback_th_job : public job
	{
		functor1<Obj_T, pthread_t> func;
		callback_th_job(functor1<Obj_T, pthread_t> f) : func(f) {}
		void do_it()
		{
			func(_thread);
		}
	};

	template <typename Obj_T>
	struct callback2_job : public job
	{
		deferred *def;
		callback2_job(deferred *d) : def(d) {}
		void do_it()
		{
			(*d)();
			delete d;
		}
	};

	template <typename Obj_T>
	struct call_deferreds_job : public job
	{
		call_deferreds_job(Obj_T *obj) : _object(obj) {}
		void step()
		{
			_object->call_deferreds_loop(_thread);
		}
		Obj_T *_object;
	};

	template <typename Track_T>
	struct load_track_job : public job
	{
		load_track_job(Track_T *t, pthread_mutex_t *l):track(t),_lock(l){
			_name="load track";
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
		pthread_mutex_t *_lock;
		pthread_cond_t _next_step;
		void step()
		{
			if (!track->load_step(_lock))
			{
				done = true;
				printf("job done %p\n", this);
			}
		}
	};

	template <typename Track_T>
	struct draw_waveform_job : public job
	{
		draw_waveform_job(Track_T *t, pthread_mutex_t *l):track(t),_lock(l){
			_name="draw wave";
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
		pthread_mutex_t *_lock;
		pthread_cond_t _next_step;
		void step()
		{
			track->draw_if_loaded();
			done = true;
		}
	};

	static pthread_mutex_t _job_lock;
	static pthread_cond_t _cr_job_rdy;
	static pthread_cond_t _job_rdy;
	static pthread_cond_t _job_done;

	static std::queue<job*> _critical_jobs;
	static std::queue<job*> _idle_jobs;
	static pthread_once_t once_control; 
	bool _blockOnCritical;

	void spin(bool critical=false)
	{
#if 1
		if (critical) pthread_create(&_tid, 0, threadproc_cr, (void*)this);
		else pthread_create(&_tid, 0, threadproc_id, (void*)this);
#else
		pthread_create(&_tid, 0, threadproc, (void*)this);
#endif
	}

public:
	static void do_job(job *j, bool sync=false, bool critical=false)
	{
		pthread_once(&once_control, init);
	//	printf("doing job %p (%s) sync %d critical %d\n", j, j->_name, sync, critical);
		pthread_mutex_lock(&_job_lock);
		j->_deleteme = !sync;
		if (critical)
		{
			_critical_jobs.push(j);
			pthread_cond_signal(&_cr_job_rdy);
		}
		else
			_idle_jobs.push(j);

		pthread_cond_signal(&_job_rdy);
		if (sync)
		{
			while (!j->done)
			{
				printf("waiting on %p\n", j);
				pthread_cond_wait(&_job_done, &_job_lock);
				printf("done %p\n", j);
			}
			delete j;
		}
		pthread_mutex_unlock(&_job_lock);
	}

protected:
	static void* threadproc(void *arg)
	{
		static_cast<Worker*>(arg)->thread();
		return 0;
	}

	static void* threadproc_id(void *arg)
	{
		static_cast<Worker*>(arg)->idle_thread();
		return 0;
	}

	static void* threadproc_cr(void *arg)
	{
		static_cast<Worker*>(arg)->critical_thread();
		return 0;
	}

	void critical_thread()
	{
		job *cr_j=0;
		while (true)
		{
			pthread_mutex_lock(&_job_lock);
			while (_critical_jobs.empty())
			{
				pthread_cond_wait(&_cr_job_rdy, &_job_lock);
			}

			cr_j = _critical_jobs.front();
			_critical_jobs.pop();
			pthread_mutex_unlock(&_job_lock);
			cr_j->_thread = _tid;
			cr_j->do_it();
			cr_j->done = true;
			if (cr_j->_deleteme)
				delete cr_j;
			cr_j = 0;
			pthread_cond_signal(&_job_done);
		}
		delete this;
	}

	void idle_thread()
	{
		job *id_j=0;
		while (true)
		{
			if (id_j)
			{
				id_j->step();
				if (id_j->done)
				{
					if (id_j->_deleteme)
						delete id_j;
				//	printf("signal %p\n", id_j);
					id_j = 0;
					pthread_cond_signal(&_job_done);
				}
			}
			else 
			{
				pthread_mutex_lock(&_job_lock);
				while (_idle_jobs.empty())
				{
					pthread_cond_wait(&_job_rdy, &_job_lock);
				}
				id_j = _idle_jobs.front();
				_idle_jobs.pop();
				id_j->_thread = _tid;
				pthread_mutex_unlock(&_job_lock);
			}
		}
		delete this;
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
			//	if (!_blockOnCritical)
			//		pthread_mutex_unlock(&_job_lock);
				pthread_mutex_unlock(&_job_lock);
				cr_j->_thread = _tid;
				cr_j->do_it();
				cr_j->done = true;
				if (cr_j->_deleteme)
					delete cr_j;
			//	printf("signal %p\n", cr_j);
				cr_j = 0;
				pthread_cond_signal(&_job_done);
			//	if (!_blockOnCritical)
			//		pthread_mutex_lock(&_job_lock);
				pthread_mutex_lock(&_job_lock);
			}
			
			if (id_j)
			{
				pthread_mutex_unlock(&_job_lock);
				id_j->step();
				if (id_j->done)
				{
					if (id_j->_deleteme)
						delete id_j;
				//	printf("signal %p\n", id_j);
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
					id_j->_thread = _tid;
				}
				else
				{
					while (_critical_jobs.empty() && _idle_jobs.empty())
						pthread_cond_wait(&_job_rdy, &_job_lock);
					pthread_mutex_unlock(&_job_lock);
				}
			}
		}
		delete this;
	}
};

#endif // !defined(LOADER_H)
