// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef _LOADER_H
#define _LOADER_H

#include <queue>

#include <pthread.h>

//#include "track.h"
#include "util.h"
#include "malloc.h"
#include "stream.h"

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
		(new Worker)->spin(true);
        (new Worker)->spin(true);
		(new Worker)->spin(false);
		(new Worker)->spin(false);
		(new Worker)->spin(false);
		(new Worker)->spin(false);
		(new Worker)->spin(false);
        (new Worker)->spin(false);
		(new Worker)->spin(false);
        (new Worker)->spin(false);
		(new Worker)->spin(false);
	}

	static void destroy()
	{
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

	template <typename Source_T, typename Chunk_T>
	struct generate_chunk_job : public job
	{
		Source_T *src;
        IChunkGeneratorCallback<Chunk_T> *cbObj;
        int chkId;
		Lock_T *lock;
		generate_chunk_job(Source_T *s, 
			IChunkGeneratorCallback<Chunk_T> *cbo, int cid, Lock_T *l) :
			src(s), cbObj(cbo), chkId(cid), lock(l) {_name="generate chunk";}

		void do_it()
		{
			cbObj->chunkCb(src->next(), chkId);
		}
	};
    
    template <typename Source_T, typename Chunk_T>
	struct generate_chunk_loop : public job, public IChunkGeneratorLoop<Chunk_T>
	{
		Source_T *src;
        IChunkGeneratorCallback<Chunk_T> *cbObj;
        int chkId;
		Lock_T *lock;
		generate_chunk_loop(Source_T *s,
                           IChunkGeneratorCallback<Chunk_T> *cbo, int cid, int chunks_to_buffer) :
        src(s), cbObj(cbo), chkId(cid), running(true), _chunks_to_buffer(chunks_to_buffer) {_name="generate chunk";}
        
		void do_it()
		{
            myLock.acquire();
            cbObj->lock(chkId);
            while (running)
            {
                while (nextChks.size() >= _chunks_to_buffer)
                {
                    cbObj->unlock(chkId);
                    doGen.wait(myLock);
                    cbObj->lock(chkId);
                }
                nextChks.push(src->next());
            }
		}
        
        Chunk_T *get()
        {
            Chunk_T *chk = 0;
            myLock.acquire();
            if (nextChks.empty())
                chk = zero_source<chunk_t>::get()->next();
            else
            {
                chk = nextChks.front();
                nextChks.pop();
                doGen.signal();
            }
            myLock.release();
            return chk;
        }
        
        Lock_T myLock;
        Condition_T doGen;
        bool running;
        std::queue<Chunk_T*> nextChks;
        int _chunks_to_buffer;
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
			(*this->d)();
			delete this->d;
		}
	};

	template <typename Obj_T>
	struct call_deferreds_job : public job
	{
		call_deferreds_job(Obj_T *obj) : _object(obj) {}
		void step()
		{
			_object->call_deferreds_loop(_thread);
            delete this;
            pthread_exit(0);
		}
		Obj_T *_object;
	};

	template <typename Track_T>
	struct load_track_job : public job
	{
		load_track_job(Track_T *t, Lock_T *l):track(t),_lock(l){
			_name="load track";
		}
		Track_T *track;
		
		//const wchar_t *file;
		//void do_it()
		//{
		//	track->set_source_file(file);
		//	done = true;
		//}
		Lock_T _l;
		Lock_T *_lock;
		Condition_T _next_step;
		void step()
		{
			track->load(_lock);
			T_allocator<typename Track_T::chunk_t>::gc();
            done = true;
            printf("job done %p\n", this);
		}
	};

	template <typename Track_T>
	struct draw_waveform_job : public job
	{
		draw_waveform_job(Track_T *t, Lock_T *l):track(t),_lock(l){
			_name="draw wave";
		}
		Track_T *track;
		
		//const wchar_t *file;
		//void do_it()
		//{
		//	track->set_source_file(file);
		//	done = true;
		//}
		Lock_T _l;
		Lock_T *_lock;
		Condition_T _next_step;
		void step()
		{
			track->draw_if_loaded();
			done = true;
		}
	};

	static Lock_T _job_lock;
	static Condition_T _cr_job_rdy;
	static Condition_T _job_rdy;
	static Condition_T _job_done;

	static std::queue<job*> _critical_jobs;
	static std::queue<job*> _idle_jobs;
	static pthread_once_t once_control; 
	bool _blockOnCritical;

	void spin(bool critical=false)
	{
#if 1
		if (critical)
        {
            sched_param p = {1};
            pthread_create(&_tid, 0, threadproc_cr, (void*)this);
            pthread_setschedparam(_tid, SCHED_FIFO, &p);
        }
		else pthread_create(&_tid, 0, threadproc_id, (void*)this);
#else
		pthread_create(&_tid, 0, threadproc, (void*)this);
#endif
	}

public:
	static void do_job(job *j, bool sync=false, bool critical=false, bool deleteme=true)
	{
		pthread_once(&once_control, init);
	//	printf("doing job %p (%s) sync %d critical %d\n", j, j->_name, sync, critical);
		_job_lock.acquire();
		j->_deleteme = deleteme;
		if (critical)
		{
			_critical_jobs.push(j);
			_cr_job_rdy.signal();
		}
		else
			_idle_jobs.push(j);

		_job_rdy.signal();
		if (sync)
		{
			while (!j->done)
			{
				printf("waiting on %p\n", j);
				_job_done.wait(_job_lock);
				printf("done %p\n", j);
			}
			delete j;
		}
		_job_lock.release();
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
			_job_lock.acquire();
			while (_critical_jobs.empty())
			{
				_cr_job_rdy.wait(_job_lock);
			}

			cr_j = _critical_jobs.front();
			_critical_jobs.pop();
			_job_lock.release();
			cr_j->_thread = _tid;
			cr_j->do_it();
			cr_j->done = true;
			if (cr_j->_deleteme)
				delete cr_j;
			cr_j = 0;
			_job_done.signal();
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
					_job_done.signal();
				}
			}
			else 
			{
				_job_lock.acquire();
				while (_idle_jobs.empty())
				{
					_job_rdy.wait(_job_lock);
				}
				id_j = _idle_jobs.front();
				_idle_jobs.pop();
				id_j->_thread = _tid;
				_job_lock.release();
			}
		}
		delete this;
	}

	void thread()
	{
		job *id_j=0, *cr_j=0;
		while (true)
		{
			_job_lock.acquire();
			while (!_critical_jobs.empty())
			{
				cr_j = _critical_jobs.front();
				_critical_jobs.pop();
			//	if (!_blockOnCritical)
			//		pthread_mutex_unlock(&_job_lock);
				_job_lock.release();
				cr_j->_thread = _tid;
				cr_j->do_it();
				cr_j->done = true;
				if (cr_j->_deleteme)
					delete cr_j;
			//	printf("signal %p\n", cr_j);
				cr_j = 0;
				_job_done.signal();
			//	if (!_blockOnCritical)
			//		pthread_mutex_lock(&_job_lock);
				_job_lock.acquire();
			}
			
			if (id_j)
			{
				_job_lock.release();
				id_j->step();
				if (id_j->done)
				{
					if (id_j->_deleteme)
						delete id_j;
				//	printf("signal %p\n", id_j);
					id_j = 0;
					_job_done.signal();
				}
			}
			else 
			{
				if (!_idle_jobs.empty())
				{
					id_j = _idle_jobs.front();
					_idle_jobs.pop();
					_job_lock.release();
					id_j->_thread = _tid;
				}
				else
				{
					while (_critical_jobs.empty() && _idle_jobs.empty())
						_job_rdy.wait(_job_lock);
					_job_lock.release();
				}
			}
		}
		delete this;
	}
};

#endif // !defined(LOADER_H)
