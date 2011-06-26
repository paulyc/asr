#ifndef _WAVFILE_H
#define _WAVFILE_H

#include "file.h"
#include <string>
#include <exception>

#define _USE_MATH_DEFINES
#include <cmath>

#include <mad.h>

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

class file_chunker
{
public:
	file_chunker(const wchar_t * filename)
	{
		_file = new DiskFile(filename, L"rb");
	}
	file_chunker(GenericFile *file) :
		_file(file)
	{
	}
	virtual ~file_chunker()
	{
		delete _file;
	}
protected:
	GenericFile * _file;
};

template <typename Chunk_T>
class wavfile_chunker_base : public T_source<Chunk_T>, public file_chunker
{
public:
	wavfile_chunker_base(const wchar_t * filename) :
		file_chunker(filename),
		_eof(false)
	{
		load_header(filename);
	}
	wavfile_chunker_base(GenericFile *file) :
		file_chunker(file),
		_eof(false)
	{
		load_header(L"GenericFile");
	}

	virtual ~wavfile_chunker_base()
	{
	}

	void load_header(const wchar_t * filename)
	{
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

		_len.samples = _dataBytes / (_fmtChk.nChannels*_bytesPerSample);
		_len.chunks = _len.samples / Chunk_T::chunk_size + 1;
		_len.smp_ofs_in_chk = _len.samples % Chunk_T::chunk_size;
		_len.time = _len.samples / double(_fmtChk.sampleRate);
	}

	virtual Chunk_T* next() = 0;

	void seek(int smp_ofs)
	{
		_eof = false;
		_file->seek(_dataOfs + _fmtChk.nChannels*_bytesPerSample*smp_ofs);
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

	virtual double sample_rate()
	{
		return double(_fmtChk.sampleRate);
	}

protected:
	
	RIFFHeader _riffHdr;
	WAVFormatChunk _fmtChk;
	short _bytesPerSample;
	long _dataOfs;
	unsigned long _dataBytes;
	bool _eof;
};

template <typename Chunk_T>
class wavfile_chunker : public wavfile_chunker_base<Chunk_T>
{
public:
	wavfile_chunker(const wchar_t *filename) : 
		wavfile_chunker_base<Chunk_T>(filename)
	{
		// one chunk of data+padding
		_buffer = new char[_bytesPerSample*_fmtChk.nChannels*Chunk_T::chunk_size+4];
	}

	virtual ~wavfile_chunker()
	{
		delete _buffer;
	}

	Chunk_T* next()
	{
		size_t bytes_to_read = _bytesPerSample*_fmtChk.nChannels*Chunk_T::chunk_size, rd;
		Chunk_T* chk = T_allocator<Chunk_T>::alloc();

		rd = _file->read(_buffer, 1, bytes_to_read);
		if (rd < bytes_to_read)
		{
			_eof = true;
			for (short *b=(short*)_buffer+(bytes_to_read-rd)/_bytesPerSample; 
				b < (short*)_buffer+_fmtChk.nChannels*Chunk_T::chunk_size; ++b)
				*b = 0;
		}
		chk->load_from<short>((short*)_buffer);
		return chk;
	}
protected:
	char *_buffer;
};

template <typename Chunk_T>
class mp3file_chunker : public T_source<Chunk_T>, public file_chunker
{
public:
	mp3file_chunker(const wchar_t * filename) :
		file_chunker(filename),
		_smp_read(0),
		_eof(false),
		_max(0.0f),
		_sample_rate(44100.0)
	{
		mad_stream_init(&_stream);
		mad_frame_init(&_frame);
		mad_synth_init(&_synth);
		mad_timer_reset(&_timer);
	}
	~mp3file_chunker()
	{
		mad_synth_finish(&_synth);
		mad_frame_finish(&_frame);
		mad_stream_finish(&_stream);
	}
	Chunk_T* next()
	{
		Chunk_T* chk = T_allocator<Chunk_T>::alloc();
		unsigned n = 0;
		size_t rd;
		
		Chunk_T::sample_t *smp_out = chk->_data;
		Chunk_T::sample_t *smp_end = smp_out+Chunk_T::chunk_size;
		if (_smp_read > 0)
		{
			for (; _smp_read < _synth.pcm.length && smp_out < smp_end; ++_smp_read, ++smp_out)
			{
				(*smp_out)[0] = (float)mad_f_todouble(_synth.pcm.samples[0][_smp_read]);

				if (MAD_NCHANNELS(&_frame.header) == 2)
					(*smp_out)[1] = (float)mad_f_todouble(_synth.pcm.samples[1][_smp_read]);
				else
					(*smp_out)[1] = (*smp_out)[0];

				if (fabs((*smp_out)[0]) > _max)
					_max = fabs((*smp_out)[0]);
				if (fabs((*smp_out)[1]) > _max)
					_max = fabs((*smp_out)[1]);
			}
		}
		if (smp_out == smp_end)
			return chk;

		do
		{
			if (_stream.buffer == NULL || _stream.error == MAD_ERROR_BUFLEN)
			{
				if (_stream.next_frame)
				{
					assert(_stream.bufend >= _stream.next_frame);
					_inputBufferFilled = _stream.bufend - _stream.next_frame;
					memmove(_inputBuffer, _stream.next_frame, _inputBufferFilled);
				}
				else
				{
					_inputBufferFilled = 0;
				}

				rd = _file->read((char*)_inputBuffer+_inputBufferFilled, 1, INPUT_BUFFER_SIZE-_inputBufferFilled);
				if (rd <= 0)
				{
					_eof = true;
					break;
				}
				_inputBufferFilled += rd;

				if (_file->eof())
				{
					memset(_inputBuffer+_inputBufferFilled, 0, MAD_BUFFER_GUARD);
					_inputBufferFilled += MAD_BUFFER_GUARD;
				}

				mad_stream_buffer(&_stream, _inputBuffer, _inputBufferFilled);
				_stream.error = MAD_ERROR_NONE;
			}

			if (mad_frame_decode(&_frame, &_stream))
			{
				// error
				if (MAD_RECOVERABLE(_stream.error))
					continue;
				else
					if (_stream.error == MAD_ERROR_BUFLEN)
						continue;
					else
						throw std::exception("fatal error during MPEG decoding");
			}

			_sample_rate = (double)_frame.header.samplerate;

			mad_timer_add(&_timer, _frame.header.duration);
			
			mad_synth_frame(&_synth, &_frame);
			for (_smp_read = 0; _smp_read < _synth.pcm.length && smp_out < smp_end; ++_smp_read, ++smp_out)
			{
				(*smp_out)[0] = (float)mad_f_todouble(_synth.pcm.samples[0][_smp_read]);

				if (MAD_NCHANNELS(&_frame.header) == 2)
					(*smp_out)[1] = (float)mad_f_todouble(_synth.pcm.samples[1][_smp_read]);
				else
					(*smp_out)[1] = (*smp_out)[0];
				
				if (fabs((*smp_out)[0]) > _max)
					_max = fabs((*smp_out)[0]);
				if (fabs((*smp_out)[1]) > _max)
					_max = fabs((*smp_out)[1]);

				++n;
			}
		} while (n == 0 || smp_out < smp_end);
		for (; smp_out < smp_end; ++smp_out)
			Zero<Chunk_T::sample_t>::set(*smp_out);
		return chk;
	}

	bool eof()
	{
		return _eof;
	}
	float maxval()
	{
		return _max;
	}
	virtual double sample_rate()
	{
		return _sample_rate;
	}
private:
	const static int INPUT_BUFFER_SIZE = 5*8192;
	mad_stream _stream;
	mad_frame _frame;
	mad_synth _synth;
	mad_timer_t _timer;

	unsigned char _inputBuffer[INPUT_BUFFER_SIZE+MAD_BUFFER_GUARD];
	unsigned long _inputBufferFilled;
	unsigned short _smp_read;

	bool _eof;

	float _max;
	double _sample_rate;
};

#include <FLAC/stream_decoder.h>

template <typename Chunk_T>
class flacfile_chunker : public T_source<Chunk_T>, public file_chunker
{
public:
	flacfile_chunker(const wchar_t * filename) :
		file_chunker(filename),
		_decoder(0)
	{
		if((_decoder = FLAC__stream_decoder_new()) == NULL) {
			delete _file;
			_file = 0;
			throw std::exception("ERROR: allocating decoder");
		}

		if(FLAC__stream_decoder_init_FILE(_decoder, 
			_file, write_callback, metadata_callback, 
			error_callback, (void*)this) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		{
			delete _file;
			_file = 0;
			throw std::exception("ERROR: initializing decoder");
		}

		FLAC__stream_decoder_process_until_end_of_metadata(_decoder);
	}

	virtual ~flacfile_chunker()
	{
	//	FLAC__stream_decoder_delete
	}

	static FLAC__StreamDecoderWriteStatus write_callback(
		const FLAC__StreamDecoder *decoder, 
		const FLAC__Frame *frame, 
		const FLAC__int32 * const buffer[], 
		void *client_data)
	{
	}

	static void metadata_callback(const FLAC__StreamDecoder *decoder, 
		const FLAC__StreamMetadata *metadata, void *client_data)
	{
	}

	static void error_callback(const FLAC__StreamDecoder *decoder, 
		FLAC__StreamDecoderErrorStatus status, void *client_data)
	{
	}

	Chunk_T* next()
	{
		Chunk_T* chk = T_allocator<Chunk_T>::alloc();
		return chk;
	}

protected:
	FLAC__StreamDecoder *_decoder;
};

#endif
