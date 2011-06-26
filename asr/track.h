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

	SeekablePitchableFileSource(int track_id, const wchar_t *filename=0, pthread_mutex_t *lock=0) :
		_in_config(false),
		_filename(0),
		_paused(true),
		_src(0),
		_src_buf(0),
		_resample_filter(0),
		_meta(0),
		_display(0),
		_cuepoint(0.0),
		_track_id(track_id),
		_pitchpoint(48000.0),
		_display_width(-1)
#if USE_NEW_WAVE
		,_mip_map(0)
#endif
	{
		pthread_mutex_init(&_config_lock, 0);
		if (filename)
			set_source_file(filename, lock);
	}

	virtual ~SeekablePitchableFileSource()
	{
		pthread_mutex_lock(&_config_lock);
		delete _display;
		_display = 0;
#if USE_NEW_WAVE
		delete _mip_map;
		_mip_map = 0;
#endif
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

	void set_source_file(const wchar_t *filename, pthread_mutex_t *lock)
	{
		pthread_mutex_lock(&_config_lock);
		_in_config = true;
		_loaded = false;
		_paused = true;
	//	pthread_mutex_unlock(&_config_lock);

		pthread_mutex_lock(lock);
		delete _display;
		_display = 0;
		pthread_mutex_unlock(lock);
#if USE_NEW_WAVE
		pthread_mutex_lock(lock);
		delete _mip_map;
		_mip_map = 0;
		pthread_mutex_unlock(lock);
#endif
		pthread_mutex_lock(lock);
		delete _resample_filter;
		_resample_filter = 0;
		pthread_mutex_unlock(lock);
		pthread_mutex_lock(lock);
		delete _meta;
		_meta = 0;
		pthread_mutex_unlock(lock);
		pthread_mutex_lock(lock);
		delete _src_buf;
		_src_buf = 0;
		pthread_mutex_unlock(lock);
		pthread_mutex_lock(lock);
		delete _src;
		_src = 0;
		pthread_mutex_unlock(lock);

		pthread_mutex_lock(lock);

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

		pthread_mutex_unlock(lock);
		pthread_mutex_lock(lock);
		
		_resample_filter = new lowpass_filter_td<Chunk_T, double>(_src_buf, 22050.0, _src->sample_rate(), 48000.0);
		_resample_filter->fill_coeff_tbl(); // wtf cause cant call virtual function _h from c'tor
#if !USE_NEW_WAVE
		_meta = new StreamMetadata<Chunk_T>(_src_buf);
		_display = new WavFormDisplay<
			StreamMetadata<Chunk_T>, 
			SeekablePitchableFileSource<Chunk_T> >(_meta, this);
#endif

		pthread_mutex_unlock(lock);
		pthread_mutex_lock(lock);

	//	pthread_mutex_lock(&_config_lock);
		_in_config = false;
		pthread_mutex_unlock(&_config_lock);

		pthread_mutex_unlock(lock);

		Worker::do_job(new Worker::load_track_job<SeekablePitchableFileSource<Chunk_T> >(this, lock));
	}

	Chunk_T* next(void *dummy=0)
	{
		Chunk_T *chk;

		if (_in_config || _paused)
		{
			chk = zero_source<Chunk_T>::get()->next();
		}
		else
		{
			pthread_mutex_lock(&_config_lock);
			chk = _resample_filter->next();
			pthread_mutex_unlock(&_config_lock);
			render();
		}
		
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
#if USE_NEW_WAVE
		_display_width = width;
#else
		_display->set_width(width);
#endif
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
		_display->set_wav_heights(&asio->_io_lock);
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

	bool load_step(pthread_mutex_t *lock)
	{
		pthread_mutex_lock(&_config_lock);
#if USE_NEW_WAVE
		if (_src_buf->len().samples < 0)
#else
		if (len().samples < 0)
#endif
		{
			pthread_mutex_lock(lock);
			_src_buf->load_next(lock);
			pthread_mutex_unlock(lock);
			pthread_mutex_unlock(&_config_lock);
			return true;
		}
#if !USE_NEW_WAVE
		pthread_mutex_lock(lock);
		if (!_display->set_next_height())
		{
		//	GenericUI *ui;
			pthread_mutex_unlock(lock);
			_loaded = true;
			if (asio && asio->get_ui())
				asio->get_ui()->render(_track_id);
			_resample_filter->set_output_scale(1.0f / _src->maxval());
			pthread_mutex_unlock(&_config_lock);
			return false;
		}
		pthread_mutex_unlock(lock);
#else
		do
		{
			pthread_mutex_unlock(&_config_lock);
			pthread_mutex_lock(&_config_lock);
		} while (_display_width == -1);

		_mip_map = new MipMap<BufferedStream<Chunk_T> >(_src_buf, 8, _display_width, lock);
		_display = new WavFormDisplay2<MipMap<BufferedStream<Chunk_T> >,
			T_source<Chunk_T>, 
		SeekablePitchableFileSource<Chunk_T> >(_src_buf, _mip_map, _display_width);
		_loaded = true;
		asio->get_ui()->render(_track_id);
		_resample_filter->set_output_scale(1.0f / _src->maxval());
		pthread_mutex_unlock(&_config_lock);
		return false;
#endif
		
	//	render();
		pthread_mutex_unlock(&_config_lock);
		return true;
	}

	void lockedpaint()
	{
		pthread_mutex_lock(&_config_lock);
		asio->get_ui()->render(_track_id);
		pthread_mutex_unlock(&_config_lock);
	}

	void zoom_px(int d)
	{
		_display->zoom_px(d);
		Worker::do_job(new Worker::draw_waveform_job<SeekablePitchableFileSource<Chunk_T> >(this, 0));
	}

	void move_px(int d)
	{
		_display->move_px(d);
		Worker::do_job(new Worker::draw_waveform_job<SeekablePitchableFileSource<Chunk_T> >(this, 0));
	}

	void render()
	{
		asio->get_ui()->set_position(this, _display->get_display_pos(_resample_filter->get_time()), true);
		if (asio->get_ui()->want_render())
			Worker::do_job(new Worker::draw_waveform_job<SeekablePitchableFileSource<Chunk_T> >(this, 0));
	}

	void set_pitch(double mod)
	{
		set_output_sampling_frequency(48000.0/mod);
	}

	double get_pitch()
	{
		return 48000.0/_resample_filter->get_output_sampling_frequency();
	}

	void set_output_sampling_frequency(double f)
	{
		pthread_mutex_lock(&_config_lock);
		if (!_in_config && _resample_filter)
		{
			printf("output mod %f\n", 48000.0/f);
			_resample_filter->set_output_sampling_frequency(f);
		}
		pthread_mutex_unlock(&_config_lock);
	}

	double get_output_sampling_frequency(double f)
	{
		return _resample_filter->get_output_sampling_frequency();
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

	void nudge_time(double dt)
	{
		_resample_filter->seek_time(_resample_filter->get_time()+dt);
	}

	void nudge_pitch(double dp)
	{
		set_pitch(get_pitch()+dp);
	}

	double get_display_time(double f)
	{
		return _display->get_display_time(f);
	}

	const pos_info& len()
	{
		return _src_buf->len();
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

	void set_pitchpoint()
	{
		_pitchpoint = _resample_filter->get_output_sampling_frequency();
	}

	double get_pitchpoint()
	{
		return _pitchpoint;
	}

	void goto_pitchpoint()
	{
		_resample_filter->set_output_sampling_frequency(_pitchpoint);
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

#if USE_NEW_WAVE
	typedef WavFormDisplay2<MipMap<BufferedStream<Chunk_T> >,
			T_source<Chunk_T>, 
		SeekablePitchableFileSource<Chunk_T> > display_t;
#else
	typedef WavFormDisplay<
		StreamMetadata<Chunk_T>, 
		SeekablePitchableFileSource<Chunk_T> 
	> display_t;
#endif
	display_t *_display;
	double _cuepoint;
	bool _loaded;
	int _track_id;
	double _pitchpoint;
	int _display_width;
#if USE_NEW_WAVE
	MipMap<BufferedStream<Chunk_T> > *_mip_map;
#endif
};

#endif // !defined(TRACK_H)
