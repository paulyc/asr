#ifndef _TRACK_H
#define _TRACK_H

#include <pthread.h>

#include "wavfile.h"
#include "buffer.h"
#include "type.h"
#include "ui.h"
#include "future.h"

template <typename Chunk_T>
class PitchableMixin
{
public:
	PitchableMixin() : _resample_filter(0), _pitchpoint(48000.0), _decoder(0)
	{
	}

	virtual void lock() = 0;
	virtual void unlock() = 0;
	virtual bool loaded() = 0;
	virtual void deferred_call(deferred *d) = 0;

	void create(BufferedStream<Chunk_T> *src, double sample_rate)
	{
		if (_resample_filter != 0)
		{
			throw new std::exception("PitchableSource has already been create()d");
		}
		_resample_filter = new lowpass_filter_td<Chunk_T, double>(src, 22050.0, sample_rate, 48000.0);
		_resample_filter->fill_coeff_tbl(); // wtf cause cant call virtual function _h from c'tor
	}

	void destroy()
	{
		delete _resample_filter;
		_resample_filter = 0;
	}

	void set_output_sampling_frequency(double f)
	{
		deferred_call(new deferred1<PitchableMixin<Chunk_T>, double>(this, &PitchableMixin<Chunk_T>::set_output_sampling_frequency_impl, f));
	}

	void set_output_sampling_frequency_impl(double f)
	{
		lock();
		if (loaded() && _resample_filter)
		{
			printf("output mod %f\n", 48000.0/f);
			_resample_filter->set_output_sampling_frequency(f);
		}
		unlock();
	}

	double get_output_sampling_frequency()
	{
		return _resample_filter->get_output_sampling_frequency();
	}

	void set_pitch(double mod)
	{
		set_output_sampling_frequency(48000.0/mod);
	}

	double get_pitch()
	{
		return 48000.0/_resample_filter->get_output_sampling_frequency();
	}

	void nudge_pitch(double dp)
	{
		set_pitch(get_pitch()+dp);
	}

	void set_pitchpoint()
	{
		_pitchpoint = _resample_filter->get_output_sampling_frequency();
	}

	void goto_pitchpoint()
	{
		set_output_sampling_frequency(_pitchpoint);
		// do something with the ui
	}

	double get_pitchpoint()
	{
		return _pitchpoint;
	}

	lowpass_filter_td<Chunk_T, double> *get_source()
	{
		return _resample_filter;
	}

	void use_decoder(peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size> *decoder)
	{
		_resample_filter->pos_stream(this);
	}

	/*pos_info next_pos()
	{
		if (!_decoder->_pos_stream.empty())
		{
			pos_info p = _decoder->_pos_stream.front();
			_decoder->_pos_stream.pop();
			return p;
		}
		else
		{
			pos_info p;
			p.valid = false;
			return p;
		}
	}

	typedef typename peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size>::pos_info pos_info;
	const pos_info& next_pos_info()
	{
		if (_decoder->_pos_stream.empty())
		{
			return 
		}
		else
		{
			const pos_info &pos = _decoder->_pos_stream.front();
			_decoder->_pos_stream.pop();
			return pos
		}
		
	}

	double last_time;
	smp_ofs_t last_smp;*/

protected:
	lowpass_filter_td<Chunk_T, double> *_resample_filter;
	double _pitchpoint;
	peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size> *_decoder;
};

template <typename Chunk_T>
class SeekableMixin
{
public:
	SeekableMixin() : _cuepoint(0.0) {}

	virtual void lock() = 0;
	virtual void unlock() = 0;
	virtual bool loaded() = 0;
	virtual lowpass_filter_td<Chunk_T, double> *get_root_source() = 0;
	virtual void render() = 0;
	virtual double get_display_pos(double) = 0;
	virtual typename const T_source<Chunk_T>::pos_info& len() = 0;
	virtual void deferred_call(deferred *d) = 0;

	void set_cuepoint(double pos)
	{
		_cuepoint = pos;
	}

	double get_cuepoint()
	{
		return _cuepoint;
	}

	void seek_time(double t)
	{
		deferred_call(new deferred1<SeekableMixin<Chunk_T>, double>(this, &SeekableMixin<Chunk_T>::seek_time_impl, t));
	}

	void seek_time_impl(double t)
	{
		lock();
		if (loaded() && get_root_source())
			get_root_source()->seek_time(t);
		render();
		unlock();
	}

	void seek_f(double f)
	{
		deferred_call(new deferred1<SeekableMixin<Chunk_T>, double>(this, &SeekableMixin<Chunk_T>::seek_f_impl, f));
	}

	void seek_f_impl(double f)
	{
		lock();
		if (loaded() && get_root_source() && len().samples > 0)
			get_root_source()->seek_time(f * len().time);
		render();
		unlock();
	}

	void nudge_time(double dt)
	{
		lock();
		if (loaded())
			get_root_source()->seek_time(get_root_source()->get_time()+dt);
		unlock();
	}

	double get_cuepoint_pos()
	{
		return get_display_pos(_cuepoint);
	}

	void goto_cuepoint()
	{
		double t = get_root_source()->get_time();
		if (t <= _cuepoint || t - 0.3 > _cuepoint) //debounce (?)
			seek_time(_cuepoint);
	}

protected:
	double _cuepoint;
};


template <typename Chunk_T>
class ViewableMixin
{
public:
	typedef WavFormDisplay<
		StreamMetadata<Chunk_T>, 
		ViewableMixin<Chunk_T> 
	> display_t;

	ViewableMixin() : _meta(0), _display(0), _display_width(750) {}

	virtual void lock() = 0;
	virtual void unlock() = 0;

	void create(BufferedStream<Chunk_T> *src)
	{
		_meta = new StreamMetadata<Chunk_T>(src);
		_display = new WavFormDisplay<
			StreamMetadata<Chunk_T>, 
			ViewableMixin<Chunk_T> >(_meta, this, _display_width);
	}

	void destroy()
	{
		delete _display;
		_display = 0;
		delete _meta;
		_meta = 0;
	}

	double get_display_time(double f)
	{
		return _display->get_display_time(f);
	}

	double get_display_pos(double f)
	{
		return _display->get_display_pos(f);
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

	void set_wav_heights(bool unlock=true, bool lock=false)
	{
		if (unlock)
			this->unlock();
		if (lock)
			this->lock();
		//_display->set_wav_heights(&asio->_io_lock);
		_display->set_wav_heights();
		if (lock)
			this->unlock();
		if (unlock)
			this->lock();
	}

	typename const display_t::wav_height& get_wav_height(int pixel)
	{
		return _display->get_wav_height(pixel);
	}

protected:
	StreamMetadata<Chunk_T> *_meta;
	display_t *_display;
	int _display_width;
};

template <typename Chunk_T>
class BufferedSource : public T_source<Chunk_T>
{
public:
	BufferedSource() : _src(0), _src_buf(0), _filename(0)
	{
	}

	void create(const wchar_t *filename)
	{
		_filename = filename;

		if (wcsstr(_filename, L".mp3") == _filename + wcslen(_filename) - 4)
		{
			_src = new mp3file_chunker<Chunk_T>(_filename);
		}
		else if (wcsstr(_filename, L".wav") == _filename + wcslen(_filename) - 4)
		{
			_src = new wavfile_chunker<Chunk_T>(_filename);
		}
		else
		{
			_src = new flacfile_chunker<Chunk_T>(_filename);
		}
		_src_buf = new BufferedStream<Chunk_T>(_src);
	}

	void destroy()
	{
		delete _src_buf;
		_src_buf = 0;
		delete _src;
		_src = 0;
		_filename = 0;
	}

	virtual typename const T_source<Chunk_T>::pos_info& len()
	{
		return _src_buf->len();
	}

protected:
	T_source<Chunk_T> *_src;
	BufferedStream<Chunk_T> *_src_buf;
	const wchar_t *_filename;
};

template <typename Chunk_T>
class SeekablePitchableFileSource : public BufferedSource<Chunk_T>, public PitchableMixin<Chunk_T>, public SeekableMixin<Chunk_T>, public ViewableMixin<Chunk_T>
{
public:
	typedef type chunk_t;
	typedef SeekablePitchableFileSource<Chunk_T> track_t;

	SeekablePitchableFileSource(ASIOProcessor *io, int track_id, const wchar_t *filename) :
		_io(io),
		_in_config(false),
		_paused(true),
		_loaded(true),
		_track_id(track_id),
		_last_time(0.0)
	{
		pthread_mutex_init(&_loading_lock, 0);
		pthread_cond_init(&_track_loaded, 0);

		if (filename)
			set_source_file(filename, _io->get_lock());

		_future = new FutureExecutor;
	}

	virtual ~SeekablePitchableFileSource()
	{
		pthread_mutex_lock(&_loading_lock);
		while (!_loaded) 
			pthread_cond_wait(&_track_loaded, &_loading_lock);

		delete _future;
		_future = 0;

		pthread_mutex_destroy(&_loading_lock);
		pthread_cond_destroy(&_track_loaded);

		ViewableMixin<Chunk_T>::destroy();
		PitchableMixin<Chunk_T>::destroy();
		
		BufferedSource<Chunk_T>::destroy();
	}

	void set_source_file(const wchar_t *filename, pthread_mutex_t *lock)
	{
		pthread_mutex_lock(&_loading_lock);
		while (_in_config || !_loaded)
		{
			pthread_cond_wait(&_track_loaded, &_loading_lock);
		}
		
		_in_config = true;
		_loaded = false;
		_paused = true;
		pthread_mutex_unlock(&_loading_lock);

		LOCK_IF_SMP(lock);
		ViewableMixin<Chunk_T>::destroy();
		UNLOCK_IF_SMP(lock);

		LOCK_IF_SMP(lock);
		PitchableMixin<Chunk_T>::destroy();
		UNLOCK_IF_SMP(lock);
		
		LOCK_IF_SMP(lock);
		BufferedSource<Chunk_T>::destroy();
		UNLOCK_IF_SMP(lock);

		LOCK_IF_SMP(lock);
		BufferedSource<Chunk_T>::create(filename);
		UNLOCK_IF_SMP(lock);

		LOCK_IF_SMP(lock);
		PitchableMixin<Chunk_T>::create(_src_buf, _src->sample_rate());
		ViewableMixin<Chunk_T>::create(_src_buf);
		UNLOCK_IF_SMP(lock);

		pthread_mutex_lock(&_loading_lock);
		_in_config = false;
		_last_time = 0.0;
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

	bool play_pause()
	{
		//lock();dont know if lock
		_paused = !_paused;
	//	unlock();
		return _paused;
	}

	void play()
	{
		//lock();dont know if lock
		if (_paused)
			_paused = false;
		//unlock();
	}

	void load_step_if()
	{
		if (!_loaded && !_in_config)
            load_step();
	}

	bool load_step(pthread_mutex_t *lock=0)
	{
		while (len().samples < 0)
		{
			if (lock) pthread_mutex_lock(lock);
			_src_buf->load_next();
			if (lock) pthread_mutex_unlock(lock);
			//return true;
		}
        
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

			if (_io && _io->get_ui())
            {
				_io->get_ui()->render(_track_id);
            }

            pthread_cond_signal(&_track_loaded);
			pthread_mutex_unlock(&_loading_lock);
			
			return false;
	//	}
		//pthread_mutex_unlock(lock);
	}

	void lockedpaint()
	{
	//	printf("track::lockedpaint\n");
		pthread_mutex_lock(&_loading_lock);
		_io->get_ui()->render(_track_id);
		pthread_mutex_unlock(&_loading_lock);
	}

	void draw_if_loaded()
	{
		if (loaded())
		{
			set_wav_heights(false, false);
			lockedpaint();
		}
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
		update_position();
		if (_io->get_ui()->want_render())
			Worker::do_job(new Worker::draw_waveform_job<SeekablePitchableFileSource<Chunk_T> >(this, 0));
	}
    
    void update_position()
    {
		const double tm = _resample_filter->get_time();
		if (tm > _cuepoint && _last_time < _cuepoint)
		{
			_io->trigger_sync_cue(_track_id);
		}
		_last_time = tm;
        _io->get_ui()->set_position(this, _display->get_display_pos(tm), true);
    }

	void deferred_call(deferred *d)
	{
		_future->submit(d);
	}

	double get_display_pos(double f)
	{
		return ViewableMixin<Chunk_T>::get_display_pos(f);
	}

	typename const T_source<Chunk_T>::pos_info& len()
	{
		return BufferedSource<Chunk_T>::len();
	}

	bool loaded()
	{
		return _loaded;
	}

	void lock()
	{
		pthread_mutex_lock(&_loading_lock);
	}

	void unlock()
	{
		pthread_mutex_unlock(&_loading_lock);
	}

	void set_clip(int id)
	{
		deferred_call(new deferred1<track_t, int>(this, &track_t::set_clip_impl, id));
	}

	void set_clip_impl(int id)
	{
		_io->_ui->set_clip(id);
	}

	lowpass_filter_td<Chunk_T, double> *get_root_source()
	{
		return PitchableMixin<Chunk_T>::get_source();
	}

	void have_position(int chk_ofs, smp_ofs_t smp, double tm, double freq)
	{
		if (!_loaded || _paused)
		{
			return;
		}
		lowpass_filter_td<Chunk_T, double> *src = get_root_source();
		if (src)
			src->have_position(chk_ofs, smp, tm, freq);
	}

protected:
	ASIOProcessor *_io;
	bool _in_config;
	bool _paused;
	bool _loaded;
	int _track_id;
	
	pthread_mutex_t _loading_lock;
	pthread_cond_t _track_loaded;
	
	FutureExecutor *_future;
	double _last_time;
};

#endif // !defined(TRACK_H)
