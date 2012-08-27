#ifndef _LOCK_H
#define _LOCK_H

class Lock
{
public:
	Lock() {
		pthread_init(&_lock);
	}
	void acquire() {
	}
	void release() {
	}
	int count() {
		return _count;
	}
protected:
	pthread_mutex_t _lock;
	int _count;
};

#endif // !defined(_LOCK_H)
