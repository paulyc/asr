#ifndef _WAVFILE_H
#define _WAVFILE_H

#include "file.h"
#include <string>

#define _USE_MATH_DEFINES
#include <cmath>

struct RIFFHeader
{
	long riff;
	unsigned long filelen;
	long fileid;
};

struct RIFFChunkHeader
{
	long id;
	unsigned long len;
};

struct WAVFormatChunk
{
	short format;
	short nChannels;
	long sampleRate;
	long byteRate;
	short blockAlign;
	short bitsPerSample;
};

inline const char * cwcs_to_ccs(const wchar_t *str)
{
	static char buf[256];
	wcstombs(buf, str, sizeof(buf));
	return buf;
}

template <typename Chunk_T>
class wavfile_chunker_base : public T_source<Chunk_T>
{
public:
	wavfile_chunker_base(const wchar_t * filename) :
		_eof(false)
	{
		_file = new DiskFile(filename, L"rb");
		_file->read(&_riffHdr, sizeof(_riffHdr), 1);
		if (_riffHdr.riff != 'FFIR' || _riffHdr.fileid != 'EVAW')
		{
			delete _file;
			_file = 0;
			throw std::exception(cwcs_to_ccs((std::wstring(filename) + L" is not a valid WAVE file").c_str()));
		}

		RIFFChunkHeader ch;
		_file->read(&ch, sizeof(ch), 1);
		if (ch.id != ' tmf' && ch.id != '_tmf')
		{
			delete _file;
			_file = 0;
			throw std::exception(cwcs_to_ccs((std::wstring(filename) + L" is not a valid WAVE file: format chunk not found").c_str()));
		}

		if (ch.len != sizeof(WAVFormatChunk))
		{
			delete _file;
			_file = 0;
			throw std::exception("Can't handle this type of WAVE file");
		}

		_file->read(&_fmtChk, sizeof(_fmtChk), 1);
		if (_fmtChk.format != 0x0001)
		{
			delete _file;
			_file = 0;
			throw std::exception("Can't handle this type of WAVE file");
		}

		if (_fmtChk.bitsPerSample & 0x7)
		{
			_bytesPerSample = (_fmtChk.bitsPerSample >> 3) + 1;
			throw std::exception("Can't handle bits per sample not a multiple of 8");
		}
		else
		{
			_bytesPerSample = _fmtChk.bitsPerSample >> 3;
		}
		if (_bytesPerSample != 2)
			throw std::exception("Can't handle bits per sample not 16");

		// read next chunk header - should be data
		_file->read(&ch, sizeof(ch), 1);
		if (ch.id != 'atad')
		{
			delete _file;
			_file = 0;
			throw std::exception(cwcs_to_ccs((std::wstring(filename) + L" is not a valid WAVE file: data chunk not found").c_str()));
		}
		_dataOfs = _file->tell();
		_dataBytes = ch.len;

		_len.samples = _dataBytes / (_fmtChk.nChannels*sizeof(short));
		_len.chunks = _len.samples / Chunk_T::chunk_size + 1;
		_len.smp_ofs_in_chk = _len.samples % Chunk_T::chunk_size;
		_len.time = _len.samples / double(_fmtChk.sampleRate);
	}
	wavfile_chunker_base(GenericFile *file) :
		_file(file)
	{
	}

	virtual ~wavfile_chunker_base()
	{
		delete _file;
	}

	virtual Chunk_T* next() = 0;

	void seek(int smp_ofs)
	{
		_eof = false;
		_file->seek(_dataOfs + _fmtChk.nChannels*sizeof(short)*smp_ofs);
	}
	virtual void seek_chk(int chk_ofs)
	{
		smp_ofs_t smp_ofs = chk_ofs * Chunk_T::chunk_size;
		seek(smp_ofs);
	}

	const pos_info& len()
	{
		return _len;
	}

protected:
	GenericFile * _file;
	RIFFHeader _riffHdr;
	WAVFormatChunk _fmtChk;
	short _bytesPerSample;
	long _dataOfs;
	unsigned long _dataBytes;
	bool _eof;
	pos_info _len;
};

template <typename Chunk_T, typename Sample_Output_T>
class wavfile_chunker : public wavfile_chunker_base<Chunk_T>
{
public:
	wavfile_chunker(const wchar_t *filename) : 
		wavfile_chunker_base<Chunk_T>(filename)
	{
	}
};

template <typename Chunk_T>
class wavfile_chunker<Chunk_T, SamplePairf> : public wavfile_chunker_base<Chunk_T>
{
public:
	wavfile_chunker<Chunk_T, SamplePairf>(const wchar_t *filename) : 
		wavfile_chunker_base<Chunk_T>(filename)
	{
	}
	Chunk_T* next()
	{
		short buffer[2*Chunk_T::chunk_size], smp;
		size_t bytes_to_read = _bytesPerSample*2*Chunk_T::chunk_size, rd;
		Chunk_T* chk = T_allocator<Chunk_T>::alloc();

		rd = _file->read(buffer, 1, bytes_to_read);
		if (rd < bytes_to_read)
		{
			_eof = true;
			for (short *b=buffer+(bytes_to_read-rd)/sizeof(short); b < buffer+2*Chunk_T::chunk_size; ++b)
				*b = 0;
		}
		for (int r=0; r < Chunk_T::chunk_size; ++r)
		{
			PairFromT<SamplePairf, short>(chk->_data[r], buffer[r*2], buffer[r*2+1]);
		}
		return chk;
	}
};


/*
template <int chunk_size, int channels>
class wavfile_chunker_T_N<chunk_time_domain_2d<fftwf_complex, 2, chunk_size>, chunk_size, channels> : public wavfile_chunker_T_N_base<chunk_time_domain_2d<fftwf_complex, 2, chunk_size>, chunk_size, channels>
{
public:
	wavfile_chunker_T_N<chunk_time_domain_2d<fftwf_complex, 2, chunk_size>, chunk_size, channels>(const wchar_t *filename) : 
		wavfile_chunker_T_N_base<chunk_time_domain_2d<fftwf_complex, 2, chunk_size>, chunk_size, channels>(filename)
	{
	}
	chunk_time_domain_2d<fftwf_complex, 2, chunk_size>* next()
	{
		short buffer[2*chunk_size];
		size_t bytes_to_read = _bytesPerSample*2*chunk_size, rd;
		chunk_time_domain_2d<fftwf_complex, 2, chunk_size>* chk = T_allocator<chunk_time_domain_2d<fftwf_complex, 2, chunk_size> >::alloc();

		rd = _file->read(buffer, 1, bytes_to_read);
		if (rd < bytes_to_read)
		{
			_eof = true;
			for (short *b=buffer+(bytes_to_read-rd)/sizeof(short); b < buffer+2*chunk_size; ++b)
				*b = 0;
		}
		for (int r=0; r < 2; ++r)
		{
			for (int c=0; c < chunk_size; ++c)
			{
				fftwf_complex &cpx = chk->dereference(r, c);
				cpx[0] = buffer[r*2+c] * (1.0f/float(0x7FFF));
				cpx[1] = 0.0f;
			}
		}
		return chk;
	}
};
*/
/*
//note: this has output interleaved, and only works for 16bit samples
template <typename Output_T, int buffer_size, int channels>
class wavfile_chunker_T_N<chunk_time_domain_2d<Output_T, buffer_size, channels>, buffer_size, channels> : public wavfile_chunker_T_N_base<chunk_time_domain_2d<Output_T, buffer_size, channels>, buffer_size, channels>
{
public:
	double tm;
	wavfile_chunker_T_N<chunk_time_domain_2d<Output_T, buffer_size, channels>, buffer_size, channels>(const wchar_t *filename) : 
		wavfile_chunker_T_N_base<chunk_time_domain_2d<Output_T, buffer_size, channels>, buffer_size, channels>(filename),
		tm(0.0)
	{
	}
	chunk_time_domain_2d<Output_T, buffer_size, channels>* next()
	{
		short buffer[channels*buffer_size], smp;
		size_t bytes_to_read = _bytesPerSample*channels*buffer_size, rd;
		chunk_time_domain_2d<Output_T, buffer_size, channels>* chk = T_allocator<chunk_time_domain_2d<Output_T, buffer_size, channels> >::alloc();

		rd = _file->read(buffer, 1, bytes_to_read);
		if (rd < bytes_to_read)
		{
			for (short *b=buffer+(bytes_to_read-rd)/sizeof(short); b < buffer+channels*buffer_size; ++b)
				*b = 0;
		}
		for (int r=0; r < buffer_size; ++r)
		{
			for (int c=0; c < channels; ++c)
			{
			//	float &f = chk->dereference(r,c);
			//	f = buffer[r*2+c] * (1.0f/float(0x7FFF));
				smp = buffer[r*channels+c];
				if (smp >= 0)
				{
					chk->_data[r*channels+c] = buffer[r*channels+c] * (Output_T(1.0)/Output_T(SHRT_MAX));
				}
				else
				{
					chk->_data[r*channels+c] = buffer[r*channels+c] * (Output_T(-1.0)/Output_T(SHRT_MIN));
				}
			
				if (chk->_data[r*channels+c] > Output_T(1.0) || chk->_data[r*channels+c] < Output_T(-1.0))
				{
					printf("input value is ending up as %f at tm %f\n", float(chk->_data[r*channels+c]), tm);

				}
				float p = 2*M_PI*1000.0f*tm;
				chk->_data[r*channels+c] = sin(p);
			}
			tm += 1./44100.0;
		}

		return chk;
	}
};
*/

/*
template <typename T, int chunk_size>
class wavfile_chunker_T_N : public wavfile_chunker_T_N_base<chunk_T_T_size<T, 2*chunk_size>, chunk_size>
{
public:
	virtual void ld_data(T *lbuf, T *rbuf) = 0;
};

template <typename T, int chunk_size>
class int_N_wavfile_chunker_T<short, chunk_size> : public wavfile_chunker_T_N<short, chunk_size>
{
public:
	int_N_wavfile_chunker_T<short, chunk_size>(const wchar_t *filename) :
		wavfile_chunker_T_N<short, chunk_size>(filename)
	{
	}

	virtual chunk_T_T_size<short, 2*chunk_size>* next()
	{
		short buf[2*chunk_size];
		chunk_T_T_size<short, 2*chunk_size> *chk = T_allocator<chunk_T_T_size<short, 2*chunk_size> >::alloc();
		short *lbuf = chk->_data, *rbuf = chk->_data + chunk_size;
		_file->read(buf, sizeof(short), 2*chunk_size);
		for (int i = 0; i < chunk_size; ++i)
		{
			*lbuf++ = buf[i*2];
			*rbuf++ = buf[i*2+1];
		}
		return chk;
	}

	virtual void ld_data(short *lbuf, short *rbuf)
	{
		short buf[2*chunk_size];
		_file->read(buf, sizeof(short), 2*chunk_size);
		for (int i = 0; i < chunk_size; ++i)
		{
			*lbuf++ = buf[i*2];
			*rbuf++ = buf[i*2+1];
		}
	}
};

template <typename T, int chunk_size>
class int_N_wavfile_chunker_T<int, chunk_size> : public wavfile_chunker_T_N<int, chunk_size>
{
public:
	int_N_wavfile_chunker_T<int, chunk_size>(const wchar_t *filename) :
		wavfile_chunker_T_N<int, chunk_size>(filename)
	{
	}

	virtual chunk_T_T_size<int, 2*chunk_size>* next()
	{
		short buf[2*chunk_size];
		chunk_T_T_size<int, 2*chunk_size> *chk = T_allocator<chunk_T_T_size<int, 2*chunk_size> >::alloc();
		int *lbuf = chk->_data, *rbuf = chk->_data + chunk_size;
		_file->read(buf, sizeof(short), 2*chunk_size);
		for (int i = 0; i < chunk_size; ++i)
		{
			*lbuf++ = ((int)buf[i*2]) << 16;
			*rbuf++ = ((int)buf[i*2+1]) << 16;
		}
		return chk;
	}

	virtual void ld_data(int *lbuf, int *rbuf)
	{
		short buf[2*chunk_size];
		_file->read(buf, sizeof(short), 2*chunk_size);
		for (int i = 0; i < chunk_size; ++i)
		{
			*lbuf++ = ((int)buf[i*2]) << 16;
			*rbuf++ = ((int)buf[i*2+1]) << 16;
		}
	}
};

template <typename T, int chunk_size>
class int_N_wavfile_chunker_T_test : public wavfile_chunker_T_N<T, chunk_size>
{
public:
	int_N_wavfile_chunker_T_test() :
		int_N_wavfile_chunker_T_base<T, chunk_size>(new TestFile<T>)
	{
	}
	chunk_T_T_size<T, 2*chunk_size>* next()
	{
		chunk_T_T_size<T, 2*chunk_size> *chk = T_allocator<chunk_T_T_size<T, 2*chunk_size> >::alloc();
		_file->read(chk->_data, sizeof(T), 2*chunk_size);
		return chk;
	}
	void ld_data(T *lbuf, T *rbuf)
	{
		_file->read(lbuf, sizeof(T), chunk_size);
		_file->read(rbuf, sizeof(T), chunk_size);
	}
};
*/
/*
template <typename Sample_T, typename Chunk_T>
class wavfile_writer : public T_sink<Chunk_T>
{
public:
	wavfile_writer(T_source<Chunk_T> *src
};
*/
#endif
