#ifndef _TYPE_H
#define _TYPE_H

#include "wavfile.h"

typedef chunk_T_T_size<int, 2*BUFFERSIZE> my_chunk;
typedef T_allocator<my_chunk> my_allocator;
typedef T_source<my_chunk> my_source;
typedef T_sink<my_chunk> my_sink;

typedef fftwf_complex default_internal_sample_type;
typedef chunk_time_domain_2d<fftwf_complex, 2, BUFFERSIZE> default_internal_chunk_type;

#if 0
#if !USE_SSE2 && !NON_SSE_INTS
typedef int_N_wavfile_chunker_T<short, BUFFERSIZE> my_wavfile_chunker;
typedef int_N_wavfile_chunker_T_base<short, BUFFERSIZE> my_wavfile_chunker_base;
#else
typedef int_N_wavfile_chunker_T<int, BUFFERSIZE> my_wavfile_chunker;
typedef int_N_wavfile_chunker_T_base<int, BUFFERSIZE> my_wavfile_chunker_base;
#endif
#endif

typedef wavfile_chunker_T_N<default_internal_chunk_type, BUFFERSIZE, 2> my_wavfile_chunker;
typedef wavfile_chunker_T_N_base<default_internal_chunk_type, BUFFERSIZE, 2> my_wavfile_chunker_base;

#endif // !defined(TYPE_H)
