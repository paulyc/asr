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

#ifndef _CONFIG_H
#define _CONFIG_H

#define MAC 1
#define IOS 0
#define PPC 0
#define WINDOWS 0
#define SGI 0
#define SUN 0
#define LINUX 0
#define BEOS 0

#define PLATFORM_X86 0
#define PLATFORM_ARM 1

#if MAC
#define OPENGL_ENABLED 0
#else
#define OPENGL_ENABLED 0
#endif

#if MAC
#include <unistd.h>
#endif

#define NATIVE_INT64 0
#define IEEE754_64FLOAT 1

#define ONE_CPU 1

#define OLD_PATH 1
#define TEST 0
#define TEST0 0
#define TEST1 0
#define TEST2 0

#define NON_SSE_INTS 1
#define CARE_ABOUT_INPUT 1
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

#define TRACE 0

#define HANDLE_TOPLEVEL_EXCEPTIONS 1

#define VINYL_CONTROL 0

#define DUMMY_ASIO 0
#define PARALLELS_ASIO 0

#define CHUNK_SIZE 1024

#define TEST_BEATS 0

#define PARALLEL_PROCESS 1

#include <stdlib.h>
#include "malloc.h"

#endif // !defined(_CONFIG_H)
