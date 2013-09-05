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

#ifndef _BUFFER_H
#define _BUFFER_H

#include <algorithm>
#include <vector>
#include <pthread.h>

#include "util.h"

template <typename T>
class RingBuffer
{
public:
	RingBuffer(int sizeTs) : _sizeTs(sizeTs) , _peekBuffer(0), _peekBufferSize(0)
	{
		_buffer = new T[sizeTs];
		_read = _write = _buffer;
	}

	~RingBuffer()
	{
		delete [] _buffer;
		delete [] _peekBuffer;
	}

	int count()
	{
		const int c = _write - _read;
		if (c < 0)
			return c + _sizeTs;
		else
			return c;
	}

	int write(T *data, int Ts_requested)
	{
		// actual max data is _sizeTs - 1
		// but you shouldn't be filling it up like that anyway!
		if (count() + Ts_requested > _sizeTs - 1)
		{
			fprintf(stderr, "Warning, RingBuffer truncating write\n");
			Ts_requested = _sizeTs - 1 - count();
		}

		int Ts = Ts_requested;

		if (_write + Ts >= _buffer + _sizeTs)
		{
			const int n = _buffer + _sizeTs - _write;
			memcpy(_write, data, n * sizeof(T));
			data += n;
			Ts -= n;
			_write = _buffer;
		}

		memcpy(_write, data, Ts * sizeof(T));
		_write += Ts;

		return Ts_requested;
	}

	int read(T *data, int Ts_requested)
	{
		if (Ts_requested > count())
		{
			fprintf(stderr, "Warning, RingBuffer truncating read\n");
			Ts_requested = count();
		}

		int Ts = Ts_requested;

		if (_read + Ts >= _buffer + _sizeTs)
		{
			const int n = _buffer + _sizeTs - _read;
			memcpy(data, _read, n * sizeof(T));
			data += n;
			Ts -= n;
			_read = _buffer;
		}

		memcpy(data, _read, Ts * sizeof(T));
		_read += Ts;

		return Ts_requested;
	}

	void clear()
	{
		_read = _write = _buffer;
	}

	void ignore(int Ts)
	{
		if (Ts > count())
		{
			fprintf(stderr, "Warning, RingBuffer truncating ignore\n");
			Ts = count();
		}
		_read += Ts;
		if (_read >= _buffer + _sizeTs)
			_read -= _sizeTs;
	}

	T *peek(int Ts)
	{
		if (Ts > count())
		{
			fprintf(stderr, "Warning, RingBuffer truncating peek\n");
			Ts = count();
		}

		if (_read + Ts >= _buffer + _sizeTs)
		{
			if (Ts > _peekBufferSize)
			{
				delete [] _peekBuffer;
				_peekBuffer = new T[Ts];
				_peekBufferSize = Ts;
			}
			const int n = _buffer + _sizeTs - _read;		
			memcpy(_peekBuffer, _read, n * sizeof(T));
			memcpy(_peekBuffer + n, _buffer, (Ts - n) * sizeof(T));
			return _peekBuffer;
		}
		return _read;
	}

private:
	int _sizeTs;
	T *_buffer;
	T *_write;
	T *_read;

	T *_peekBuffer;
	int _peekBufferSize;
};

template <typename Chunk_T>
class FilterSource : public T_sink_source<Chunk_T>
{
public:
	FilterSource(T_source<Chunk_T> *src) :
	  T_sink_source<Chunk_T>(src) {}

	virtual typename Chunk_T::sample_t* get_at_ofs(smp_ofs_t ofs, int n) = 0;
};

class FilterSourceImpl : public FilterSource<chunk_t>
{
public:
	FilterSourceImpl(T_source<chunk_t> *src)
	: FilterSource<chunk_t>(src),
	_ringBuffer(100000)
	{
		//	chunk_t *chk = _src->next();
		_bufferReadOfs = -10000;
		SamplePairf values[10000] = {{0.0,0.0}};
		_ringBuffer.write(values, 10000);
		//	_ringBuffer.write(chk->_data, chunk_t::chunk_size);
		
	}
	
	virtual ~FilterSourceImpl()
	{
	}
	
	void seek_smp(smp_ofs_t smp_ofs)
	{
		printf("do not call\n");
		_src->seek_smp(smp_ofs - 500);
		_bufferReadOfs = smp_ofs - 500;
		chunk_t *chk = _src->next();
		_ringBuffer.clear();
		_ringBuffer.write(chk->_data, chunk_t::chunk_size);
	}
	virtual void seek_chk(int chk_ofs)
	{
		//	_src->seek_chk(chk_ofs);
	}
	
	SamplePairf *get_at_ofs(smp_ofs_t ofs, int n)
	{
		_ringBuffer.ignore(ofs - _bufferReadOfs);
		_bufferReadOfs += ofs - _bufferReadOfs;
		while (ofs + n > _bufferReadOfs + _ringBuffer.count())
		{
			//	int skip = _ringBuffer.count() - (n+100) ;
			//	if (skip <= 0) fprintf(stderr, "this is not supposed to happen!\n");
			//	_ringBuffer.ignore(skip);
			//	_bufferReadOfs += skip;
			
			chunk_t *chk = _src->next();
			_ringBuffer.write(chk->_data, chunk_t::chunk_size);
			T_allocator<chunk_t>::free(chk);
		}
		SamplePairf *atReadOfs = _ringBuffer.peek(ofs - _bufferReadOfs + n);
		return atReadOfs + (ofs - _bufferReadOfs);
	}
private:
	RingBuffer<SamplePairf> _ringBuffer;
	smp_ofs_t _bufferReadOfs;
};

template <typename Chunk_T>
class BufferedStream : public FilterSource<Chunk_T>
{
public:
	typedef Chunk_T chunk_t;

	const static int BufSz = 0x3000;

	BufferedStream(T_source<Chunk_T> *src, double smp_rate=44100.0, bool preload=true);
	~BufferedStream();
	
	typedef typename Chunk_T::sample_t Sample_T;

	Chunk_T *get_chunk(unsigned int chk_ofs);
	Chunk_T *next();
	void seek_smp(smp_ofs_t smp_ofs);
	int get_samples(double tm, typename Chunk_T::sample_t *buf, int num);
	int get_samples(smp_ofs_t ofs, typename Chunk_T::sample_t *buf, int num);
	typename T_source<Chunk_T>::pos_info& len();
	bool load_next();

	Sample_T* load(double tm);
	Sample_T* get_at(double tm, int num_samples=26);
	Sample_T* get_at_ofs(smp_ofs_t ofs, int n);

protected:
	ASIOProcessor *_io;
	std::vector<Chunk_T*> _chks;
	smp_ofs_t _chk_ofs;
	smp_ofs_t _smp_ofs;
	smp_ofs_t _chk_ofs_loading;
	Lock_T _buffer_lock;
	Lock_T _src_lock;
	Lock_T _data_lock;

	Sample_T _buffer[BufSz];
	Sample_T *_buffer_p;
	double _buffer_tm;

	double _sample_rate;
	double _sample_period;
};

#include "meta.h"

#include "buffer.cpp"

#endif // !defined(BUFFER_H)
