#ifndef _LOCK_H
#define _LOCK_H

#include <pthread.h>
#include <semaphore.h>
#include <sched.h>

#define YIELD_IF_1CPU sched_yield

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

class Lock
{
public:
	Lock() : _count(0) {
		pthread_mutex_init(&_lock, NULL);
	}
	~Lock() {
		pthread_mutex_destroy(&_lock);
	}
	void acquire() {
		pthread_mutex_lock(&_lock);
	}
	void release() {
		pthread_mutex_unlock(&_lock);
	}
	int count() {
		return _count;
	}
protected:
	pthread_mutex_t _lock;
	int _count;
};

class MultiLock : public Lock
{
public:
	MultiLock() : Lock()
	{
		_p_own_flag = &_own_flag;
	}
	void acquire() {
		if (_count > 0) {
			pthread_t current = pthread_self();
			if (pthread_equal(_owner, current)) {
				++_count;
			} else {
				Lock::acquire();
			}
		} else if (_count == 0) {
			_owner = pthread_self();
			++_count;
			Lock::acquire();
		} else {
			printf("Something strange happened. _count is %d\n", _count);
		}
	}
protected:
	int _own_flag;
	int *_p_own_flag;
	pthread_t _owner;
};

class FastUserSpinLock : public Lock
{
public:
	FastUserSpinLock() {
		_own_flag = 0;
	}
	void acquire() {
		__asm {
			jmp after_yield
spin:
			push ecx
		}

		YIELD_IF_1CPU();

		__asm {
			pop ecx
after_yield:
			lea edx, dword ptr[ecx+_own_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jnz spin

			mov ebx, 1
			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jnz spin
		}
	}
	void release() {
		__asm {
			lea edx, dword ptr[ecx+_own_flag]
			mov eax, 1
			mov ebx, 0
			lock cmpxchg dword ptr[edx], ebx
		}
	}

protected:
	int _own_flag;
};

class FastUserSyscallLock : public Lock
{
public:
	FastUserSyscallLock() : 
	  Lock(),
	  _waiters(0),
	  _checker_flag(0),
	  _own_flag(0)
	{
		sem_init(&_sem, 0, 0);
	}
	~FastUserSyscallLock()
	{
		sem_destroy(&_sem);
	}
	void acquire() {
		__asm {
			jmp get_checker_flag
spin2:
			push ecx
		}
		sched_yield();
		__asm {
			pop ecx

get_checker_flag:
			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jnz spin2

			mov ebx, 1
			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jnz spin2

check_for_owner:
			lea edx, dword ptr[ecx+_own_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jz do_acquire

do_wait:
			lea edx, dword ptr[ecx+_waiters]
			mov eax, 1
			lock xadd dword ptr[edx], eax

			; release check lock & sem_wait
			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, 1
			mov ebx, 0
			lock cmpxchg dword ptr[edx], ebx

			push ecx
		}
		sem_wait(&_sem);
		__asm {
			pop ecx
			jmp get_checker_flag_again

spin3:
			push ecx
		}
		sched_yield();
		__asm {
			pop ecx

get_checker_flag_again:
			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jnz spin3

			mov ebx, 1
			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jnz spin3

			lea edx, dword ptr[ecx+_waiters]
			mov eax, -1
			lock xadd dword ptr[edx], eax

			; have checker flag need to see if locked
			jmp check_for_owner

spin4:
			push ecx
		}
		sched_yield();
		__asm {
			pop ecx

do_acquire:
			lea edx, dword ptr[ecx+_own_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jnz do_wait

			mov ebx, 1
			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jnz do_wait

			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, 1
			mov ebx, 0
			lock cmpxchg dword ptr[edx], ebx
		}
	}
	void release() {
		__asm {
			jmp get_checker_flag3
spin5:
			push ecx
		}
		sched_yield();
		__asm {
			pop ecx

get_checker_flag3:
			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jnz spin5

			mov ebx, 1
			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jnz spin5

check_for_waiters:
			mov eax, dword ptr[ecx+_waiters]
			test eax,eax
			jz release_the_locks_and_dont_check_again

			; someone is waiting or is about to be
			lea eax, dword ptr[ecx+_sem]
			push ecx
		}
		sem_post(&_sem);
		__asm {
			pop ecx

			test eax, eax
			jnz release_the_locks_and_dont_check_again

release_the_locks_and_check_again:
			lea edx, dword ptr[ecx+_own_flag]
			mov eax, 1
			mov ebx, 0
			lock cmpxchg dword ptr[edx], ebx

			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, 1
			mov ebx, 0
			lock cmpxchg dword ptr[edx], ebx
			
			jmp get_checker_flag3

		
release_the_locks_and_dont_check_again:
			lea edx, dword ptr[ecx+_own_flag]
			mov eax, 1
			mov ebx, 0
			lock cmpxchg dword ptr[edx], ebx
			
			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, 1
			mov ebx, 0
			lock cmpxchg dword ptr[edx], ebx

end_of_story:
		}
	}
	
protected:
	sem_t _sem;
	int _waiters;
	int _checker_flag;
	int _own_flag;
};

#define FastUserLock FastUserSyscallLock

class FastCriticalSection : public FastUserLock
{
};

#endif // !defined(_LOCK_H)
