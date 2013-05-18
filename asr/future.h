#ifndef _FUTURE_H
#define _FUTURE_H

#include "worker.h"

class Executor
{
public:
	Executor()
	{
		pthread_mutex_init(&_deferreds_lock, 0);
		pthread_cond_init(&_have_deferred, 0);
	}

	virtual ~Executor()
	{
		pthread_cond_destroy(&_have_deferred);
		pthread_mutex_destroy(&_deferreds_lock);
	}

	void submit(deferred *d)
	{
		pthread_mutex_lock(&_deferreds_lock);
		_defcalls.push(d);
		pthread_cond_signal(&_have_deferred);
		pthread_mutex_unlock(&_deferreds_lock);
	}

	void process_calls()
	{
		while (!_defcalls.empty())
		{
			deferred *d = _defcalls.front();
			_defcalls.pop();
			pthread_mutex_unlock(&_deferreds_lock);
			d->operator()();
			delete d;
			pthread_mutex_lock(&_deferreds_lock);
		}
	}
protected:
	std::queue<deferred*> _defcalls;
	pthread_mutex_t _deferreds_lock;
	pthread_cond_t _have_deferred;
};

class FutureExecutor : public Executor
{
friend class Worker;
public:
	FutureExecutor() : Executor(), _running(true)
	{
		Worker::do_job(new Worker::call_deferreds_job<FutureExecutor>(this));
	}

	virtual ~FutureExecutor()
	{
		pthread_mutex_lock(&_deferreds_lock);
		_running = false;
		pthread_cond_signal(&_have_deferred);
		pthread_mutex_unlock(&_deferreds_lock);
		
		pthread_join(_deferred_thread, 0);
	}

private:
	void call_deferreds_loop(pthread_t th)
	{
		_deferred_thread = th;
		pthread_mutex_lock(&_deferreds_lock);
		while (_running)
		{
			process_calls();
			pthread_cond_wait(&_have_deferred, &_deferreds_lock);
		}
	}

	pthread_t _deferred_thread;
	bool _running;
};

#endif // !defined(FUTURE_H)
