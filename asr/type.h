#ifndef _TYPE_H
#define _TYPE_H

//#include "wavfile.h"

/*
typedef chunk_T_T_size<int, 2*BUFFERSIZE> my_chunk;
typedef T_allocator<my_chunk> my_allocator;
typedef T_source<my_chunk> my_source;
typedef T_sink<my_chunk> my_sink;
*/

#include <fftw3.h>

#include "config.h"

typedef float SamplePairf[2];
typedef float SamplePairFloat32[2];
typedef double SamplePaird[2];
typedef double SamplePairFloat64[2];
typedef short SamplePairInt16[2];

typedef fftwf_complex ComplexPairf[2];
typedef fftw_complex ComplexPaird[2];

#if WINDOWS
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
#if WINDOWS
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
#endif
#else // !WINDOWS
#include <stdint.h>
#endif

typedef float float32_t;
typedef double float64_t;

typedef int32_t smp_ofs_t;
typedef int32_t ssmp_ofs_t;
typedef uint32_t usmp_ofs_t;

typedef int32_t chk_ofs_t;
typedef int32_t schk_ofs_t;
typedef uint32_t uchk_ofs_t;

#include "chunk.h"
typedef chunk_time_domain_1d<SamplePairf, CHUNK_SIZE> chunk_t;
typedef chunk_freq_domain_1d<fftwf_complex, CHUNK_SIZE> cchunk_t;

//typedef fftwf_complex default_internal_sample_type;
//typedef chunk_time_domain_2d<fftwf_complex, 2, BUFFERSIZE> default_internal_chunk_type;

#if 0
#if !USE_SSE2 && !NON_SSE_INTS
typedef int_N_wavfile_chunker_T<short, BUFFERSIZE> my_wavfile_chunker;
typedef int_N_wavfile_chunker_T_base<short, BUFFERSIZE> my_wavfile_chunker_base;
#else
typedef int_N_wavfile_chunker_T<int, BUFFERSIZE> my_wavfile_chunker;
typedef int_N_wavfile_chunker_T_base<int, BUFFERSIZE> my_wavfile_chunker_base;
#endif
#endif

//typedef wavfile_chunker_T_N<default_internal_chunk_type, BUFFERSIZE, 2> my_wavfile_chunker;
//typedef wavfile_chunker_T_N_base<default_internal_chunk_type, BUFFERSIZE, 2> my_wavfile_chunker_base;

template <typename T> 
const char* gettype();

#define stringify(x) #x
#define typable(type) template <> const char* gettype<type>() { return stringify(type); }
#define define_template_type_n(base,n,type) typedef base<n> type; typable(type)
#define define_template_type_T(base,T,type) typedef base<T> type; typable(type)
#define define_template_type_T_n(base,T,n,type) typedef base<T,n> type; typable(type)
#define define_template_type_T_n_T(base,T1,n,T2,type) typedef base<T,n,T2> type; typable(type)

#endif // !defined(TYPE_H)
