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
using std::exception;

#include <fftw3.h>
#include <emmintrin.h>

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

template <typename T> 
const char* gettype() { return "T"; }

typable(float)
typable(double)

ASIOThinger<SamplePairf, short> * asio = NULL;

void begin()
{
	asio = new ASIOThinger<SamplePairf, short>;

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
	delete asio;
}

void testf()
{
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
		fprintf(output, "\ncalling ASIOThinger::LoadHelp\n");
		end = ASIOThinger::LoadHelp(buffer, tmp);
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
		testf();
	}catch(exception&e){
		throw e;
	}
	return 0;
#endif
#if 1
	//init();
	begin();
#else
	typedef chunk_time_domain_2d<SamplePairf, 4096, 1> c_T;
	//wavfile_chunker_T_N<c_T, 4096> wav_chunker(L"H:\\Music\\Gareth Emery - Metropolis (Original Mix).wav");
	wavfile_chunker_T_N<c_T, 4096, 1> wav_chunker(L"F:\\My Music\\Flip & Fill - I Wanna Dance With Somebody (Resource Mix).wav");
	lowpass_filter_td<SamplePairf, c_T, double> filt1(&wav_chunker, 22050.0, 44100.0, 44100.0);
	//lowpass_filter_td<float, c_T, double> filt2(&filt1, 22050.0, 44100.0, 45000.0);
	my_output<c_T> out(&filt1);
	//my_output<c_T> out(&wav_chunker);
	do
	{
		out.process();
	} while (!wav_chunker.eof());
	return 0;
#endif

#if WINDOWS
	MainLoop_Win32();
#endif

	end();
	return 0;
}
