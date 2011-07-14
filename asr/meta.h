#ifndef _META_H
#define _META_H

#include <sched.h>

template <typename Source_T>
class MipMap
{
public:
	typedef typename Source_T::chunk_t::sample_t sample_t;
	class Level 
	{
	public:
		Level(int mips, int smps) :
			_map(mips),
			_smpno(0)
		{
			_smp_per_mip = double(smps)/double(mips);
			_write_iter = _map.begin();
		}

		struct mip
		{
			mip()
			{
				abs_sum[0] = 0.0f;
				abs_sum[1] = 0.0f;
				peak_hi[0] = 0.0f;
				peak_hi[1] = 0.0f;
				peak_lo[0] = 0.0f;
				peak_lo[1] = 0.0f;
			}
			typename MipMap::sample_t abs_sum;
			typename MipMap::sample_t avg;
			typename MipMap::sample_t avg_db;
			typename MipMap::sample_t peak_hi;
			typename MipMap::sample_t peak_lo;
			typename MipMap::sample_t peak_hi_db;
			typename MipMap::sample_t peak_lo_db;
		};

		void process_next(typename Source_T::chunk_t *chk)
		{
			mip &m = *_write_iter;
			for (typename MipMap::sample_t *smp = chk->_data, 
				*end = smp+Source_T::chunk_t::chunk_size;
				smp != end; ++smp)
			{
				if (_smpno > _smp_per_mip)
				{
					m.avg[0] = m.abs_sum[0] / _smpno;
					m.avg[1] = m.abs_sum[1] / _smpno;
					dBm<typename MipMap::sample_t>::calc(m.avg_db, m.avg);
					dBm<typename MipMap::sample_t>::calc(m.peak_hi_db, m.peak_hi);
					Abs<typename MipMap::sample_t>::calc(m.peak_lo_db, m.peak_lo);
					dBm<typename MipMap::sample_t>::calc(m.peak_lo_db, m.peak_lo_db);

					_smpno = 0;
					++_write_iter;
					if (_write_iter == _map.end())
						return;
					m = *_write_iter;
				}
				m.abs_sum[0] += fabs((*smp)[0]);
				m.abs_sum[1] += fabs((*smp)[1]);
				m.peak_hi[0] = max(m.peak_hi[0], (*smp)[0]);
				m.peak_hi[1] = max(m.peak_hi[1], (*smp)[1]);
				m.peak_lo[0] = min(m.peak_lo[0], (*smp)[0]);
				m.peak_lo[1] = min(m.peak_lo[1], (*smp)[1]);
				++_smpno;
			}
		}

		const mip& get_mip(int indx)
		{
			return _map[indx];
		}

		double _smp_per_mip;
		typename std::vector<mip> _map;
		typename std::vector<mip>::iterator _write_iter;
		int _smpno;
	};

	MipMap(BufferedStream<typename Source_T::chunk_t> *src, int levels, int displayWidth, pthread_mutex_t *lock) :
		_src(src)
	{
		int chunks = src->len().chunks;
		int smps = src->len().samples;
		assert(chunks != -1 && smps != -1);
		for (int mips = displayWidth, l = 0; l < levels; ++l, mips *= 2)
		{
			pthread_mutex_lock(lock);
			_levels.push_back(new Level(mips, smps));
			pthread_mutex_unlock(lock);
		}

		for (int c = 0; c < chunks; ++c)
		{
			printf("chunk %d of %d\n", c, chunks);
			typename Source_T::chunk_t *chk = _src->get_chunk(c);
			for (std::vector<Level*>::iterator i = _levels.begin(); i != _levels.end(); ++i)
			{
				pthread_mutex_lock(lock);
				(*i)->process_next(chk);
				pthread_mutex_unlock(lock);
			}
		}
	}

	~MipMap()
	{
		for (std::vector<Level*>::iterator i = _levels.begin(); i != _levels.end(); ++i)
			delete (*i);
	}

	Level* GetLevel(int l)
	{
		return _levels[l];
	}

protected:
	BufferedStream<typename Source_T::chunk_t> *_src;
	std::vector<Level*> _levels;
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
		ChunkMetadata():valid(false),subband(10){}
		bool valid;
		Sample_T abs_sum;
		Sample_T avg;
		Sample_T avg_db;
		Sample_T peak;
		Sample_T peak_db;

		struct s
		{
			Sample_T abs_sum;
			Sample_T avg;
			Sample_T avg_db;
			Sample_T peak;
			Sample_T peak_db;
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
			}
			indx=0;
			for (Sample_T *data=chk->_data, *end=data+Chunk_T::chunk_size;
				data != end; )
			{
				i = indx*10/Chunk_T::chunk_size;
				Abs<Sample_T>::calc(dabs, *data);
				Sum<Sample_T>::calc(meta.subband[i].abs_sum, meta.subband[i].abs_sum, dabs);
				SetMax<Sample_T>::calc(meta.subband[i].peak, dabs);
				++data;
				++indx;
			}
			meta.abs_sum[0] = 0.0f;
			meta.abs_sum[1] = 0.0f;
			meta.peak[0] = 0.0f;
			meta.peak[1] = 0.0f;
			for (i=0; i<10; ++i)
			{
				meta.abs_sum[0] += meta.subband[i].abs_sum[0];
				meta.abs_sum[1] += meta.subband[i].abs_sum[1];
				meta.peak[0] = max(meta.peak[0], meta.subband[i].peak[0]);
				meta.peak[1] = max(meta.peak[1], meta.subband[i].peak[1]);

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
