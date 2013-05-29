#ifndef _STREAM_H
#define _STREAM_H

#include <queue>
#include <pthread.h>

#include "malloc.h"
#include "util.h"
#include "lock.h"

template <typename T>
class T_source
{
public:
	typedef T chunk_t;
	typedef T type;
	virtual ~T_source()
	{
	}
	virtual T* next()
	{
		return T_allocator<T>::alloc();
	}
	virtual void seek_chk(int chk_ofs)
	{
		seek_smp(chk_ofs*T::chunk_size);
	}
	virtual void seek_smp(smp_ofs_t smp_ofs)
	{
		throw string_exception("not implemented");
	}
	virtual bool eof()
	{
		throw string_exception("not implemented");
	}
	virtual double sample_rate()
	{
		return 44100.0;
	}
	
	virtual float maxval()
	{
		return 1.0f;
	}

	struct pos_info
	{
		pos_info() : samples(-1), chunks(-1), smp_ofs_in_chk(-1), time(HUGE_VAL) {}
		smp_ofs_t samples;
		smp_ofs_t chunks;
		smp_ofs_t smp_ofs_in_chk;
		double time;
	};

	virtual pos_info& len()
	{
		return _len;
	}
protected:
	pos_info _len;
};

template <typename T>
class T_sink
{
protected:
	T_source<T> *_src;
public:
	T_sink(T_source<T> *src=0) :
	  _src(src)
	{
	}

	virtual ~T_sink()
	{
	}

	virtual T* process(bool freeme=true)
	{
		return process(_src->next(), freeme);
	}

	virtual T* process(T *t, bool freeme=true)
	{
		if (freeme)
		{
			T_allocator<T>::free(t);
			return 0;
		}
		return t;
	}

	void set_src(T_source<T> *s)
	{
		_src = s;
	}
};

template <typename T>
class T_sink_sourceable : public T_sink<T>
{
public:
	T_sink_sourceable(T_source<T> *src) :
	  T_sink<T>(src)
	{
	}
	virtual ~T_sink_sourceable()
	{
	}
	void set_source(T_source<T> *src)
	{
		_src_lock.acquire();
		this->_src = src;
		_src_lock.release();
	}
protected:
	Lock_T _src_lock;
};

template <typename T>
class T_sink_source : public T_source<T>, public T_sink<T>
{
public:
	T_sink_source(T_source<T> *src=0) :
	  T_sink<T>(src)
	{
	}
	virtual T* next()
	{
		return this->_src->next();
	}
	virtual bool eof()
	{
		return this->_src->eof();
	}
	virtual typename T_source<T>::pos_info& len()
	{
		return this->_src->len();
	}
};

template <typename T>
class QueueingSource : public T_sink_source<T>
{
public:
	QueueingSource(T_source<T> *src) : T_sink_source<T>(src) {}

	~QueueingSource()
	{
		reset_source(0);
	}

	void reset_source(T_source<T> *src)
	{
		while (!_t_q.empty())
		{
			T_allocator<T>::free(_t_q.front());
			_t_q.pop();
		}
		this->_src = src;
	}

	T* get_next()
	{
		T *n = _t_q.front();
		_t_q.pop();
		return n;
	}

	T *next() 
	{ 
		T *process_chk = this->_src->next();
		T *passthru_chk = T_allocator<T>::alloc();
		memcpy(passthru_chk->_data, process_chk->_data, T::chunk_size*sizeof(SamplePairf));
		_t_q.push(passthru_chk);
		return process_chk; 
	}
private:
	std::queue<T*> _t_q;
};

template <typename T>
class zero_source : public T_source<T>
{
protected:
	zero_source() {}
	static zero_source *_the_src;
	static pthread_once_t _once_control;
public:
	static zero_source* get()
	{
		pthread_once(&_once_control, init);
		return _the_src;
	}

	static void init()
	{
		if (!_the_src)
			_the_src = new zero_source;
	}

	T* next()
	{
		T* chk = T_allocator<T>::alloc();
		for (typename T::sample_t *smp = chk->_data, *end=smp+T::chunk_size;
			smp != end; ++smp)
		{
			Zero<typename T::sample_t>::set(*smp);
		}
		return chk;
	}
	typedef T chunk_t;
};

template <typename T>
class VectorSource : public T_source<T>
{
public:
    VectorSource<T>(int sz) : _vec(sz), _readIndx(0), _writeIndx(0) {}
    T *next()
    {
        if (_readIndx >= _vec.size())
        {
            return zero_source<T>::get()->next();
        }
        else
        {
            return _vec[_readIndx++];
        }
    }
    void add(T *t)
    {
        _vec[_writeIndx++] = t;
    }
    void set(int i, T *t)
    {
        _vec[i] = t;
    }
    T* get(int i)
    {
        return _vec[i];
    }

    size_t size() const
    {
        return _vec.size();
    }
private:
    std::vector<T*> _vec;
    int _readIndx;
    int _writeIndx;
};

template <typename T>
zero_source<T>* zero_source<T>::_the_src = 0;

template <typename T>
pthread_once_t zero_source<T>::_once_control = PTHREAD_ONCE_INIT;

template <typename Chunk_T>
class file_raw_output : public T_sink_sourceable<Chunk_T>
{
protected:
	FILE *_f;
public:
	file_raw_output(T_source<Chunk_T> *src, const char *filename="output.raw") :
	  T_sink_sourceable<Chunk_T>(src)
	{
		_f = fopen(filename, "wb");
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
		Chunk_T *t = this->_src->next();
		fwrite(t->_data, Chunk_T::sample_size, Chunk_T::chunk_size, _f);
		T_allocator<Chunk_T>::free(t);
	}
};

template <typename T>
class IChunkGeneratorCallback
{
public:
    virtual void lock(int id) = 0;
    virtual void unlock(int id) = 0;
};

template <typename T>
class IChunkGeneratorLoop
{
public:
    virtual T *get() = 0;
};

class ChunkGenerator : public IChunkGeneratorCallback<chunk_t>
{
public:
    ChunkGenerator(int bufferSizeFrames, Lock_T *ioLock) : _ioLock(ioLock), _lockMask(0)
    {
        _chunksToBuffer = bufferSizeFrames / chunk_t::chunk_size;
        if (bufferSizeFrames % chunk_t::chunk_size > 0)
            ++_chunksToBuffer;
    }
    
    void AddChunkSource(T_source<chunk_t> *src, int id);
    chunk_t* GetNextChunk(int streamID);
    virtual void lock(int id);
    virtual void unlock(int id);
private:

    std::unordered_map<int, IChunkGeneratorLoop<chunk_t>*> _streams;
    int _chunksToBuffer;
    Lock_T _lock;
    Lock_T *_ioLock;
    int _lockMask;
};

class BlockingChunkStream : public T_source<chunk_t>
{
public:
    BlockingChunkStream(ChunkGenerator *gen, int streamID) : _gen(gen), _streamID(streamID) {}
    chunk_t *next()
    {
        return _gen->GetNextChunk(_streamID);
    }
private:
    ChunkGenerator *_gen;
    int _streamID;
};

template <typename Input_Chunk_T, typename Output_Chunk_T>
class ChunkConverter : public T_source<Output_Chunk_T>, public T_sink<Input_Chunk_T>
{
public:
	ChunkConverter(T_source<Input_Chunk_T> *src) : T_sink<Input_Chunk_T>(src) {}
	Output_Chunk_T *next() { return 0; }
};

template <int chunk_size>
class ChunkConverter<chunk_time_domain_1d<SamplePairf, chunk_size>, chunk_time_domain_1d<SamplePairInt16, chunk_size> >  :
public T_source<chunk_time_domain_1d<SamplePairInt16, chunk_size> >,
public T_sink<chunk_time_domain_1d<SamplePairf, chunk_size> >
{
	typedef chunk_time_domain_1d<SamplePairf, chunk_size> input_chunk_t;
	typedef chunk_time_domain_1d<SamplePairInt16, chunk_size> output_chunk_t;
public:
	ChunkConverter(T_source<input_chunk_t> *src) : T_sink<chunk_time_domain_1d<SamplePairf, chunk_size> >(src) {}
	output_chunk_t *next()
	{
		input_chunk_t *in_chk = this->_src->next();
		SamplePairf *in_data = in_chk->_data;
		float in;
		output_chunk_t *out_chk = T_allocator<output_chunk_t>::alloc();
		SamplePairInt16 *out_data = out_chk->_data;
        
		for (int i=0; i<chunk_size; ++i)
		{
			in = in_data[i][0];
			if (in < 0.0f)
				out_data[i][0] = short(fmax(-1.0f, in) * -SHRT_MIN);
			else
				out_data[i][0] = short(fmin(1.0f, in) * SHRT_MAX);
            
			in = in_data[i][1];
			if (in < 0.0f)
				out_data[i][1] = short(fmax(-1.0f, in) * -SHRT_MIN);
			else
				out_data[i][1] = short(fmin(1.0f, in) * SHRT_MAX);
		}
		T_allocator<input_chunk_t>::free(in_chk);
		return out_chk;
	}
};

#endif // !defined(STREAM_H)
