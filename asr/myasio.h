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

template <typename Chunk_T, typename Source_T, typename Output_Sample_T>
class asio_sink : public T_sink_sourceable<Chunk_T>
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
		_bufL = new short[_buf_size];
		_bufR = new short[_buf_size];
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
	short *_bufL;
    short *_bufR;
};

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
class asio_source : public T_source<Chunk_T>
{
public:
	typedef typename Chunk_T chunk_t;
	asio_source();
	~asio_source();
	
	void copy_data(size_t buf_sz, short *bufL, short *bufR);
	bool chunk_ready() { return _chks_next.size() > 0; }
	Chunk_T* next();
protected:
	Chunk_T *_chk_working;
	std::queue<Chunk_T *> _chks_next;
	Output_Sample_T *_chk_ptr;
	sem_t _next_sem;
};

#endif // !defined(_MYASIO_H)
