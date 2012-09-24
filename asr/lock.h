#ifndef _LOCK_H
#define _LOCK_H

#include <pthread.h>
#include <semaphore.h>

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
spin:
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
	static int shared_var;
	static int *shared_var_p;
	static void* testth(void *param) {
		FastUserSpinLock *lock = (FastUserSpinLock*)param;
		int count;
		for (count = 0; count < 100000000; ++count) {
			lock->acquire();
			shared_var_p = &shared_var;
			(*shared_var_p)++; 
			shared_var_p = 0;
			lock->release();
		}
		return (void*)count;
	}
	static void test() {
		FastUserSpinLock *l = new FastUserSpinLock;
		pthread_t tid1, tid2, tid3, tid4;
		int dunno;
		pthread_create(&tid1, 0, testth, (void*)l);
		pthread_create(&tid2, 0, testth, (void*)l);
		pthread_create(&tid3, 0, testth, (void*)l);
		pthread_create(&tid4, 0, testth, (void*)l);

		pthread_join(tid1,(void**)&dunno);
		printf("dunno is %d\n", dunno);
		pthread_join(tid2,(void**)&dunno);
		printf("dunno is %d\n", dunno);
		printf("shared var is %d\n", shared_var);
		delete l;
	}
protected:
	int _own_flag;
};

class FastUserSyscallLock : public FastUserSpinLock
{
public:
	FastUserSyscallLock() : 
	  FastUserSpinLock(),
	  _waiters(0)
	{
		sem_init(&_sem, 0, 0);
	}
	~FastUserSyscallLock()
	{
		sem_destroy(&_sem);
	}
	void acquire() {
	//	_check_lock.acquire();
		
		__asm {
			jmp check_lock_FastUserSyscallLock

sem_wait_for_lock_FastUserSyscallLock:
			mov eax, 1
			lock xadd dword ptr[ecx+_waiters], eax

			mov edx, dword ptr[ecx+_own_flag]

			lea eax, dword ptr[ecx+_sem]
			push eax ; calling convention?
			call sem_wait

		;	xor eax,eax
		;	mov ebx, 1
		;	lock cmpxchg dword ptr[edx], ebx

		;	test eax, eax
		;	jnz spin_FastUserSyscallLock

			mov eax, -1
			lock xadd dword ptr[ecx+_waiters], eax

check_lock_FastUserSyscallLock:
			mov edx, dword ptr[ecx+_own_flag]
			mov eax, dword ptr[edx]
			mov ebx, 1
			test eax, eax
			jnz sem_wait_for_lock_FastUserSyscallLock

			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jnz sem_wait_for_lock_FastUserSyscallLock

end_FastUserSyscallLock:
		}
	//	_check_lock.acquire();
	}
	void release() {
	//	_check_lock.acquire();
		__asm {
			jmp chk_waiters
debug_breakpoint:
			int 3
hello:
			xor ebx, ebx
			jmp state
chk_waiters:
			lea edx, dword ptr[ecx+_waiters] ; edx = &waiters
			mov ebx, dword ptr[edx]          ; ebx = waiters
			mov eax, ebx
			dec ebx                          ; ebx = waiters-1

			test ebx,ebx
			jl hello
state:
			push eax
			lock xadd dword ptr[ecx+_waiters], ebx ;promise?
			pop ebx
			test eax, ebx
			jne wakeup_waiter
			jmp done
releaseme:

			lea edx, dword ptr[ecx+_own_flag]
			mov eax, dword ptr[edx]
			mov ebx, eax

			lock cmpxchg dword ptr[edx], ebx
			test eax, eax
			jnz debug_breakpoint

wakeup_waiter:
			; wakeup could move here probly
			lea eax, dword ptr[ecx+_sem]
			push eax
			call sem_post
			test eax, eax
			jz wakeup_waiter
done:
		}
	//	_check_lock.release();
	}
protected:
	sem_t _sem;
	int _waiters;
	FastUserSpinLock _check_lock;
};

class FastUserSyscallLock2 : public FastUserSyscallLock
{
public:
	void acquire() {
		_check_lock.acquire();
		
		__asm {
			jmp sem_wait_for_lock_FastUserSyscallLock2

sem_wait_for_lock_FastUserSyscallLock2:
			mov eax, dword ptr[ecx+_waiters]
			mov eax, dword ptr[eax]
			lock cmpxchg dword ptr[edx], ebx

			call FastUserSyscallLock2::release
			lea eax, dword ptr[ecx+_sem]
			push eax
			call sem_wait
			call FastUserSyscallLock2::acquire
check_lock_FastUserSyscallLock2:
			mov ebx, 1
			lea edx, dword ptr[ecx+_own_flag]
			mov eax, dword ptr[edx]
			test eax, eax
			jnz sem_wait_for_lock_FastUserSyscallLock2

			lock cmpxchg dword ptr[edx], ebx

			test eax, eax
			jnz sem_wait_for_lock_FastUserSyscallLock2
		}
		_check_lock.release();
	}
	void release() {
		_check_lock.acquire();
		__asm {
		;	mov edx, dword ptr[ecx+_p_own_flag]
		;	mov eax, 1
		;	mov ebx, 0

		;	lock cmpxchg dword ptr[edx], ebx
check_waiters_FastUserSyscallLock2:
		;	lea edx, dword ptr[ecx+_p_waiters]
		;	mov edx, dword ptr[edx]
			lea ebx, dword ptr[ecx+_waiters]
			mov ebx, dword ptr[ebx]
			test ebx, ebx
			jz done_FastUserSyscallLock2

		;	lock cmpxchg dword ptr[edx], ebx

		;	test eax, eax
		;	jnz done_FastUserSyscallLock2
wakeup_waiter_FastUserSyscallLock2:
		;	lea eax, dword ptr[ecx+_waiters]
		;	mov eax, dword ptr[eax]

			; wakeup could move here probly
			lea eax, dword ptr[ecx+_sem]
			push eax
			call sem_post
			test eax, eax
			jnz wakeup_waiter_FastUserSyscallLock2
			mov eax, -1
			lock xadd dword ptr[ecx+_waiters], eax
done_FastUserSyscallLock2:
		}
		_check_lock.release();
	}
};

#define FastUserLock FastUserSpinLock

class FastCriticalSection : public FastUserLock
{
};

#endif // !defined(_LOCK_H)
