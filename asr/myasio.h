#ifndef _MYASIO_H
#define _MYASIO_H

#include <semaphore.h>

class ASIOProcessor;

class MyASIOCallbacks
{
public:
	static void bufferSwitch(long doubleBufferIndex, ASIOBool directProcess);
	static ASIOTime* bufferSwitchTimeInfo(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess);
	static void sampleRateDidChange(ASIOSampleRate sRate);
	static long asioMessage(long selector, long value, void *message, double *opt);
};

class IOInput
{
public:
	virtual void process(int doubleBufferIndex) = 0;
};

class IOOutput
{
public:
	virtual ~IOOutput() {}

	virtual void process() = 0;
	virtual void switchBuffers(int dbIndex) = 0;
};

class IASIOSink : public IOOutput
{
};

class IASIOSource : public IOInput
{
public:
	virtual ~IASIOSource() {}

	virtual void copy_data(long doubleBufferIndex) = 0;
	virtual bool chunk_ready() = 0;
};

template <typename Chunk_T, typename Source_T, typename Output_Sample_T>
class asio_sink : public IASIOSink, public T_sink_sourceable<Chunk_T>
{
public:
	asio_sink(Source_T *src, Output_Sample_T **bufs1, Output_Sample_T **bufs2, long bufSz) :
	  T_sink_sourceable<Chunk_T>(src),
	  _buf_size(bufSz),
	  _src_t(src)
	{
	//	_chk = _src->next();
	//	_read = _chk->_data;
		_chk = 0;
		_read = 0;
		_buffers[0] = bufs1;
		_buffers[1] = bufs2;
		_bufL = new Output_Sample_T[_buf_size];
		_bufR = new Output_Sample_T[_buf_size];
	}
	virtual ~asio_sink()
	{
		delete [] _bufR;
		delete [] _bufL;
		T_allocator<Chunk_T>::free(_chk);
	}
    void process();
	void switchBuffers(int dbIndex);
protected:
	Chunk_T *_chk;
	typename Chunk_T::sample_t *_read;
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
	typedef typename Chunk_T chunk_t;
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

#endif // !defined(_MYASIO_H)
