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
class ASIOThinger;

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T, int chunk_size, bool interleaved_input=true>
class asio_sink : public T_sink<Chunk_T>
{
public:
	asio_sink(T_source<Chunk_T> *src) :
	  T_sink<Chunk_T>(src)
	{
		_chk = _src->next();
		_read = _chk->_data;
	}
	void process();
protected:
	Chunk_T *_chk;
	Input_Sample_T *_read;
};

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T, int chunk_size>
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
		const int *n = t->sizes_as_array();
		//for (int i = 0; i < n[0]; ++i)
	//		fwrite(t->_data + i*2, sizeof(float), 1, _f);
		fwrite(t->_data, 8, n[0]*n[1], _f);
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

template <typename Input_Buffer_T, typename Output_Buffer_T>
class ASIOThinger
{
public:
	ASIOThinger();
	virtual ~ASIOThinger();

	ASIOError Start();
	ASIOError Stop();

	void CopyBuffer(Output_Buffer_T *bufOut, Input_Buffer_T *copyFromBuf, Input_Buffer_T *copyFromEnd, float resamplerate);
	
	void BufferSwitch(long doubleBufferIndex, ASIOBool directProcess);

	void CheckBuffers(float resamplerate);
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

	long _doubleBufferIndex;
	bool _running;
	double _speed;
	const wchar_t *_default_src;
	bool _resample;
	float _resamplerate;
	//lowpass_filter_td<fftwf_complex, default_internal_chunk_type, float> *_resample_filter;
	lowpass_filter_td<SamplePairf, chunk_time_domain_2d<SamplePairf, 4096, 1>, double> *_resample_filter;

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
	my_wavfile_chunker *_src1;
	wavfile_chunker_T_N<chunk_time_domain_2d<SamplePairf, 4096, 1>, 4096, 1> *_src2;
	BufferedStream<chunk_time_domain_2d<SamplePairf, 4096, 1>, SamplePairf, 4096> *_src_buf;
	bool _src1_active;

	SpeedParser2<BUFFERSIZE> _sp;
	std::ofstream _log;

	asio_source<short, SamplePairf, chunk_time_domain_2d<SamplePairf, 4096, 1>, 4096> *_my_source;
	peak_detector<SamplePairf, chunk_time_domain_2d<SamplePairf, 4096, 1>, 4096> *_my_pk_det;
	controller<lowpass_filter_td<SamplePairf, chunk_time_domain_2d<SamplePairf, 4096, 1>, double>, 
			   peak_detector<SamplePairf, chunk_time_domain_2d<SamplePairf, 4096, 1>, 4096> 
			   > *_my_controller;
	file_raw_output<chunk_time_domain_2d<SamplePairf, 4096, 1> > *_my_raw_output;
	asio_sink<SamplePairf, short, chunk_time_domain_2d<SamplePairf, 4096, 1>, 4096, true> *_my_sink;
};

#endif // !defined(_IO_H)
