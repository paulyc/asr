// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef _META_H
#define _META_H

#include <sched.h>

template <typename Chunk_T>
class StreamMetadata
{
public:
	typedef Chunk_T chunk_t;
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

	void load_metadata(Lock_T *lock, ASIOProcessor *io)
	{
		for (int chk_ofs=0; chk_ofs < _src->len().chunks; ++chk_ofs)
		{
			get_metadata(chk_ofs, lock);
			sched_yield();
		}
	}

	const ChunkMetadata& get_metadata(smp_ofs_t chk_ofs, Lock_T *lock=0)
	{
		int i, indx;
		if (chk_ofs < 0)
		{
			throw string_exception("cant handle chunk ofs < 0");
		}

		usmp_ofs_t uchk_ofs = (usmp_ofs_t)chk_ofs;
		if (_chk_data.size() <= uchk_ofs)
		{
			_chk_data.resize(uchk_ofs*2);
		}
		ChunkMetadata &meta = _chk_data[uchk_ofs];
		Sample_T dabs;
		if (!meta.valid)
		{
		//	if (lock) pthread_mutex_lock(lock);
			Chunk_T *chk = _src->get_chunk(uchk_ofs);
		//	if (lock) pthread_mutex_unlock(lock);

			CRITICAL_SECTION_GUARD(lock);
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
				meta.peak[0] = fmax(meta.peak[0], meta.subband[i].peak[0]);
				meta.peak[1] = fmax(meta.peak[1], meta.subband[i].peak[1]);
				meta.peak_lo[0] = fmin(meta.peak_lo[0], meta.subband[i].peak_lo[0]);
				meta.peak_lo[1] = fmin(meta.peak_lo[1], meta.subband[i].peak_lo[1]);

				Quotient<Sample_T>::calc(meta.subband[i].avg, meta.subband[i].abs_sum, Chunk_T::chunk_size/10);
				dBm<Sample_T>::calc(meta.subband[i].avg_db, meta.subband[i].avg);
				dBm<Sample_T>::calc(meta.subband[i].peak_db, meta.subband[i].peak);
			}
			Quotient<Sample_T>::calc(meta.avg, meta.abs_sum, (float32_t)Chunk_T::chunk_size);
			dBm<Sample_T>::calc(meta.avg_db, meta.avg);
			dBm<Sample_T>::calc(meta.peak_db, meta.peak);
			meta.valid = true;
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
