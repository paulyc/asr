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

#ifndef _LOCK_H
#define _LOCK_H

#include <pthread.h>
#include <semaphore.h>
#include <sched.h>

#include "config.h"

#if !IOS
#include <emmintrin.h>
#endif

#if ONE_CPU
#define YIELD_IF_1CPU sched_yield();
#else
#define YIELD_IF_1CPU
#endif

#define RELEASE_USERLOCK(var) __asm { \
__asm lea edx, dword ptr[ecx+var] \
__asm mov eax, 1 \
__asm mov ebx, 0 \
__asm lock cmpxchg dword ptr[edx], ebx \
}

#define ACQUIRE_USERLOCK(var) __asm { \
spin__LINE__: \
__asm	push ecx \
} \
 \
		YIELD_IF_1CPU \
 \
__asm { \
__asm			pop ecx \
after_yield__LINE__: \
__asm			lea edx, dword ptr[ecx+var] \
__asm			mov eax, dword ptr[edx] \
__asm			test eax, eax \
__asm			jnz spin \
 \
__asm			mov ebx, 1 \
__asm			lock cmpxchg dword ptr[edx], ebx \
 \
__asm			test eax, eax \
__asm			jnz spin \
}

template <typename T>
class ThreadTester
{
public:
	ThreadTester() :
	  shared_var(0),
	  shared_var_p(0)
	{
	}

	T lock;
	int shared_var;
	int *shared_var_p;

	static void* testth(void *param) {
		ThreadTester<T> *tester = (ThreadTester<T>*)(param);
		int count;
		for (count = 0; count < 10000000; ++count) {
			tester->lock.acquire();
			tester->shared_var_p = &tester->shared_var;
			(*tester->shared_var_p)++; 
			tester->shared_var_p = 0;
			tester->lock.release();
		}
		return (void*)count;
	}
	void test() {
		pthread_t tid1, tid2, tid3, tid4;
		int dunno;
		pthread_create(&tid1, 0, testth, (void*)this);
		pthread_create(&tid2, 0, testth, (void*)this);
		pthread_create(&tid3, 0, testth, (void*)this);
		pthread_create(&tid4, 0, testth, (void*)this);

		pthread_join(tid1,(void**)&dunno);
		printf("dunno is %d\n", dunno);
		pthread_join(tid2,(void**)&dunno);
		printf("dunno is %d\n", dunno);
		pthread_join(tid3,(void**)&dunno);
		printf("dunno is %d\n", dunno);
		pthread_join(tid4,(void**)&dunno);
		printf("dunno is %d\n", dunno);
		printf("shared var is %d\n", shared_var);
	}
};

class PthreadLock
{
	friend class PthreadCondition;
public:
	PthreadLock() : _count(0) {
		pthread_mutex_init(&_lock, NULL);
	}
	~PthreadLock() {
		pthread_mutex_destroy(&_lock);
	}
	void acquire() {
		pthread_mutex_lock(&_lock);
		++_count;
	}
	void release() {
		--_count;
		pthread_mutex_unlock(&_lock);
	}
	int count() {
		return _count;
	}
protected:
	pthread_mutex_t _lock;
	int _count;
};

class PthreadCondition
{
public:
	PthreadCondition() {
		pthread_cond_init(&_cond, NULL);
	}
	~PthreadCondition() {
		pthread_cond_destroy(&_cond);
	}
	void wait(PthreadLock &lock) {
		pthread_cond_wait(&_cond, &lock._lock);
	}
	void signal() {
		pthread_cond_signal(&_cond);
	}
	void signal_all() {
		pthread_cond_broadcast(&_cond);
	}
protected:
	pthread_cond_t _cond;
};

class MultiLock : public PthreadLock
{
public:
	MultiLock() : PthreadLock()
	{
		_p_own_flag = &_own_flag;
	}
	void acquire() {
		if (_count > 0) {
			pthread_t current = pthread_self();
			if (pthread_equal(_owner, current)) {
				++_count;
			} else {
				PthreadLock::acquire();
			}
		} else if (_count == 0) {
			_owner = pthread_self();
			++_count;
			PthreadLock::acquire();
		} else {
			printf("Something strange happened. _count is %d\n", _count);
		}
	}
protected:
	int _own_flag;
	int *_p_own_flag;
	pthread_t _owner;
};

#if !IOS
class FastUserSpinLock
{
public:
	FastUserSpinLock() : _ownFlag(0) {}
	void acquire()
	{
		while(!__sync_bool_compare_and_swap(&_ownFlag, 0, 1))
		{
			while(_ownFlag) _mm_pause();
		}
	}
	void release()
	{
		__sync_lock_release(&_ownFlag);
	}
private:
	volatile int _ownFlag;
};

class FastUserCondition
{
public:
	FastUserCondition() : _waiters(0) {sem_init(&_sem, 0, 0);}
	void wait(FastUserSpinLock &lock)
	{
		while (!__sync_bool_compare_and_swap(&_waiters, _waiters, _waiters+1))
			; // this space intentionally left blank
		lock.release();
		sem_wait(&_sem);
		lock.acquire();
	}
	void signal()
	{
		while (true)
		{
			if (_waiters > 0)
			{
				if (!__sync_bool_compare_and_swap(&_waiters, _waiters, _waiters-1))
					continue;
				sem_post(&_sem);
				break;
			}
		}
	}
	void signal_all()
	{
		while (_waiters > 0)
		{
			if (!__sync_bool_compare_and_swap(&_waiters, _waiters, _waiters-1))
				continue;
			sem_post(&_sem);
		}
	}
private:
	int _waiters;
	sem_t _sem;
};
#endif

class CriticalSectionGuard
{
public:
	CriticalSectionGuard() : _criticalCount(0) {}
	void enter()
	{
		_lock.acquire();
		++_criticalCount;
		_lock.release();
	}
	void leave()
	{
		_lock.acquire();
		if (--_criticalCount == 0)
		{
			_wait.signal_all();
		}
		_lock.release();
	}
	void yield()
	{
		_lock.acquire();
		while (_criticalCount > 0)
			_wait.wait(_lock);
		_lock.release();
	}
private:
	int _criticalCount;
#if IOS
	typedef PthreadLock Lock_T;
	typedef PthreadCondition Cond_T;
#else
	typedef FastUserSpinLock Lock_T;
	typedef FastUserCondition Cond_T;
#endif
	Lock_T _lock;
	Cond_T _wait;
};

#if 0

class PreemptableLockTester : public ThreadTester<PreemptableSpinLock>
{
public:
	static void* testth_preemptable(void *param) {
		PreemptableLockTester *tester = (PreemptableLockTester*)(param);
		int count;
		for (count = 0; count < 10000000; ++count) {
			tester->lock.acquire_preemptable();
			tester->shared_var_p = &tester->shared_var;
			(*tester->shared_var_p)++; 
			tester->shared_var_p = 0;
			tester->lock.release();
		}
		printf("Preemptable done\n");
		return (void*)count;
	}
	static void* testth_preempter(void *param) {
		PreemptableLockTester *tester = (PreemptableLockTester*)(param);
		int count;
		for (count = 0; count < 10000000; ++count) {
			tester->lock.acquire_preempt();
			tester->shared_var_p = &tester->shared_var;
			(*tester->shared_var_p)++; 
			tester->shared_var_p = 0;
			tester->lock.release();
		}
		printf("Preempter done\n");
		return (void*)count;
	}
	void test() {
		pthread_t tid1, tid2, tid3, tid4;
		int dunno;
		pthread_create(&tid1, 0, testth_preempter, (void*)this);
		pthread_create(&tid2, 0, testth_preemptable, (void*)this);
		pthread_create(&tid3, 0, testth_preemptable, (void*)this);
		pthread_create(&tid4, 0, testth_preemptable, (void*)this);

		pthread_join(tid1,(void**)&dunno);
		printf("dunno is %d\n", dunno);
		pthread_join(tid2,(void**)&dunno);
		printf("dunno is %d\n", dunno);
		pthread_join(tid3,(void**)&dunno);
		printf("dunno is %d\n", dunno);
		pthread_join(tid4,(void**)&dunno);
		printf("dunno is %d\n", dunno);
		printf("shared var is %d\n", shared_var);
	}
};

#endif // 0

typedef PthreadLock Lock_T;
typedef PthreadCondition Condition_T;

#endif // !defined(_LOCK_H)
