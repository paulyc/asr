#ifndef _MYASIO_H
#define _MYASIO_H

#if WINDOWS

#include <asio.h>
#include <semaphore.h>

#include "AudioDevice.h"

class ASIOProcessor;

class MyASIOCallbacks
{
public:
	static void bufferSwitch(long doubleBufferIndex, ASIOBool directProcess);
	static ASIOTime* bufferSwitchTimeInfo(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess);
	static void sampleRateDidChange(ASIOSampleRate sRate);
	static long asioMessage(long selector, long value, void *message, double *opt);
	static ASIOProcessor *io;
};

class IASIOSource : public AudioInput
{
public:
	virtual ~IASIOSource() {}

	virtual void copy_data(long doubleBufferIndex) = 0;
	virtual bool chunk_ready() = 0;
};

template <typename Chunk_T, typename Source_T, typename Output_Sample_T>
class asio_sink : public AudioOutput
{
public:
	asio_sink(Source_T *src, Output_Sample_T **bufs1, Output_Sample_T **bufs2, long bufSz) :
	  AudioOutput(src),
	  _buf_size(bufSz),
	  _src_t(src)
	{
		_buffers[0] = bufs1;
		_buffers[1] = bufs2;
		_bufL = new Output_Sample_T[_buf_size];
		_bufR = new Output_Sample_T[_buf_size];
	}
	virtual ~asio_sink()
	{
		delete [] _bufR;
		delete [] _bufL;
	}
    void process();
	void switchBuffers(int dbIndex);
protected:
	Output_Sample_T **_buffers[2];
	long _buf_size;
	Source_T *_src_t;
	Output_Sample_T *_bufL;
    Output_Sample_T *_bufR;
};

template <typename Chunk_T>
class ASIOChunkSource : public IASIOSource, public T_source<Chunk_T>
{
public:
	Chunk_T* next() = 0;
};

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
class asio_source : public ASIOChunkSource<Chunk_T>
{
public:
	typedef Chunk_T chunk_t;
	asio_source(T_sink<chunk_t> *sink, size_t buf_sz, Input_Sample_T **bufsL, Input_Sample_T **bufsR);
	~asio_source();
	
	void copy_data(long doubleBufferIndex);
	bool chunk_ready() { return _chks_next.size() > 0; }
	Chunk_T* next();

	void process(int doubleBufferIndex);
protected:
	T_sink<chunk_t> *_sink;
	size_t _buf_sz;
	Input_Sample_T **_bufsL, **_bufsR;

	Chunk_T *_chk_working;
	std::queue<Chunk_T *> _chks_next;
	Output_Sample_T *_chk_ptr;
	sem_t _next_sem;
};

#endif // WINDOWS

#endif // !defined(_MYASIO_H)
