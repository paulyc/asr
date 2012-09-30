#ifndef _STREAM_H
#define _STREAM_H

template <typename T>
class T_source
{
public:
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
		throw std::exception("not implemented");
	}
	virtual bool eof()
	{
		return false;
	}
	virtual double sample_rate()
	{
		return 44100.0;
	}
	struct pos_info
	{
		pos_info() : samples(-1), chunks(-1), smp_ofs_in_chk(-1), time(HUGE_VAL) {}
		smp_ofs_t samples;
		smp_ofs_t chunks;
		smp_ofs_t smp_ofs_in_chk;
		double time;
	} _len;

	virtual const pos_info& len()
	{
		return _len;
	}

	virtual float maxval()
	{
		return 1.0f;
	}
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

	virtual void process(bool freeme=true)
	{
		process(_src->next(), freeme);
	}

	virtual void process(T *t, bool freeme=true)
	{
		if (freeme) T_allocator<T>::free(t);
	}
};

template <typename T>
class T_sink_sourceable : public T_sink<T>
{
public:
	T_sink_sourceable(T_source<T> *src) :
	  T_sink(src)
	{
		pthread_mutex_init(&_src_lock, 0);
	}
	virtual ~T_sink_sourceable()
	{
		pthread_mutex_destroy(&_src_lock);
	}
	void set_source(T_source<T> *src)
	{
		pthread_mutex_lock(&_src_lock);
		_src = src;
		pthread_mutex_unlock(&_src_lock);
	}
protected:
	pthread_mutex_t _src_lock;
};

template <typename T>
class T_sink_source : public T_source<T>, public T_sink<T>
{
public:
	T_sink_source(T_source<T> *src=0) :
	  T_sink(src)
	{
	}
	virtual bool eof()
	{
		return _src->eof();
	}
	virtual int length_samples()
	{
		return _src->length_samples();
	}
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
	typedef typename T chunk_t;
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
	file_raw_output(T_source<Chunk_T> *src, const wchar_t *filename=L"output.raw") :
	  T_sink_sourceable(src),
	  _f(_wfopen(filename, L"wb"))
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

#endif // !defined(STREAM_H)
