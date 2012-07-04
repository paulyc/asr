#ifndef _FUTURE_H
#define _FUTURE_H

#include "worker.h"

class FutureExecutor
{
friend class Worker;
public:
	FutureExecutor() : _running(true)
	{
		pthread_mutex_init(&_deferreds_lock, 0);
		pthread_cond_init(&_have_deferred, 0);

		Worker::do_job(new Worker::call_deferreds_job<FutureExecutor>(this));
	}

	virtual ~FutureExecutor()
	{
		pthread_mutex_lock(&_deferreds_lock);
		_running = false;
		pthread_cond_signal(&_have_deferred);
		pthread_mutex_unlock(&_deferreds_lock);
		
		pthread_join(_deferred_thread, 0);
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

private:
	void call_deferreds_loop(pthread_t th)
	{
		_deferred_thread = th;
		pthread_mutex_lock(&_deferreds_lock);
		while (_running)
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
			pthread_cond_wait(&_have_deferred, &_deferreds_lock);
		}
		pthread_exit(0);
	}

	pthread_t _deferred_thread;
	std::queue<deferred*> _defcalls;
	pthread_mutex_t _deferreds_lock;
	pthread_cond_t _have_deferred;
	bool _running;
};

#endif // !defined(FUTURE_H)
