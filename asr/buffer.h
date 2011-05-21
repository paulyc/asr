#ifndef _BUFFER_H
#define _BUFFER_H

#include <vector>

#include "util.h"

template <typename Chunk_T, typename Sample_T>
class CircularBuffer
{
public:
	CircularBuffer(T_source<Chunk_T> *src);
};

template <typename Chunk_T, typename Sample_T, int chunk_size>
class BufferedStream : public T_source<Chunk_T>, T_sink<Chunk_T>
{
public:
	BufferedStream(T_source<Chunk_T> *src, bool preload=true) :
	  T_sink(src),
	  _chks(10000, 0),
	  _chk_ofs(0),
	  _smp_ofs(0)
	{

	}

	~BufferedStream()
	{
		std::vector<Chunk_T *>::iterator i;
		for (i = _chks.begin(); i != _chks.end(); ++i)
		{
			T_allocator<Chunk_T>::free(*i);
		}
	}

	Chunk_T *get_chunk(int chk_ofs)
	{
		if (chk_ofs >= _chks.size())
			_chks.resize(chk_ofs*2, 0);
		if (!_chks[chk_ofs])
		{
			_src->seek_chk(chk_ofs);
			_chks[chk_ofs] = _src->next();
		}
		return _chks[chk_ofs];
	}

	Chunk_T *next()
	{
		smp_ofs_t ofs_in_chk;
		Chunk_T *chk = T_allocator<Chunk_T>::alloc(), *buf_chk = get_chunk(_chk_ofs);
		int to_fill = chunk_size, loop_fill;
		Sample_T *to_ptr = chk->_data;
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
			ofs_in_chk = _smp_ofs % chunk_size;
			loop_fill = min(to_fill, chunk_size - ofs_in_chk);
			memcpy(to_ptr, buf_chk->_data + ofs_in_chk, loop_fill*sizeof(Sample_T));
			to_fill -= loop_fill;
			to_ptr += loop_fill;
			if (ofs_in_chk + loop_fill >= chunk_size)
				buf_chk = get_chunk(++_chk_ofs);
			ofs_in_chk = (ofs_in_chk + loop_fill) % chunk_size;
			_smp_ofs += loop_fill;
		}
		
		return chk;
	}

	void seek_chk(int chk_ofs)
	{
		seek_smp(chk_ofs*chunk_size);
	}

	void seek_smp(smp_ofs_t smp_ofs)
	{
		_smp_ofs = smp_ofs;
		if (_smp_ofs < 0)
			_chk_ofs = 0;
		else
			_chk_ofs = smp_ofs / chunk_size;
	}

	int get_samples(double tm, Sample_T *buf, int num)
	{
		smp_ofs_t ofs = tm *44100.0;
		return get_samples(ofs, buf, num);
	}
	int get_samples(smp_ofs_t ofs, Sample_T *buf, int num)
	{
		while (ofs < 0 && num > 0)
		{
			(*buf)[0] = 0.0;
			(*buf)[1] = 0.0;
			++ofs;
			--num;
			++buf;
		}
		smp_ofs_t chk_ofs = ofs / 4096,
			smp_ofs = ofs % 4096;
		int chk_left, to_cpy, written=0;
		do
		{
			chk_left = 4096 - smp_ofs;
			to_cpy = min(chk_left, num);
			if (chk_ofs >= _chks.size())
				_chks.resize(chk_ofs*2, 0);
			if (!_chks[chk_ofs])
			{
				_src->seek_chk(chk_ofs);
				_chks[chk_ofs] = _src->next();
			}

			// copy bits
			memcpy(buf, _chks[chk_ofs]->_data + smp_ofs, sizeof(Sample_T)*to_cpy);

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

protected:
	std::vector<Chunk_T *> _chks;
	int _chk_ofs;
	smp_ofs_t _smp_ofs;
};

template <typename Source_T, typename Sample_T, int BufSz=0x1000>
class BufferMgr
{
public:
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
				smp_ofs_t ofs = tm_diff * 44100.0;
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

protected:
	Source_T *_src;
	Sample_T _buffer[BufSz];
	Sample_T *_buffer_p;
	double _buffer_tm;

	double _sample_rate;
	double _sample_period;
};

template <typename Chunk_T, typename Sample_T, int chunk_size>
class StreamMetadata
{
public:
	struct ChunkMetadata
	{
		ChunkMetadata():valid(false){}
		bool valid;
		Sample_T abs_sum;
		Sample_T avg;
		Sample_T avg_db;
	};

	class iterator
	{
	public:
		iterator(StreamMetadata p, int o, bool e=false):parent(p),ofs(o),eof(e){}
		iterator& operator++()
		{
			++ofs;
			return *this;
		}
		/*
		ChunkMetadata& operator*()
		{
			return 
			*/
	private:
		StreamMetadata *parent;
		int ofs;
		bool eof;
	};

	iterator begin()
	{
		return iterator(this, 0, false);
	}
	iterator end()
	{
		return iterator(this, 0, true);
	}


	StreamMetadata(BufferedStream<Chunk_T, Sample_T, chunk_size> *src) :
	  _src(src)
	  {
	  }

	const ChunkMetadata& get_metadata(int chk_ofs)
	{
		if (_chk_data.size() <= chk_ofs)
			_chk_data.resize(chk_ofs*2);
		ChunkMetadata &meta = _chk_data[chk_ofs];
		if (!meta.valid)
		{
			Chunk_T *chk = _src->get_chunk(chk_ofs);
			meta.abs_sum = Zero<Sample_T>::val;
			for (Sample_T *data=chk->_data, *end=data+chunk_size;
				data != end; ++data)
			{
				Sum<Sample_T>::calc(meta.abs_sum, meta.abs_sum, *data);
			}
			Division<Sample_T>::calc(meta.avg, meta.abs_sum, chunk_size);
			Log10<Sample_T>::calc(meta.avg_db, meta.avg);
			Product<Sample_T>::calc(meta.avg_db, meta.avg_db, 20);
			meta.valid = true;
		}
		return meta;
	}
protected:
	BufferedStream<Chunk_T, Sample_T, chunk_size> *_src;
	std::vector<ChunkMetadata> _chk_data;
};

#endif // !defined(BUFFER_H)
