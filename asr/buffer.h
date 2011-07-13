#ifndef _BUFFER_H
#define _BUFFER_H

#include <vector>
#include <pthread.h>

#include "util.h"

template <typename Chunk_T, typename Sample_T>
class CircularBuffer
{
public:
	CircularBuffer(T_source<Chunk_T> *src);
};

template <typename Chunk_T>
class BufferedStream : public T_source<Chunk_T>, T_sink<Chunk_T>
{
public:
	typedef Chunk_T chunk_t;

	BufferedStream(T_source<Chunk_T> *src, bool preload=true) :
	  T_sink(src),
	  _chks(10000, 0),
	  _chk_ofs(0),
	  _smp_ofs(0),
	  _chk_ofs_loading(0)
	{
		int chks = len().chunks;
		if (chks != -1)
			_chks.resize(chks, 0);
		pthread_mutex_init(&_buffer_lock, 0);
		pthread_mutex_init(&_src_lock, 0);
		pthread_mutex_init(&_data_lock, 0);
	}

	~BufferedStream()
	{
		pthread_mutex_destroy(&_data_lock);
		pthread_mutex_destroy(&_src_lock);
		pthread_mutex_destroy(&_buffer_lock);
		std::vector<Chunk_T *>::iterator i;
		for (i = _chks.begin(); i != _chks.end(); ++i)
		{
			T_allocator<Chunk_T>::free(*i);
		}
	}

	Chunk_T *get_chunk(int chk_ofs)
	{
		pthread_mutex_lock(&_buffer_lock);
		if (chk_ofs >= _chks.size())
			_chks.resize(chk_ofs*2, 0);
		if (!_chks[chk_ofs])
		{
			pthread_mutex_unlock(&_buffer_lock);
			if (_src->_len.chunks != -1 && chk_ofs >= _src->_len.chunks)
			{
				Chunk_T *c = zero_source<Chunk_T>::get()->next();
				pthread_mutex_lock(&_buffer_lock);
				_chks[chk_ofs] = c;
			}
			else
			{
				// possible race condition where mp3 chunk not loaded
				try
				{
					pthread_mutex_lock(&_src_lock);
					_src->seek_chk(chk_ofs);
				} 
				catch (std::exception &e)
				{
					pthread_mutex_unlock(&_src_lock);
					return zero_source<Chunk_T>::get()->next();
				}
				Chunk_T *c = _src->next();
				pthread_mutex_unlock(&_src_lock);
				pthread_mutex_lock(&_buffer_lock);
				_chks[chk_ofs] = c;
			}
		}
		Chunk_T *r = _chks[chk_ofs];
		pthread_mutex_unlock(&_buffer_lock);
		return r;
	}

	Chunk_T *next_chunk(int chk_ofs)
	{
		Chunk_T *c;
		pthread_mutex_lock(&_buffer_lock);
		if (chk_ofs >= _chks.size())
			_chks.resize(chk_ofs*2, 0);
		if (!_chks[chk_ofs])
		{
			pthread_mutex_unlock(&_buffer_lock);
			pthread_mutex_lock(&_src_lock);
			c = _src->next();
			pthread_mutex_unlock(&_src_lock);
			pthread_mutex_lock(&_buffer_lock);
			_chks[chk_ofs] = c;
		}
		c = _chks[chk_ofs];
		pthread_mutex_unlock(&_buffer_lock);
		return c;
	}

	Chunk_T *next()
	{
		smp_ofs_t ofs_in_chk;
		Chunk_T *chk = T_allocator<Chunk_T>::alloc(), *buf_chk = get_chunk(_chk_ofs);
		smp_ofs_t to_fill = Chunk_T::chunk_size, loop_fill;
		Chunk_T::sample_t *to_ptr = chk->_data;
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
			memcpy(to_ptr, buf_chk->_data + ofs_in_chk, loop_fill*sizeof(Chunk_T::sample_t));
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

	void seek_chk(int chk_ofs)
	{
		seek_smp(chk_ofs*Chunk_T::chunk_size);
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
		smp_ofs_t ofs = tm *_src->sample_rate();
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
		smp_ofs_t chk_ofs = ofs / Chunk_T::chunk_size,
			smp_ofs = ofs % Chunk_T::chunk_size;
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
				if (_src->len().chunks != -1 && _src->len().chunks <= chk_ofs)
					c = zero_source<Chunk_T>::get()->next();
				else
				{
					_src->seek_chk(chk_ofs);
					c = _src->next();
				}
				pthread_mutex_unlock(&_src_lock);
				pthread_mutex_lock(&_buffer_lock);
				_chks[chk_ofs] = c;
			}

			// copy bits
			memcpy(buf, _chks[chk_ofs]->_data + smp_ofs, sizeof(Chunk_T::sample_t)*to_cpy);
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

	const pos_info& len()
	{
		return _src->len();
	}

	int load_complete()
	{
		int chk = 0;
		for (; !_src->eof(); ++chk)
		{
			next_chunk(chk);
		}
		_len.chunks = chk-1;
		pthread_mutex_lock(&_src_lock);
		_src->_len.chunks = chk-1;
		_src->_len.samples = _src->_len.chunks*Chunk_T::chunk_size;
		_src->_len.smp_ofs_in_chk = _src->_len.samples % _src->_len.chunks;
		_src->_len.time = _src->_len.samples/_src->sample_rate();
		pthread_mutex_unlock(&_src_lock);
		return _len.chunks;
	}

	bool load_next(pthread_mutex_t *lock)
	{
		if (_src->eof())
		{
			_len.chunks = _chk_ofs_loading-1;
			pthread_mutex_lock(&_src_lock);
			_src->_len.chunks = _chk_ofs_loading-1;
			_src->_len.samples = _src->_len.chunks*Chunk_T::chunk_size;
			_src->_len.smp_ofs_in_chk = _src->_len.samples % _src->_len.chunks;
			_src->_len.time = _src->_len.samples/_src->sample_rate();
			pthread_mutex_unlock(&_src_lock);
			return false;
		}
		next_chunk(_chk_ofs_loading++);
		return true;
	}

protected:
	std::vector<Chunk_T *> _chks;
	smp_ofs_t _chk_ofs;
	smp_ofs_t _smp_ofs;
	smp_ofs_t _chk_ofs_loading;
	pthread_mutex_t _buffer_lock;
	pthread_mutex_t _src_lock;
	pthread_mutex_t _data_lock;
};

template <typename Source_T, int BufSz=0x1000>
class BufferMgr
{
public:
	typedef typename Source_T::type::sample_t Sample_T;

	BufferMgr(Source_T *src, double smp_rate=44100.0) :
	  _src(src),
	  _buffer_p(0),
	  _sample_rate(smp_rate)
	{
		_sample_period = 1.0 / _sample_rate;

	}

	Sample_T* load(double tm)
	{
		_src->get_samples(tm, _buffer, BufSz);
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
				smp_ofs_t ofs = tm_diff * _src->sample_rate();
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

	Sample_T* get_at_ofs(smp_ofs_t ofs)
	{
		_src->get_samples(ofs, _buffer, 30);
		return _buffer;
	}

	const typename Source_T::pos_info& len()
	{
		return _src->len();
	}

protected:
	Source_T *_src;
	Sample_T _buffer[BufSz];
	Sample_T *_buffer_p;
	double _buffer_tm;

	double _sample_rate;
	double _sample_period;
};

#include "meta.h"

#endif // !defined(BUFFER_H)
