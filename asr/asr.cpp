#include "config.h"

#include <asio.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <string>
#include <exception>
#include <iostream>
#include <fstream>
#include <hash_map>
using std::exception;

#include <pthread.h>

#include <fftw3.h>
#include <emmintrin.h>

#if DEBUG_MALLOC
struct alloc_info
{
	alloc_info(){}
	alloc_info(size_t b, const char *f, int l) : 
		bytes(b), file(f), line(l){}
	size_t bytes;
	const char *file;
	int line;
};

bool destructing_hash = false;

class my_hash_map : public stdext::hash_map<void*, alloc_info>
{
public:
	~my_hash_map()
	{
		destructing_hash = true;
	}
};

my_hash_map _malloc_map;
pthread_mutex_t malloc_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
pthread_once_t once_control = PTHREAD_ONCE_INIT; 
pthread_mutexattr_t malloc_attr;

void init_lock()
{
	pthread_mutexattr_init(&malloc_attr);
	pthread_mutexattr_settype(&malloc_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&malloc_lock, &malloc_attr);
}

void *operator new(size_t by, const char *f, int l)
{
	pthread_once(&once_control, init_lock);
	pthread_mutex_lock(&malloc_lock);
	void *m = malloc(by);
	_malloc_map[m].bytes = by;
	_malloc_map[m].file = f;
	_malloc_map[m].line = l;
	pthread_mutex_unlock(&malloc_lock);
	return m;
}

void *operator new[](size_t by, const char *f, int l)
{
	pthread_once(&once_control, init_lock);
	pthread_mutex_lock(&malloc_lock);
	void *m = malloc(by);
	_malloc_map[m].bytes = by;
	_malloc_map[m].file = f;
	_malloc_map[m].line = l;
	pthread_mutex_unlock(&malloc_lock);
	return m;
}

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
	for (stdext::hash_map<void*, alloc_info>::iterator i = _malloc_map.begin();
		i != _malloc_map.end(); ++i)
	{
		printf("%p allocated %s:%d bytes %d\n", i->first, 
			i->second.file, 
			i->second.line,
			i->second.bytes);
	}
}
#endif

#include "asr.h"
#include "type.h"
#include "util.h"

#include "fftw.h"

#include "wavfile.h"
#include "filter.h"
#include "display.h"
#include "asiodrv.cpp"
#include "io.h"
#include "speedparser.h"
#include "ui.h"
#include "tracer.h"

template <typename T> 
const char* gettype() { return "T"; }

typable(float)
typable(double)

ASIOProcessor * asio = 0;
GenericUI *ui = 0;

void begin()
{
	asio = new ASIOProcessor;
#if WINDOWS
	ui = new Win32UI(asio);
#endif
	asio->set_ui(ui);
#if RUN
	asio->Start();
	//ASIOStart();
#endif

#if TEST2
	asio->Start();
#endif
}

void end()
{
#if RUN
	//ASIOStop();
	asio->Stop();
#endif
	asio->Finish();
	delete ui;
	delete asio;

#if DEBUG_MALLOC
	dump_malloc();
#endif
}

void testf()
{
	printf("testf\n");
#if TEST0
	char buf[128];
	char *f1 = buf;
	while ((int)f1 & 0xFF) ++f1;
	int *foo = (int*)f1, bar;
	foo[0] = 1;
	foo[1] = 2;
	foo[2] = 3;
	foo[3] = 4;
	foo[4] = 5;
	foo[5] = 6;
	foo[6] = 7;
	foo[7] = 8;

	__m128i a, b, *c;
	a.m128i_i32[0] = 1;
	a.m128i_i32[1] = 2;
	a.m128i_i32[2] = 3;
	a.m128i_i32[3] = 4;
	b = *(__m128i*)foo;
	1;
	__asm
	{
		;ldmxcsr 0x1f80
		movdqa xmm1, b

	}
	bar = 10;
	//c = (__m128i*)foo;
	//a = _mm_load_si128(c);
	//__asm
	//{
	//	movdqa xmm1, [foo]
	//}
	bar = 50;
	return;
#endif

#if TEST1
	FILE *output = fopen("foo.txt", "w");
#if !USE_SSE2 && !NON_SSE_INTS
		short *tmp = T_allocator_N_16ba<short, BUFFERSIZE>::alloc();
#else
		int *tmp = T_allocator_N_16ba<int, BUFFERSIZE>::alloc();
#endif
		float *buffer = T_allocator_N_16ba<float, BUFFERSIZE>::alloc(), *end;
		for (int i=0;i<BUFFERSIZE;++i)
		{
			tmp[i] = BUFFERSIZE/2 - i;
			fprintf(output, "%d ", tmp[i]);
		}
		fprintf(output, "\ncalling ASIOProcessor::LoadHelp\n");
		end = ASIOProcessor::LoadHelp(buffer, tmp);
		for (int i=0;i<BUFFERSIZE;++i)
		{
			fprintf(output, "%f ", buffer[i]);
		}
		fprintf(output, "\n%p %p %x\n", buffer, end, end-buffer);
	fclose(output);
#endif
}

int main()
{
	//chunk_T_T_size<int, 128> sthing;
	//printf("%d\n", sizeof(sthing._data));
	//printf("%x\n", _WIN32_WINNT);
#if TEST
	try{
		Tracer::hook(testf);
		testf();
	}catch(exception&e){
		throw e;
	}
	return 0;
#endif

	begin();

#if WINDOWS
	ui->main_loop();
#endif

	end();
	return 0;
}
