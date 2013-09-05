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

#ifndef _COMPRESSOR_H
#define _COMPRESSOR_H

template <typename Chunk_T>
class NormalizingCompressor : public T_source<Chunk_T>, public T_sink<Chunk_T>
{
public:
	NormalizingCompressor(T_source<Chunk_T> *src) :
		T_sink<Chunk_T>(src)
	{
	}

	// has no state so really a static operation
	static void process(Chunk_T *chk)
	{
		double max = 0.0;
		for (typename Chunk_T::sample_t *smp = chk->_data; 
			smp < chk->_data + Chunk_T::chunk_size; 
			++smp)
		{
			double v = fabs((*smp)[0]);
			if (v > max)
				max = v;
			v = fabs((*smp)[1]);
			if (v > max)
				max = v;
		}

		double gain;
		if (max > 0.5)
			gain = 1.0 / max;
		else
			gain = 1.0;

		for (typename Chunk_T::sample_t *smp = chk->_data; 
			smp < chk->_data + Chunk_T::chunk_size; 
			++smp)
		{
			(*smp)[0] *= gain;
			(*smp)[1] *= gain;
		}
	}

	Chunk_T *next()
	{
		Chunk_T *chk = this->_src->next();
		process(chk);
		return chk;
	}
};

#endif // !defined(_COMPRESSOR_H)
