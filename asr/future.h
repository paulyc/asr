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
