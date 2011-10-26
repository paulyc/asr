#ifndef _META_H
#define _META_H

#include <sched.h>

template <typename Chunk_T>
class StreamMetadata
{
public:
	typedef typename Chunk_T chunk_t;
	typedef typename Chunk_T::sample_t Sample_T;

	BufferedStream<Chunk_T> *getSrc(){return _src;}

	struct ChunkMetadata
	{
		ChunkMetadata():valid(false),subband(10){}
		bool valid;
		Sample_T abs_sum;
		Sample_T avg;
		Sample_T avg_db;
		Sample_T peak;
		Sample_T peak_db;
		Sample_T peak_lo;

		struct s
		{
			Sample_T abs_sum;
			Sample_T avg;
			Sample_T avg_db;
			Sample_T peak;
			Sample_T peak_db;
			Sample_T peak_lo;
		};
		std::vector<s> subband;
	};

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

	void load_metadata(pthread_mutex_t *lock)
	{
		for (int chk_ofs=0; chk_ofs < _src->len().chunks; ++chk_ofs)
		{
			get_metadata(chk_ofs, lock);
			sched_yield();
		}
	}

	const ChunkMetadata& get_metadata(int chk_ofs, pthread_mutex_t *lock=0)
	{
		int i, indx;
		if (_chk_data.size() <= chk_ofs)
		{
			_chk_data.resize(chk_ofs*2);
		}
		ChunkMetadata &meta = _chk_data[chk_ofs];
		Sample_T dabs;
		if (!meta.valid)
		{
			if (lock) pthread_mutex_lock(lock);
			Chunk_T *chk = _src->get_chunk(chk_ofs);
			if (lock) pthread_mutex_unlock(lock);

			if (lock) pthread_mutex_lock(lock);
			for (i = 0; i < 10; ++i)
			{
				meta.subband[i].abs_sum[0] = 0.0f;
				meta.subband[i].abs_sum[1] = 0.0f;
				meta.subband[i].peak[0] = 0.0f;
				meta.subband[i].peak[1] = 0.0f;
				meta.subband[i].peak_lo[0] = 0.0f;
				meta.subband[i].peak_lo[1] = 0.0f;
			}
			indx=0;
			for (Sample_T *data=chk->_data, *end=data+Chunk_T::chunk_size;
				data != end; )
			{
				i = indx*10/Chunk_T::chunk_size;
				Abs<Sample_T>::calc(dabs, *data);
				Sum<Sample_T>::calc(meta.subband[i].abs_sum, meta.subband[i].abs_sum, dabs);
				SetMax<Sample_T>::calc(meta.subband[i].peak, *data);
				SetMin<Sample_T>::calc(meta.subband[i].peak_lo, *data);
				++data;
				++indx;
			}
			meta.abs_sum[0] = 0.0f;
			meta.abs_sum[1] = 0.0f;
			meta.peak[0] = 0.0f;
			meta.peak[1] = 0.0f;
			meta.peak_lo[0] = 0.0f;
			meta.peak_lo[1] = 0.0f;
			for (i=0; i<10; ++i)
			{
				meta.abs_sum[0] += meta.subband[i].abs_sum[0];
				meta.abs_sum[1] += meta.subband[i].abs_sum[1];
				meta.peak[0] = max(meta.peak[0], meta.subband[i].peak[0]);
				meta.peak[1] = max(meta.peak[1], meta.subband[i].peak[1]);
				meta.peak_lo[0] = min(meta.peak_lo[0], meta.subband[i].peak_lo[0]);
				meta.peak_lo[1] = min(meta.peak_lo[1], meta.subband[i].peak_lo[1]);

				Quotient<Sample_T>::calc(meta.subband[i].avg, meta.subband[i].abs_sum, Chunk_T::chunk_size/10);
				dBm<Sample_T>::calc(meta.subband[i].avg_db, meta.subband[i].avg);
				dBm<Sample_T>::calc(meta.subband[i].peak_db, meta.subband[i].peak);
			}
			Quotient<Sample_T>::calc(meta.avg, meta.abs_sum, Chunk_T::chunk_size);
			dBm<Sample_T>::calc(meta.avg_db, meta.avg);
			dBm<Sample_T>::calc(meta.peak_db, meta.peak);
			meta.valid = true;
			if (lock) pthread_mutex_unlock(lock);
		}
		return meta;
	}
protected:
	BufferedStream<Chunk_T> *_src;
	std::vector<ChunkMetadata> _chk_data;
	//std::vector<ChunkMetadata> _chk_data_micro;
	int _chk_ofs;
};

#endif // !defined(_META_H)
