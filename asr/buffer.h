#ifndef _BUFFER_H
#define _BUFFER_H

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

template <typename Chunk_T>
class BufferedStream : public FilterSource<Chunk_T>
{
public:
	typedef Chunk_T chunk_t;

	const static int BufSz = 0x3000;

	BufferedStream(ASIOProcessor *io, T_source<Chunk_T> *src, double smp_rate=44100.0, bool preload=true) :
	  FilterSource<Chunk_T>(src),
	  _io(io),
	  _chk_ofs(0),
	  _smp_ofs(0),
	  _chk_ofs_loading(0),
	  _buffer_p(0),
	  _sample_rate(smp_rate)
	{
		int chks = len().chunks;
		if (chks != -1)
			_chks.resize(chks, 0);
		pthread_mutex_init(&_buffer_lock, 0);
		pthread_mutex_init(&_src_lock, 0);
		pthread_mutex_init(&_data_lock, 0);

		_sample_period = 1.0 / _sample_rate;
        
        _chks.resize(10000, 0);
	}

	~BufferedStream()
	{
		pthread_mutex_destroy(&_data_lock);
		pthread_mutex_destroy(&_src_lock);
		pthread_mutex_destroy(&_buffer_lock);
		typename std::vector<Chunk_T*>::iterator i;
		for (i = _chks.begin(); i != _chks.end(); ++i)
		{
			T_allocator<Chunk_T>::free(*i);
		}
	}

	Chunk_T *get_chunk(unsigned int chk_ofs)
	{
		// dont think this needs locked??
		if (this->_src->len().chunks != -1 && chk_ofs >= uchk_ofs_t(this->_src->len().chunks))
		{
			return zero_source<Chunk_T>::get()->next();
		}
		pthread_mutex_lock(&_buffer_lock);
		if (chk_ofs >= _chks.size())
			_chks.resize(chk_ofs*2, 0);
		if (!_chks[chk_ofs])
		{
		//	printf("Shouldnt happen anymore\n");
			pthread_mutex_unlock(&_buffer_lock);
			// possible race condition where mp3 chunk not loaded
			try
			{
				pthread_mutex_lock(&_src_lock);
				this->_src->seek_chk(chk_ofs);
			} 
			catch (std::exception e)
			{
				printf("%s\n", e.what());
				pthread_mutex_unlock(&_src_lock);
				return zero_source<Chunk_T>::get()->next();
			}
			Chunk_T *c = this->_src->next();
			pthread_mutex_unlock(&_src_lock);
			pthread_mutex_lock(&_buffer_lock);
			_chks[chk_ofs] = c;
		}
		Chunk_T *r = _chks[chk_ofs];
		pthread_mutex_unlock(&_buffer_lock);
		return r;
	}

	Chunk_T *next_chunk(smp_ofs_t chk_ofs)
	{
		Chunk_T *c;
		pthread_mutex_lock(&_buffer_lock);
		if (chk_ofs < 0)
		{
			return zero_source<Chunk_T>::get()->next();
		}
		uint32_t uchk_ofs = (uint32_t)chk_ofs;
		if (uchk_ofs >= _chks.size())
			_chks.resize(uchk_ofs*2, 0);
		if (!_chks[uchk_ofs])
		{
			pthread_mutex_unlock(&_buffer_lock);
			pthread_mutex_lock(&_src_lock);
			c = this->_src->next();
			pthread_mutex_unlock(&_src_lock);
			pthread_mutex_lock(&_buffer_lock);
			_chks[uchk_ofs] = c;
		}
		c = _chks[uchk_ofs];
		pthread_mutex_unlock(&_buffer_lock);
		return c;
	}

	Chunk_T *next()
	{
		smp_ofs_t ofs_in_chk;
		Chunk_T *chk = T_allocator<Chunk_T>::alloc(), *buf_chk = get_chunk(_chk_ofs);
		smp_ofs_t to_fill = Chunk_T::chunk_size, loop_fill;
		typename Chunk_T::sample_t *to_ptr = chk->_data;
		pthread_mutex_lock(&_data_lock);
		while (to_fill > 0)
		{
			while (_smp_ofs < 0 && to_fill > 0)
			{
				(*to_ptr)[0] = 0.0f;
				(*to_ptr++)[1] = 0.0f;
				++_smp_ofs;
				//--loop_fill;
				--to_fill;
			}
			if (to_fill == 0)
				break;
			ofs_in_chk = _smp_ofs % Chunk_T::chunk_size;
			loop_fill = min(to_fill, Chunk_T::chunk_size - ofs_in_chk);
			memcpy(to_ptr, buf_chk->_data + ofs_in_chk, loop_fill*sizeof(typename Chunk_T::sample_t));
			to_fill -= loop_fill;
			to_ptr += loop_fill;
			if (ofs_in_chk + loop_fill >= Chunk_T::chunk_size)
				buf_chk = get_chunk(++_chk_ofs);
			ofs_in_chk = (ofs_in_chk + loop_fill) % Chunk_T::chunk_size;
			_smp_ofs += loop_fill;
		}
		pthread_mutex_unlock(&_data_lock);
		return chk;
	}

	void seek_smp(smp_ofs_t smp_ofs)
	{
		pthread_mutex_lock(&_data_lock);
		_smp_ofs = smp_ofs;
		if (_smp_ofs < 0)
			_chk_ofs = 0;
		else
			_chk_ofs = smp_ofs / Chunk_T::chunk_size;
		pthread_mutex_unlock(&_data_lock);
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
			to_cpy = min(chk_left, num);
			pthread_mutex_lock(&_buffer_lock);
			if (chk_ofs >= _chks.size())
				_chks.resize(chk_ofs*2, 0);
			if (!_chks[chk_ofs])
			{
				pthread_mutex_unlock(&_buffer_lock);
				pthread_mutex_lock(&_src_lock);
				Chunk_T *c = 0;
				if (this->_src->len().chunks != -1 && usmp_ofs_t(this->_src->len().chunks) <= chk_ofs)
					c = zero_source<Chunk_T>::get()->next();
				else
				{
					this->_src->seek_chk(chk_ofs);
					c = this->_src->next();
				}
				pthread_mutex_unlock(&_src_lock);
				pthread_mutex_lock(&_buffer_lock);
				_chks[chk_ofs] = c;
			}

			// copy bits
			memcpy(buf, _chks[chk_ofs]->_data + smp_ofs, sizeof(typename Chunk_T::sample_t)*to_cpy);
			pthread_mutex_unlock(&_buffer_lock);

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
		pthread_mutex_lock(&_src_lock);
		this->_src->len().chunks = chk-1;
		this->_src->len().samples = this->_src->len().chunks*Chunk_T::chunk_size;
		this->_src->len().smp_ofs_in_chk = this->_src->len().samples % this->_src->len().chunks;
		this->_src->len().time = this->_src->len().samples/this->_src->sample_rate();
		pthread_mutex_unlock(&_src_lock);
		return this->_len.chunks;
	}

	bool load_next()
	{
		if (this->_src->eof())
		{
			this->_len.chunks = _chk_ofs_loading-1;
			pthread_mutex_lock(&_src_lock);
			this->_src->len().chunks = _chk_ofs_loading-1;
			this->_src->len().samples = this->_src->len().chunks*Chunk_T::chunk_size;
			this->_src->len().smp_ofs_in_chk = this->_src->len().samples % this->_src->len().chunks;
			this->_src->len().time = this->_src->len().samples/this->_src->sample_rate();
			pthread_mutex_unlock(&_src_lock);
			return false;
		}
		next_chunk(_chk_ofs_loading++);
		return true;
	}

	ASIOProcessor *get_io() { return _io; }

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
	pthread_mutex_t _buffer_lock;
	pthread_mutex_t _src_lock;
	pthread_mutex_t _data_lock;

	Sample_T _buffer[BufSz];
	Sample_T *_buffer_p;
	double _buffer_tm;

	double _sample_rate;
	double _sample_period;
};

#include "meta.h"

#endif // !defined(BUFFER_H)
