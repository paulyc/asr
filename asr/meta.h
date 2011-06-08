#ifndef _META_H
#define _META_H

template <typename Source_T>
class MipMapLevel
{
public:
	typedef typename Source_T::chunk_t::sample_t sample_t;

	MipMapLevel(Source_T *src, int level) :
		_src(src)
	{
		_smp_per_mip = pow(2, level);
	}

	struct mip
	{
		sample_t abs_sum;
		sample_t avg;
		sample_t avg_db;
		sample_t peak_hi;
		sample_t peak_lo;
		sample_t peak_hi_db;
		sample_t peak_lo_db;
	};

	void process_next(typename Source_T::chunk_t *chk)
	{
	}

protected:
	Source_T *_src;
	int _smp_per_mip;
	std::vector<mip> _map;
};

template <typename Chunk_T>
class StreamMetadata
{
public:
	typedef typename Chunk_T chunk_t;
	typedef typename Chunk_T::sample_t Sample_T;

	BufferedStream<Chunk_T> *getSrc(){return _src;}

	struct ChunkMetadata
	{
		ChunkMetadata():valid(false){}
		bool valid;
		Sample_T abs_sum;
		Sample_T avg;
		Sample_T avg_db;
		Sample_T peak;
		Sample_T peak_db;
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


	StreamMetadata(BufferedStream<Chunk_T> *src) :
	  _src(src),
	  _chk_data(10000),
	  _chk_ofs(0)
	  {
	  }

	const typename T_source<Chunk_T>::pos_info& len()
	{
		return _src->len();
	}

	const ChunkMetadata& get_metadata(int chk_ofs)
	{
		if (_chk_data.size() <= chk_ofs)
			_chk_data.resize(chk_ofs*2);
		ChunkMetadata &meta = _chk_data[chk_ofs];
		Sample_T dabs;
		if (!meta.valid)
		{
			Chunk_T *chk = _src->get_chunk(chk_ofs);
			meta.abs_sum[0] = 0.0f;
			meta.abs_sum[1] = 0.0f;
			meta.peak[0] = 0.0f;
			meta.peak[1] = 0.0f;
			for (Sample_T *data=chk->_data, *end=data+Chunk_T::chunk_size;
				data != end; ++data)
			{
				Abs<Sample_T>::calc(dabs, *data);
				Sum<Sample_T>::calc(meta.abs_sum, meta.abs_sum, dabs);
				SetMax<Sample_T>::calc(meta.peak, dabs);
			}
			Quotient<Sample_T>::calc(meta.avg, meta.abs_sum, Chunk_T::chunk_size);
			dBm<Sample_T>::calc(meta.avg_db, meta.avg);
			dBm<Sample_T>::calc(meta.peak_db, meta.peak);
			meta.valid = true;
		}
		return meta;
	}
protected:
	BufferedStream<Chunk_T> *_src;
	std::vector<ChunkMetadata> _chk_data;
	int _chk_ofs;
};

#endif // !defined(_META_H)
