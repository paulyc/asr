#ifndef _CONFIG_H
#define _CONFIG_H

#undef MAC 
#define PPC 0
#define WINDOWS 1
#define SGI 0
#define SUN 0
#define LINUX 0
#define BEOS 0

#if WINDOWS
#include <windows.h>
#endif

#define NATIVE_INT64 0
#define IEEE754_64FLOAT 1

#define OLD_PATH 1
#define TEST 0
#define TEST0 0
#define TEST1 0
#define TEST2 0

#define NON_SSE_INTS 1
#define	CARE_ABOUT_INPUT 1
#define ECHO_INPUT 0
#define DO_OUTPUT 1

#if OLD_PATH
#define C_LOOPS 1
#define USE_SSE2 0
#else
#define C_LOOPS 0
#define USE_SSE2 1
#endif

#if C_LOOPS && USE_SSE2
#define USE_SSE2 0
#endif

#define CHANNELS 2
#define BUFFERSIZE 0x960
#define SAMPLERATE 48000.0f
#define INPUT_PERIOD (1.0/44100.0)

#define USE_BUFFER_MGR 1

#define DEBUG_ALLOCATOR 0
#define DEBUG_MALLOC 0
#define USE_NEW_WAVE 0

#define TRACE 0

#define ASYNC_GENERATE 0

#if 1
#define GENERATE_LOOP 1
#define BUFFER_BEFORE_COPY 1
#else
#define GENERATE_LOOP 1
#define BUFFER_BEFORE_COPY 0
#endif

#define HANDLE_TOPLEVEL_EXCEPTIONS 0

#endif // !defined(_CONFIG_H)
