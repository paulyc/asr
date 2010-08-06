#ifndef _FILTER_H
#define _FILTER_H

#include "util.h"

template <typename Sample_T, typename Chunk_T, typename Precision_T=double>
class lowpass_filter_td : public T_source<Chunk_T>, public T_sink<Chunk_T>
{
protected:
	T_source<Chunk_T> *_src;
	Precision_T _cutoff;
	Sample_T _input_buf[0x400];
	Sample_T *_input_buf_write;
	Chunk_T *_input_chk;
	Precision_T _t_input_chk;
	Sample_T *_input_ptr;

	const static int _default_sample_precision = 13;
	int _sample_precision;
	Precision_T _sampling_rate;

public:
	lowpass_filter_td(T_source<Chunk_T> *src, Precision_T cutoff) :
		_src(src),
		_cutoff(cutoff),
		_input_buf_write(_input_buf),
		_sample_precision(_default_sample_precision)
	{
		_input_chk = src->next();
		for (Sample_T *end = _input_buf + 2*_sample_precision; end != _input_buf_write; )
			*_input_buf_write++ = Zero<Sample_T>::val;
	}
	
	Chunk_T* next()
	{
		return T_source<Chunk_T>::next();
	}
	
	void next(Chunk_T* t)
	{
		T_sink<Chunk_T>::next(t);
	}
};

#endif // !defined(_FILTER_H)
