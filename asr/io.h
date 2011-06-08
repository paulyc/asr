#ifndef _IO_H
#define _IO_H

#include <asio.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <objbase.h>
#include <fstream>
#include <queue>
#include <exception>
#include <semaphore.h>

#include "asr.h"
#include "type.h"
#include "filter.h"
#include "buffer.h"
#include "decoder.h"
#include "asiodrv.h"
#include "controller.h"
#include "speedparser.h"
#include "wavformdisplay.h"
#include "track.h"

extern IASIO * asiodrv;

#define ASIO_ASSERT_NO_ERROR(call) { \
	e = call ; \
	if (e != ASE_OK) { \
		printf("Yo error %s\n", asio_error_str(e)); \
		assert(false); \
	} \
}

void bufferSwitch(long doubleBufferIndex, ASIOBool directProcess);
ASIOTime* bufferSwitchTimeInfo(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess);
void sampleRateDidChange(ASIOSampleRate sRate);
long asioMessage(long selector, long value, void *message, double *opt);

template <typename Input_Buffer_T, typename Output_Buffer_T>
class ASIOProcessor;

template <typename Chunk_T, typename Output_Sample_T>
class asio_sink : public T_sink<Chunk_T>
{
public:
	asio_sink(T_source<Chunk_T> *src, Output_Sample_T **bufs1, Output_Sample_T **bufs2, long bufSz) :
	  T_sink<Chunk_T>(src),
	  _buf_size(bufSz)
	{
		_chk = _src->next();
		_read = _chk->_data;
		_buffers[0] = bufs1;
		_buffers[1] = bufs2;
	}
	virtual ~asio_sink()
	{
		T_allocator<Chunk_T>::free(_chk);
	}
	void process(int dbIndex);
protected:
	Chunk_T *_chk;
	typename Chunk_T::sample_t *_read;
	Output_Sample_T **_buffers[2];
	long _buf_size;
};

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
class asio_source : public T_source<Chunk_T>
{
public:
	asio_source();
	~asio_source();
	
	void have_data();
	bool chunk_ready() { return _chks_next.size() > 0; }
	Chunk_T* next();
protected:
	Chunk_T *_chk_working;
	std::queue<Chunk_T *> _chks_next;
	Output_Sample_T *_chk_ptr;
	sem_t _next_sem;
};

template <typename Chunk_T>
class file_raw_output : public T_sink<Chunk_T>
{
protected:
	FILE *_f;
public:
	file_raw_output(T_source<Chunk_T> *src, const char *filename="output.raw") :
	  T_sink(src),
	  _f(fopen(filename, "wb"))
	{
	}
	  ~file_raw_output()
	  {
		  fclose(_f);
	  }
	  bool eof()
	  {
		  return feof(_f);
	  }
	virtual void process()
	{
		Chunk_T *t = _src->next();
		fwrite(t->_data, 8, Chunk_T::chunk_size, _f);
		T_allocator<Chunk_T>::free(t);
	}
};

/*
template <typename T>
class asio_96_24_sink : public asio_sink<T>
{
public:
	void next(T* t)
	{
	}
};
*/

#define CREATE_BUFFERS 1
#define CHOOSE_CHANNELS 0
#define RUN 0
#define INPUT_PERIOD (1.0/44100.0)

#if 0
template <typename T, int n>
class vector_n
{
	vector_n(
	T operator[]
private:
	T _items[n];
};
#endif

template <typename Input_Buffer_T, typename Output_Buffer_T>
class ASIOProcessor
{
public:
	struct sound_stream
	{
		sound_stream(int chan){}
		virtual int num_channels() = 0;
	//	int nChannels;
		int buffer_sz;
	};
	struct input : public sound_stream
	{
	};
	struct output : public sound_stream
	{
	};
	ASIOProcessor();
	virtual ~ASIOProcessor();

	ASIOError Start();
	ASIOError Stop();

	void BufferSwitch(long doubleBufferIndex, ASIOBool directProcess);
	void SetSrc(int ch, const wchar_t *fqpath);

	void SetResamplerate(double rate)
	{
		_resamplerate = rate;
		_my_controller->set_output_sampling_frequency(_resample_filter, _resamplerate);
	}

	void SetPos(double tm)
	{
		_my_controller->set_output_time(_resample_filter, tm);
	}

	typedef SeekablePitchableFileSource<chunk_t> track_t;
	track_t* GetTrack(int t_id)
	{
		return _tracks[t_id-1];
	}

	void SetMix(int m)
	{
		_master->set_mix(m);
	}

	long _doubleBufferIndex;
	bool _running;
	double _speed;
	const wchar_t *_default_src;
	bool _resample;
	float _resamplerate;

public: // was protected
	void Init();
	void LoadDriver();
	void ProcessInput();
	void Destroy();

	const static int inputBuffersize = 4*BUFFERSIZE;
	
	Input_Buffer_T *_inputBufL, *_inputBufR;
	size_t _inputBufLEffectiveSize, _inputBufREffectiveSize;
	Input_Buffer_T *_bufLBegin, *_bufLEnd, *_bufRBegin, *_bufREnd;
	double _inputBufTime;
	
	ASIODriverInfo _drv_info;
	size_t _buffer_infos_len;
	ASIOBufferInfo *_buffer_infos;
	ASIOCallbacks _cb;
	long _numInputChannels;
	long _numOutputChannels;
	ASIOChannelInfo *_input_channel_infos;
	ASIOChannelInfo *_output_channel_infos;
	long _bufSize;
	double _outputTime;
	bool _src_active;

	SpeedParser2<BUFFERSIZE> _sp;
	std::ofstream _log;

	asio_source<short, SamplePairf, chunk_t> *_my_source;
	peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size> *_my_pk_det;
	typedef controller<lowpass_filter_td<chunk_t, double>, 
		peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size> 
		> controller_t;
	controller_t *_my_controller;
	file_raw_output<chunk_t> *_my_raw_output;
	asio_sink<chunk_t, short> *_my_sink;

	std::vector<track_t*> _tracks;
	xfader<track_t> *_master;
	xfader<track_t> *_cue;
	xfader<track_t> *_aux;
};

#endif // !defined(_IO_H)
