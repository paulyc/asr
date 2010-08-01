#ifndef _WAVFILE_H
#define _WAVFILE_H

#include "file.h"

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

template <typename T, int chunk_size>
class int_N_wavfile_chunker_T_base : public T_source<chunk_T_T_size<T, 2*chunk_size> >
{
public:
	int_N_wavfile_chunker_T_base(const wchar_t * filename)
	{
		_file = new DiskFile(filename, L"rb");
		_file->read(&_riffHdr, sizeof(_riffHdr), 1);
		if (_riffHdr.riff != 'FFIR' || _riffHdr.fileid != 'EVAW')
		{
			delete _file;
			_file = 0;
			throw exception(cwcs_to_ccs((std::wstring(filename) + L" is not a valid WAVE file").c_str()));
		}

		RIFFChunkHeader ch;
		_file->read(&ch, sizeof(ch), 1);
		if (ch.id != ' tmf' && ch.id != '_tmf')
		{
			delete _file;
			_file = 0;
			throw exception(cwcs_to_ccs((std::wstring(filename) + L" is not a valid WAVE file: format chunk not found").c_str()));
		}

		if (ch.len != sizeof(WAVFormatChunk))
		{
			delete _file;
			_file = 0;
			throw exception("Can't handle this type of WAVE file");
		}

		_file->read(&_fmtChk, sizeof(_fmtChk), 1);
		if (_fmtChk.format != 0x0001)
		{
			delete _file;
			_file = 0;
			throw exception("Can't handle this type of WAVE file");
		}

		// read next chunk header - should be data
		_file->read(&ch, sizeof(ch), 1);
		if (ch.id != 'atad')
		{
			delete _file;
			_file = 0;
			throw exception(cwcs_to_ccs((std::wstring(filename) + L" is not a valid WAVE file: data chunk not found").c_str()));
		}

		//m_dataBytes = ch.len;
	}
	int_N_wavfile_chunker_T_base(GenericFile *file) :
		_file(file)
	{
	}

	virtual ~int_N_wavfile_chunker_T_base()
	{
		delete _file;
	}

	virtual chunk_T_T_size<T, 2*chunk_size>* next() = 0;
	virtual void ld_data(T *lbuf, T *rbuf) = 0;

protected:
	GenericFile * _file;
	RIFFHeader _riffHdr;
	WAVFormatChunk _fmtChk;
};

template <typename T, int chunk_size>
class int_N_wavfile_chunker_T : public int_N_wavfile_chunker_T_base<T, chunk_size>
{
};

template <int chunk_size>
class int_N_wavfile_chunker_T<short, chunk_size> : public int_N_wavfile_chunker_T_base<short, chunk_size>
{
public:
	int_N_wavfile_chunker_T<short, chunk_size>(const wchar_t *filename) :
		int_N_wavfile_chunker_T_base<short, chunk_size>(filename)
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

template <int chunk_size>
class int_N_wavfile_chunker_T<int, chunk_size> : public int_N_wavfile_chunker_T_base<int, chunk_size>
{
public:
	int_N_wavfile_chunker_T<int, chunk_size>(const wchar_t *filename) :
		int_N_wavfile_chunker_T_base<int, chunk_size>(filename)
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
class int_N_wavfile_chunker_T_test : public int_N_wavfile_chunker_T_base<T, chunk_size>
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

#endif
