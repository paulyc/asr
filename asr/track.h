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
	typedef SeekablePitchableFileSource<Chunk_T> track_t;

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
		_display_width(750)
#if USE_NEW_WAVE
		,_mip_map(0)
#endif
		,_running(true)
	{
		pthread_mutex_init(&_config_lock, 0);
		pthread_mutex_init(&_loading_lock, 0);
		pthread_cond_init(&_track_loaded, 0);
		pthread_mutex_init(&_deferreds_lock, 0);
		pthread_cond_init(&_have_deferred, 0);

		if (filename)
			set_source_file(filename, lock);

		Worker::do_job(new Worker::call_deferreds_job<track_t>(this));
	}

	virtual ~SeekablePitchableFileSource()
	{
		pthread_mutex_destroy(&_loading_lock);
		pthread_cond_destroy(&_track_loaded);
		_running = false;
		pthread_cond_signal(&_have_deferred);

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
		pthread_mutex_lock(&_loading_lock);
		
		_in_config = true;
		_loaded = false;
		_paused = true;
		pthread_mutex_unlock(&_loading_lock);

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

		_meta = new StreamMetadata<Chunk_T>(_src_buf);
		
		_resample_filter = new lowpass_filter_td<Chunk_T, double>(_src_buf, 22050.0, _src->sample_rate(), 48000.0);
		_resample_filter->fill_coeff_tbl(); // wtf cause cant call virtual function _h from c'tor
#if !USE_NEW_WAVE
		_display = new WavFormDisplay<
			StreamMetadata<Chunk_T>, 
			SeekablePitchableFileSource<Chunk_T> >(_meta, this, _display_width);
#endif

		pthread_mutex_unlock(lock);

		pthread_mutex_lock(&_loading_lock);
		_in_config = false;
		pthread_mutex_unlock(&_loading_lock);

		Worker::do_job(new Worker::load_track_job<SeekablePitchableFileSource<Chunk_T> >(this, lock));
	}

	Chunk_T* next(void *dummy=0)
	{
		Chunk_T *chk;

		pthread_mutex_lock(&_loading_lock);
		if (!_loaded || _paused)
		{
			pthread_mutex_unlock(&_loading_lock);
			chk = zero_source<Chunk_T>::get()->next();
		}
		else
		{
			chk = _resample_filter->next();
			pthread_mutex_unlock(&_loading_lock);
			render();
		}
		
		return chk;
	}

	int get_display_width()
	{
	//	pthread_mutex_lock(&_loading_lock);
	//	while (!_loaded)
	//		pthread_cond_wait(&_track_loaded, &_loading_lock);
		int w=_display->get_width();
	//	pthread_mutex_unlock(&_loading_lock);
		return w;
	}

	void set_display_width(int width)
	{
	//	pthread_mutex_lock(&_config_lock);
//#if USE_NEW_WAVE
		_display_width = width;
//#else
		_display->set_width(width);
//#endif
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

	void lock()
	{
		pthread_mutex_lock(&_config_lock);
	}

	void unlock()
	{
		pthread_mutex_unlock(&_config_lock);
	}

	void set_wav_heights(bool unlock=true, bool lock=false)
	{
		if (unlock)
			pthread_mutex_unlock(&_config_lock);
		if (lock)
			pthread_mutex_lock(&_loading_lock);
		//_display->set_wav_heights(&asio->_io_lock);
		_display->set_wav_heights();
		if (lock)
			pthread_mutex_unlock(&_loading_lock);
		if (unlock)
			pthread_mutex_lock(&_config_lock);
	}

	bool play_pause()
	{
		_paused = !_paused;
		return _paused;
	}

	bool load_step(pthread_mutex_t *lock)
	{
	//	pthread_mutex_lock(&_config_lock);
#if USE_NEW_WAVE
		if (_src_buf->len().samples < 0)
#else
		while (len().samples < 0)
#endif
		{
			pthread_mutex_lock(lock);
			_src_buf->load_next(lock);
			pthread_mutex_unlock(lock);
			//pthread_mutex_unlock(&_config_lock);
			//return true;
		}
#if !USE_NEW_WAVE
		_display->set_zoom(100.0);
		_display->set_left(0.0);
		//pthread_mutex_lock(lock);
		_display->set_wav_heights(lock);
	//	if (!_display->set_next_height())
	//	{
		//	GenericUI *ui;
	//		pthread_mutex_unlock(lock);
			_meta->load_metadata(lock);
			
			_resample_filter->set_output_scale(1.0f / _src->maxval());
			//_meta->load_metadata(lock);
			pthread_mutex_lock(&_loading_lock);
			_loaded = true;
			pthread_cond_signal(&_track_loaded);
			pthread_mutex_unlock(&_loading_lock);
			if (asio && asio->get_ui())
				asio->get_ui()->render(_track_id);
			return false;
	//	}
		//pthread_mutex_unlock(lock);
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
	//	printf("track::lockedpaint\n");
		pthread_mutex_lock(&_loading_lock);
		asio->get_ui()->render(_track_id);
		pthread_mutex_unlock(&_loading_lock);
	}

	void zoom_px(int d)
	{
	//	printf("track::zoom_px\n");
		if (!_loaded) return;
	//	pthread_mutex_lock(&_config_lock);
		_display->zoom_px(d);
		Worker::do_job(new Worker::draw_waveform_job<SeekablePitchableFileSource<Chunk_T> >(this, 0));
	//	pthread_mutex_unlock(&_config_lock);
	}

	void move_px(int d)
	{
	//	printf("track::move_px\n");
	//	pthread_mutex_lock(&_config_lock);
		if (!_loaded) return;
		_display->move_px(d);
		Worker::do_job(new Worker::draw_waveform_job<SeekablePitchableFileSource<Chunk_T> >(this, 0));
	//	pthread_mutex_unlock(&_config_lock);
	}

	void render()
	{
	//	printf("track::render\n");
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

	void deferred_call(deferred *d)
	{
		pthread_mutex_lock(&_deferreds_lock);
		_defcalls.push(d);
		pthread_cond_signal(&_have_deferred);
		pthread_mutex_unlock(&_deferreds_lock);
	}

	void set_output_sampling_frequency(double f)
	{
		deferred_call(new deferred1<track_t, double>(this, &track_t::set_output_sampling_frequency_impl, f));
	}

	void set_output_sampling_frequency_impl(double f)
	{
		pthread_mutex_lock(&_loading_lock);
		if (_loaded && _resample_filter)
		{
			printf("output mod %f\n", 48000.0/f);
			_resample_filter->set_output_sampling_frequency(f);
		}
		pthread_mutex_unlock(&_loading_lock);
	}

	double get_output_sampling_frequency(double f)
	{
		return _resample_filter->get_output_sampling_frequency();
	}

	void seek_time(double t)
	{
		deferred_call(new deferred1<track_t, double>(this, &track_t::seek_time_impl, t));
	}

	void seek_time_impl(double t)
	{
		pthread_mutex_lock(&_loading_lock);
		if (_loaded && _resample_filter)
			_resample_filter->seek_time(t);
		render();
		pthread_mutex_unlock(&_loading_lock);
	}

	void seek_f(double f)
	{
		deferred_call(new deferred1<track_t, double>(this, &track_t::seek_f_impl, f));
	}

	void seek_f_impl(double f)
	{
		pthread_mutex_lock(&_loading_lock);
		if (_loaded && _resample_filter && len().samples > 0)
			_resample_filter->seek_time(f * len().time);
		render();
		pthread_mutex_unlock(&_loading_lock);
	}

	void nudge_time(double dt)
	{
		pthread_mutex_lock(&_loading_lock);
		if (_loaded)
			_resample_filter->seek_time(_resample_filter->get_time()+dt);
		pthread_mutex_unlock(&_loading_lock);
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
		set_output_sampling_frequency(_pitchpoint);
		// do something with the ui
	}

	void set_clip(int id)
	{
		deferred_call(new deferred1<track_t, int>(this, &track_t::set_clip_impl, id));
	}

	void set_clip_impl(int id)
	{
		asio->_ui->set_clip(id);
	}

	void call_deferreds_loop()
	{
		pthread_mutex_lock(&_deferreds_lock);
		while (_running)
		{
			while (!_defcalls.empty())
			{
				deferred *d = _defcalls.front();
				_defcalls.pop();
				pthread_mutex_unlock(&_deferreds_lock);
				d->operator()();
				delete d;
				pthread_mutex_lock(&_deferreds_lock);
			}
			pthread_cond_wait(&_have_deferred, &_deferreds_lock);
		}
//		pthread_mutex_unlock(&_deferreds_lock);
//		pthread_mutex_destroy(&_deferreds_lock);
//		pthread_cond_destroy(&_have_deferred);
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
	pthread_mutex_t _loading_lock;
	pthread_cond_t _track_loaded;

	std::queue<deferred*> _defcalls;
	pthread_mutex_t _deferreds_lock;
	pthread_cond_t _have_deferred;
	bool _running;
};

#endif // !defined(TRACK_H)
