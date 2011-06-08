#ifndef _TRACK_H
#define _TRACK_H

#include <pthread.h>

#include "wavfile.h"
#include "buffer.h"
#include "type.h"
#include "ui.h"
#include "worker.h"

template <typename Chunk_T>
class SeekablePitchableFileSource : public T_source<Chunk_T>
{
	friend class Worker;
public:
	typedef type chunk_t;

	SeekablePitchableFileSource(int track_id, const wchar_t *filename=0) :
		_in_config(false),
		_filename(0),
		_paused(false),
		_src(0),
		_src_buf(0),
		_resample_filter(0),
		_meta(0),
		_display(0),
		_cuepoint(0.0),
		_track_id(track_id)
	{
		pthread_mutex_init(&_config_lock, 0);
		if (filename)
			set_source_file(filename);
	}

	virtual ~SeekablePitchableFileSource()
	{
		pthread_mutex_lock(&_config_lock);
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
		pthread_mutex_unlock(&_config_lock);
		pthread_mutex_destroy(&_config_lock);
	}

	void set_source_file(const wchar_t *filename)
	{
		pthread_mutex_lock(&_config_lock);
		_in_config = true;
		_loaded = false;
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

		if (wcsstr(_filename, L".mp3") == _filename + wcslen(_filename) - 4)
		{
			_src = new mp3file_chunker<Chunk_T>(_filename);
			_src_buf = new BufferedStream<Chunk_T>(_src);
		//	_src_buf->load_complete();
		}
		else
		{
			_src = new wavfile_chunker<Chunk_T>(_filename);
			_src_buf = new BufferedStream<Chunk_T>(_src);
		}
		
		_meta = new StreamMetadata<Chunk_T>(_src_buf);
		_resample_filter = new lowpass_filter_td<Chunk_T, double>(_src_buf, 22050.0, 44100.0, 48000.0);
		_resample_filter->fill_coeff_tbl(); // wtf cause cant call virtual function _h from c'tor
		_display = new WavFormDisplay<
			StreamMetadata<Chunk_T>, 
			SeekablePitchableFileSource<Chunk_T> >(_meta, this);

		pthread_mutex_lock(&_config_lock);
		_in_config = false;
		pthread_mutex_unlock(&_config_lock);

		Worker::do_job(new Worker::load_track_job<SeekablePitchableFileSource<Chunk_T> >(this));
	}

	Chunk_T* next()
	{
		Chunk_T *chk;

		pthread_mutex_lock(&_config_lock);
		if (_in_config || _paused)
		{
			chk = zero_source<Chunk_T>::get()->next();
			/*

		pthread_mutex_unlock(&_config_lock);

		Worker::do_job(
				new Worker::generate_chunk_job<zero_source<Chunk_T> >(
					zero_source<Chunk_T>::get(), &chk),
				true, true);*/
		}
		else
		{
			chk = _resample_filter->next();
			render();
			/*
		pthread_mutex_unlock(&_config_lock);

		Worker::do_job(
				new Worker::generate_chunk_job<lowpass_filter_td<Chunk_T, double> >(
					_resample_filter, &chk), 
				true, true);

		pthread_mutex_lock(&_config_lock);
			set_position(_resample_filter->get_time(), true);
			pthread_mutex_unlock(&_config_lock);
		//	render();
		*/
		}
		pthread_mutex_unlock(&_config_lock);
		return chk;
	}

	int get_display_width()
	{
	//	pthread_mutex_lock(&_config_lock);
		int w=_display->get_width();
	//	pthread_mutex_unlock(&_config_lock);
		return w;
	}

	void set_display_width(int width)
	{
	//	pthread_mutex_lock(&_config_lock);
		_display->set_width(width);
	//	pthread_mutex_unlock(&_config_lock);
	}

	void lock_pos(int y)
	{
		_display->lock_pos(y);
	}

	void unlock_pos()
	{
		_display->unlock_pos();
	}

	void set_wav_heights(bool unlock=true)
	{
		if (unlock)
			pthread_mutex_unlock(&_config_lock);
		_display->set_wav_heights(&_config_lock);
		if (unlock)
			pthread_mutex_lock(&_config_lock);
	}

	bool play_pause()
	{
		pthread_mutex_lock(&_config_lock);
		_paused = !_paused;
		pthread_mutex_unlock(&_config_lock);
		return _paused;
	}

	bool load_step()
	{
		pthread_mutex_lock(&_config_lock);
		if (len().samples < 0)
		{
			_src_buf->load_next();
			pthread_mutex_unlock(&_config_lock);
			return true;
		}
		if (!_display->set_next_height())
		{
			_loaded = true;
			render();
			_resample_filter->set_output_scale(1.0f / _src->maxval());
			pthread_mutex_unlock(&_config_lock);
			return false;
		}
	//	render();
		pthread_mutex_unlock(&_config_lock);
		return true;
	}

	void lockedcall(void(*f)(int))
	{
		pthread_mutex_lock(&_config_lock);
		f(_track_id);
		pthread_mutex_unlock(&_config_lock);
	}

	void zoom_px(int d)
	{
		_display->zoom_px(d);
		set_wav_heights(false);

		pthread_mutex_lock(&_config_lock);
		render();
		pthread_mutex_unlock(&_config_lock);
	}

	void move_px(int d)
	{
		_display->move_px(d);
		set_wav_heights(false);

		pthread_mutex_lock(&_config_lock);
		render();
		pthread_mutex_unlock(&_config_lock);
	}

	void render()
	{
		set_position(this, _display->get_display_pos(_resample_filter->get_time()), true);
	}

	void set_pitch(double mod)
	{
		set_output_sampling_frequency(48000.0/mod);
	}

	void set_output_sampling_frequency(double f)
	{
		if (!_in_config && _resample_filter)
		{
			printf("output mod %f\n", 48000.0/f);
			_resample_filter->set_output_sampling_frequency(f);
		}
	}

	void seek_time(double t)
	{
		pthread_mutex_lock(&_config_lock);
		if (!_in_config && _resample_filter)
			_resample_filter->seek_time(t);
		render();
		pthread_mutex_unlock(&_config_lock);
	}

	void seek_f(double f)
	{
		pthread_mutex_lock(&_config_lock);
		if (!_in_config && _resample_filter && len().samples > 0)
			_resample_filter->seek_time(f * len().time);
		render();
		pthread_mutex_unlock(&_config_lock);
	}

	double get_display_time(double f)
	{
		return _display->get_display_time(f);
	}

	const pos_info& len()
	{
		return _meta->len();
	}

	void set_cuepoint(double pos)
	{
		_cuepoint = pos;
	}

	double get_cuepoint()
	{
		return _cuepoint;
	}

	double get_cuepoint_pos()
	{
		return _display->get_display_pos(_cuepoint);
	}

	void goto_cuepoint()
	{
		double t = _resample_filter->get_time();
		if (t <= _cuepoint || t - 0.3 > _cuepoint) //debounce (?)
			seek_time(_cuepoint);
	}

	bool loaded()
	{
		return _loaded;
	}

protected:
	pthread_mutex_t _config_lock;
	bool _in_config;
	const wchar_t *_filename;
	bool _paused;
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
	double _cuepoint;
	bool _loaded;
	int _track_id;
};

#endif // !defined(TRACK_H)
