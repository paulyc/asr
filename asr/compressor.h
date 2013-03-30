#ifndef _COMPRESSOR_H
#define _COMPRESSOR_H

template <typename Chunk_T>
class NormalizingCompressor : public T_source<Chunk_T>, public T_sink<Chunk_T>
{
public:
	NormalizingCompressor(T_source<Chunk_T> *src) :
		T_sink(src)
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
			double v = abs((*smp)[0]);
			if (v > max)
				max = v;
			v = abs((*smp)[1]);
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
		Chunk_T *chk = _src->next();
		process(chk);
		return chk;
	}
};

#endif // !defined(_COMPRESSOR_H)
