#ifndef _LOCK_H
#define _LOCK_H

#include <pthread.h>
#include <semaphore.h>
#include <sched.h>

#define YIELD_IF_1CPU sched_yield

#define RELEASE_USERLOCK(var) __asm { \
__asm lea edx, dword ptr[ecx+var] \
__asm mov eax, 1 \
__asm mov ebx, 0 \
__asm lock cmpxchg dword ptr[edx], ebx \
}

#define ACQUIRE_USERLOCK(var) __asm { \
spin__LINE__: \
__asm 	push ecx \
} \
 \
		YIELD_IF_1CPU(); \
 \
__asm { \
__asm 			pop ecx \
after_yield__LINE__: \
__asm 			lea edx, dword ptr[ecx+var] \
__asm 			mov eax, dword ptr[edx] \
__asm 			test eax, eax \
__asm 			jnz spin \
 \
__asm 			mov ebx, 1 \
__asm 			lock cmpxchg dword ptr[edx], ebx \
 \
__asm 			test eax, eax \
__asm 			jnz spin \
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

class Lock
{
	friend class Condition;
public:
	Lock() : _count(0) {
		pthread_mutex_init(&_lock, NULL);
	}
	~Lock() {
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

class Condition
{
public:
	Condition() {
		pthread_cond_init(&_cond, NULL);
	}
	~Condition() {
		pthread_cond_destroy(&_cond);
	}
	void wait(Lock *lock) {
		pthread_cond_wait(&_cond, &lock->_lock);
	}
	void signal() {
		pthread_cond_signal(&_cond);
	}
protected:
	pthread_cond_t _cond;
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
		RELEASE_USERLOCK(_own_flag)
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
		}
		RELEASE_USERLOCK(_checker_flag)
		__asm {

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
		}
		RELEASE_USERLOCK(_checker_flag)
	}
	void release() {
		int val;
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
			;lea eax, dword ptr[ecx+_sem]
			push ecx
		}
		sem_post(&_sem);
		__asm {
			pop ecx

			test eax, eax
			jnz release_the_locks_and_dont_check_again

			push ecx
		}
		sem_getvalue(&_sem, &val);
		if (val > 0)
			sem_wait(&_sem); // undo
		__asm {
			pop ecx

release_the_locks_and_check_again:
		}
		RELEASE_USERLOCK(_own_flag)
		RELEASE_USERLOCK(_checker_flag)
		__asm {
			jmp get_checker_flag3

release_the_locks_and_dont_check_again:
		}
		RELEASE_USERLOCK(_own_flag)
		RELEASE_USERLOCK(_checker_flag)
	}
	
protected:
	sem_t _sem;
	int _waiters;
	int _checker_flag;
	int _own_flag;
};

class PriorityLock : public FastUserSpinLock
{
public:
private:
};

#define FastUserLock FastUserSyscallLock

#if 0
#ifdef WIN32
class PreemptableSpinLock : public Lock
{
public:
	PreemptableSpinLock() :
	  _checker_flag(0),
	  _owner_flag(0),
	  _is_preemptable(false),
	  _was_preempted(false),
	  _owner_thread(NULL),
	  _suspended_owner_thread(NULL)
	{
	}
	void acquire_preemptable() {
		__asm {
			jmp after_yield_acquire_preemptable
spin_acquire_preemptable:
			push ecx
		}

		YIELD_IF_1CPU();

		__asm {
			pop ecx
after_yield_acquire_preemptable:
			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jnz spin_acquire_preemptable

			mov ebx, 1
			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jnz spin_acquire_preemptable

			jmp try_acquire_and_release

release_and_start_over:
			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, 1
			mov ebx, 0
			lock cmpxchg dword ptr[edx], ebx
			jmp spin_acquire_preemptable

try_acquire_and_release:
			lea edx, dword ptr[ecx+_owner_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jnz release_and_start_over

			mov ebx, 1
			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jnz release_and_start_over

configure:
			push ecx
		}
		_is_preemptable = true;
		_owner_thread = ::GetCurrentThread();
		__asm {
			pop ecx
			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, 1
			mov ebx, 0
			lock cmpxchg dword ptr[edx], ebx
		}
	}
	void acquire_preempt() {
		__asm {
			jmp after_yield_acquire_preempt
spin_acquire_preempt:
			push ecx
		}

		YIELD_IF_1CPU();

		__asm {
			pop ecx
after_yield_acquire_preempt:
			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jnz spin_acquire_preempt

			mov ebx, 1
			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jnz spin_acquire_preempt

check_if_owned:
			lea edx, dword ptr[ecx+_owner_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jnz try_preempt

			mov ebx, 1
			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jz now_own_lock

try_preempt:
			push ecx
		}
		if (_is_preemptable) {
			::SuspendThread(_owner_thread);
			_suspended_owner_thread = _owner_thread;
			_owner_thread = ::GetCurrentThread();
			_was_preempted = true;
			_is_preemptable = false;
		} else {
			__asm {
				pop ecx

				lea edx, dword ptr[ecx+_checker_flag]
				mov eax, 1
				mov ebx, 0
				lock cmpxchg dword ptr[edx], ebx

				jmp spin_acquire_preempt
			}
		}
		__asm {
			pop ecx
now_own_lock:
			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, 1
			mov ebx, 0
			lock cmpxchg dword ptr[edx], ebx
		}
	}
	void release() {
		__asm {
			jmp after_yield_preemptable_release
spin_preemptable_release:
			push ecx
		}

		YIELD_IF_1CPU();

		__asm {
			pop ecx
after_yield_preemptable_release:
			lea edx, dword ptr[ecx+_checker_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jnz spin_preemptable_release

			mov ebx, 1
			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jnz spin_preemptable_release

			push ecx
		}

		if (_was_preempted) {
			_owner_thread = _suspended_owner_thread;
			_suspended_owner_thread = NULL;
			_was_preempted = false;
			_is_preemptable = true;
			::ResumeThread(_owner_thread);

			__asm {
				pop ecx
				lea edx, dword ptr[ecx+_checker_flag]
				mov eax, 1
				mov ebx, 0
				lock cmpxchg dword ptr[edx], ebx
			}
		} else {
			_is_preemptable = false;
			_owner_thread = NULL;
			__asm {
				pop ecx

				lea edx, dword ptr[ecx+_owner_flag]
				mov eax, 1
				mov ebx, 0
				lock cmpxchg dword ptr[edx], ebx

				lea edx, dword ptr[ecx+_checker_flag]
				mov eax, 1
				mov ebx, 0
				lock cmpxchg dword ptr[edx], ebx
			}
		}
	}

protected:
	int _checker_flag;
	int _owner_flag;
	bool _is_preemptable;
	bool _was_preempted;
	HANDLE _owner_thread;
	HANDLE _suspended_owner_thread;
};

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
		for (count = 0; count < 5000000; ++count) {
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
		pthread_create(&tid1, 0, testth_preemptable, (void*)this);
		pthread_create(&tid2, 0, testth_preemptable, (void*)this);
		pthread_create(&tid3, 0, testth_preemptable, (void*)this);
		pthread_create(&tid4, 0, testth_preempter, (void*)this);

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
#endif
#endif

typedef Lock Lock_T;


#endif // !defined(_LOCK_H)
