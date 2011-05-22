#ifndef _TRACK_H
#define _TRACK_H

#include <pthread.h>

#include "wavfile.h"
#include "buffer.h"
#include "type.h"

template <typename Chunk_T>
class SeekablePitchableFileSource : public T_source<Chunk_T>
{
public:
	SeekablePitchableFileSource(const wchar_t *filename=0) :
		_in_config(false),
		_filename(0),
		_src(0),
		_src_buf(0),
		_resample_filter(0),
		_meta(0),
		_display(0)
	{
		pthread_mutex_init(&_config_lock, 0);
		if (filename)
			set_source_file(filename);
	}

	virtual ~SeekablePitchableFileSource()
	{
		pthread_mutex_destroy(&_config_lock);
	}

	void set_source_file(const wchar_t *filename)
	{
		pthread_mutex_lock(&_config_lock);
		_in_config = true;
		pthread_mutex_unlock(&_config_lock);

		delete _display;
		_display = 0;
		delete _resample_filter;
		_resample_filter = 0;
		delete _meta;
		_meta = 0;
		delete _src_buf;
		_src_buf = 0;
		delete _src;
		_src = 0;

		_filename = filename;

		_src = new wavfile_chunker<Chunk_T>(filename);
		_src_buf = new BufferedStream<Chunk_T>(_src);
		_meta = new StreamMetadata<Chunk_T>(_src_buf);
		_resample_filter = new lowpass_filter_td<Chunk_T, double>(_src_buf, 22050.0, 44100.0, 48000.0);
		_display = new WavFormDisplay<
			StreamMetadata<Chunk_T>, 
			SeekablePitchableFileSource<Chunk_T> >(_meta, this);

		pthread_mutex_lock(&_config_lock);
		_in_config = false;
		pthread_mutex_unlock(&_config_lock);
	}

	Chunk_T* next()
	{
		Chunk_T *chk;
		pthread_mutex_lock(&_config_lock);
		if (_in_config)
			chk = zero_source<Chunk_T>::get()->next();
		else
			chk = _resample_filter->next(); // may want to critical section this
		pthread_mutex_unlock(&_config_lock);
		return chk;
	}

	void set_pitch(double mod)
	{
		set_output_sampling_frequency(48000.0/mod);
	}

	void set_output_sampling_frequency(double f)
	{
		if (!_in_config && _resample_filter)
			_resample_filter->set_output_sampling_frequency(f);
	}

	void seek_time(double t)
	{
		if (!_in_config && _resample_filter)
			_resample_filter->seek_time(t);
	}

protected:
	pthread_mutex_t _config_lock;
	bool _in_config;
	const wchar_t *_filename;
public:
	T_source<Chunk_T> *_src;
	BufferedStream<Chunk_T> *_src_buf;
	lowpass_filter_td<Chunk_T, double> *_resample_filter;
	StreamMetadata<Chunk_T> *_meta;

	typedef WavFormDisplay<
		StreamMetadata<Chunk_T>, 
		SeekablePitchableFileSource<Chunk_T> 
	> display_t;
	display_t *_display;
};

#endif // !defined(TRACK_H)
