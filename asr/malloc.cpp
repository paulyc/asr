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

#define DEBUG_MALLOC 0
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
