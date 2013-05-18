#include <pthread.h>
#include <unordered_map>
#include <cstdlib>

#include "mmalloc.h"

my_hash_map _malloc_map;

pthread_mutex_t malloc_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
pthread_once_t once_control = PTHREAD_ONCE_INIT;
pthread_mutexattr_t malloc_attr;

const char* __file__ = "unknown";
size_t __line__ = 0;

bool destructing_hash = false;

void init_lock()
{
	pthread_mutexattr_init(&malloc_attr);
	pthread_mutexattr_settype(&malloc_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&malloc_lock, &malloc_attr);
}

void do_stuff(void *m, size_t by, const char *f, int l);

#define DEBUG_MALLOC 1
#if DEBUG_MALLOC

void *operator new(size_t by, const char *f, int l)
{
	pthread_once(&once_control, init_lock);
	pthread_mutex_lock(&malloc_lock);
	void *m = malloc(by);
    do_stuff(m, by, f, l);
	pthread_mutex_unlock(&malloc_lock);
	return m;
}

void *operator new[](size_t by, const char *f, int l)
{
	pthread_once(&once_control, init_lock);
	pthread_mutex_lock(&malloc_lock);
	void *m = malloc(by);
	do_stuff(m, by, f, l);
	pthread_mutex_unlock(&malloc_lock);
	return m;
}

#include "config.h"

void operator delete(void *m)
{
	pthread_mutex_lock(&malloc_lock);
	if (!destructing_hash)
		_malloc_map.erase(m);
	free(m);
	pthread_mutex_unlock(&malloc_lock);
}

void operator delete[](void *m)
{
	pthread_mutex_lock(&malloc_lock);
	if (!destructing_hash)
		_malloc_map.erase(m);
        free(m);
        pthread_mutex_unlock(&malloc_lock);
}

void dump_malloc()
{
    _malloc_map.dump();
}

#endif // DEBUG_MALLOC
