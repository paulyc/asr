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
    //	void process(bool freeme=true)
    //	{
    //		Chunk_T *chk = next();
    //		T_allocator<T>::free(chk);
    //	}
    
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

	Chunk_T *get_chunk(unsigned int chk_ofs);

	Chunk_T *next_chunk(smp_ofs_t chk_ofs);

	Chunk_T *next();

	void seek_smp(smp_ofs_t smp_ofs)
	{
		_data_lock.acquire();
		_smp_ofs = smp_ofs;
		if (_smp_ofs < 0)
			_chk_ofs = 0;
		else
			_chk_ofs = smp_ofs / Chunk_T::chunk_size;
		_data_lock.release();
	}

	int get_samples(double tm, typename Chunk_T::sample_t *buf, int num)
	{
		smp_ofs_t ofs = tm * this->_src->sample_rate();
		return get_samples(ofs, buf, num);
	}
	int get_samples(smp_ofs_t ofs, typename Chunk_T::sample_t *buf, int num)
	{
		while (ofs < 0 && num > 0)
		{
			(*buf)[0] = 0.0;
			(*buf)[1] = 0.0;
			++ofs;
			--num;
			++buf;
		}
		usmp_ofs_t chk_ofs = (usmp_ofs_t)ofs / Chunk_T::chunk_size,
			smp_ofs = (usmp_ofs_t)ofs % Chunk_T::chunk_size;
		int chk_left, to_cpy, written=0;
		do
		{
			chk_left = Chunk_T::chunk_size - smp_ofs;
			to_cpy = std::min(chk_left, num);
			_buffer_lock.acquire();
			if (chk_ofs >= _chks.size())
				_chks.resize(chk_ofs*2, 0);
			if (!_chks[chk_ofs])
			{
				_buffer_lock.release();
				_src_lock.acquire();
				Chunk_T *c = 0;
				if (this->_src->len().chunks != -1 && usmp_ofs_t(this->_src->len().chunks) <= chk_ofs)
					c = zero_source<Chunk_T>::get()->next();
				else
				{
					this->_src->seek_chk(chk_ofs);
					c = this->_src->next();
				}
				_src_lock.release();
				_buffer_lock.acquire();
				_chks[chk_ofs] = c;
			}

			// copy bits
			memcpy(buf, _chks[chk_ofs]->_data + smp_ofs, sizeof(typename Chunk_T::sample_t)*to_cpy);
			_buffer_lock.release();

			if (to_cpy == chk_left)
			{
				++chk_ofs;
				smp_ofs = 0;
			}
			buf += to_cpy;
			num -= to_cpy;
			written += to_cpy;
		} while (num > 0);

		return written;
	}

	typename T_source<Chunk_T>::pos_info& len()
	{
		return this->_src->len();
	}

	int load_complete()
	{
		int chk = 0;
		for (; !this->_src->eof(); ++chk)
		{
			next_chunk(chk);
		}
		this->_len.chunks = chk-1;
		_src_lock.acquire();
		this->_src->len().chunks = chk-1;
		this->_src->len().samples = this->_src->len().chunks*Chunk_T::chunk_size;
		this->_src->len().smp_ofs_in_chk = this->_src->len().samples % this->_src->len().chunks;
		this->_src->len().time = this->_src->len().samples/this->_src->sample_rate();
		_src_lock.release();
		return this->_len.chunks;
	}

	bool load_next()
	{
		if (this->_src->eof())
		{
			this->_len.chunks = _chk_ofs_loading-1;
			_src_lock.acquire();
			this->_src->len().chunks = _chk_ofs_loading-1;
			this->_src->len().samples = this->_src->len().chunks*Chunk_T::chunk_size;
			this->_src->len().smp_ofs_in_chk = this->_src->len().samples % this->_src->len().chunks;
			this->_src->len().time = this->_src->len().samples/this->_src->sample_rate();
			_src_lock.release();
			return false;
		}
		next_chunk(_chk_ofs_loading++);
		return true;
	}

	typedef typename Chunk_T::sample_t Sample_T;

	Sample_T* load(double tm)
	{
		this->_src->get_samples(tm, _buffer, BufSz);
		_buffer_tm = tm;
		_buffer_p = _buffer;
		return _buffer_p;
	}

	Sample_T* get_at(double tm, int num_samples=26)
	{
		if (_buffer_p)
		{
			double tm_diff = tm-_buffer_tm;
			if (tm_diff < 0.0)
			{
				return load(tm);
			}
			else
			{
				smp_ofs_t ofs = tm_diff * this->_src->sample_rate();
				if (ofs+num_samples >= BufSz)
				{
					return load(tm);
				}
				else
				{
					return _buffer_p + ofs;
				}
			}
		}
		else
		{
			return load(tm);
		}
	}

	Sample_T* get_at_ofs(smp_ofs_t ofs, int n)
	{
		get_samples(ofs, _buffer, n);
		return _buffer;
	}

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
