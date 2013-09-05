// ASR - Digital Signal Processor
// Copyright (C) 2002-2013	Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.	 If not, see <http://www.gnu.org/licenses/>.

#ifndef _ASR_TASK_H_
#define _ASR_TASK_H_

#include <vector>

class ForkJoinTaskExecutor;
class ForkJoinTaskManager;

class ForkJoinTask
{
public:
	ForkJoinTask() : _finished(false) {}
	virtual ~ForkJoinTask() {}
	
	virtual void exec() = 0;
	virtual void fork(ForkJoinTask *task);
	virtual void join();

	ForkJoinTaskExecutor *_executor;
	bool _finished;
};

class ForkJoinTaskExecutor
{
public:
	ForkJoinTaskExecutor(ForkJoinTaskManager *mgr) : _mgr(mgr) {}
	
	void run()
	{
		
	}
	
	ForkJoinTaskManager *_mgr;
};

class ForkJoinTaskManager
{
public:
	ForkJoinTaskManager(int poolSize);
	
	void invoke(ForkJoinTask *task);
private:
	std::vector<ForkJoinTaskExecutor*> _pool;
};

#endif // !defined(_ASR_TASK_H_)
