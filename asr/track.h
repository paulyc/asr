#ifndef _TRACK_H
#define _TRACK_H

#include <pthread.h>

#include "wavfile.h"
#include "buffer.h"
#include "type.h"
#include "ui.h"
#include "future.h"
#include "dsp/beats.h"
#include "dsp/fft.h"

typedef resampling_filter_td<chunk_t, double> filter_t;

template <typename Chunk_T>
class PitchableMixin : public ITrackController
{
public:
	PitchableMixin() : _resample_filter(0), _pitchpoint(48000.0)
	{
	}

	virtual void lock() = 0;
	virtual void unlock() = 0;
	virtual bool loaded() const = 0;
	virtual void deferred_call(deferred *d) = 0;
	virtual bool play_pause(bool) = 0;
	
	typedef FilterController<filter_t> controller_t;

	void create(BufferedStream<Chunk_T> *src, double sample_rate)
	{
		destroy();
		_resample_filter = new filter_t(src, sample_rate, 48000.0);
	//	_resample_filter->fill_coeff_tbl();
		_pitch = 1.0;
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
		lock();
		_pitch = mod;
		set_output_sampling_frequency(48000.0/mod);
		unlock();
	}

	void bend_pitch(double dp)
	{
		lock();
		set_output_sampling_frequency(48000.0/(_pitch+dp));
		unlock();
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

	double get_pitchpoint() const
	{
		return _pitchpoint;
	}

	filter_t *get_source()
	{
		return _resample_filter;
	}

	/*void use_decoder(peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size> *decoder)
	{
		_resample_filter->pos_stream(this);
	}*/

protected:
	filter_t *_resample_filter;
	controller_t *_filter_controller;
	double _pitchpoint;

	double _pitch;
	//double _dPitch;
};

template <typename Chunk_T>
class SeekableMixin
{
public:
	SeekableMixin() : _cuepoint(0.0) {}

	virtual void lock() = 0;
	virtual void unlock() = 0;
	virtual bool loaded() const = 0;
	virtual filter_t *get_root_source() = 0;
	virtual void render() = 0;
	virtual double get_display_pos(double) = 0;
	virtual const typename T_source<Chunk_T>::pos_info& len() = 0;
	virtual void deferred_call(deferred *d) = 0;
	virtual bool is_paused() const = 0;
	virtual void pause(bool) = 0;

	void set_cuepoint(double pos)
	{
		_cuepoint = pos;
	}

	double get_cuepoint()
	{
		return _cuepoint;
	}

	virtual void seek_time(double t)
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
		//deadlock on  ui!!
	//	lock();
		if (loaded())
			get_root_source()->seek_time(get_root_source()->get_time()+dt);
	//	unlock();
	}

	double get_cuepoint_pos()
	{
		return get_display_pos(_cuepoint);
	}

	virtual void goto_cuepoint(bool set_if_paused)
	{
		lock();
		if (set_if_paused && is_paused())
		{
			set_cuepoint(get_root_source()->get_time());
			pause(false);
			unlock();
			return;
		}
		double t = get_root_source()->get_time();
		if (t <= _cuepoint || t - 0.3 > _cuepoint) //debounce (?)
		{
			seek_time(_cuepoint);
			pause(false);
		}
		unlock();
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
	virtual ASIOProcessor *get_io() const = 0;

	void create(BufferedStream<Chunk_T> *src)
	{
		destroy();
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
		lock();
		_display->lock_pos(y);
		unlock();
	}

	void unlock_pos()
	{
		lock();
		_display->unlock_pos();
		unlock();
	}

	void zoom_px(int d)
	{
		_display->zoom_px(d);
	}

	void move_px(int d)
	{
		_display->move_px(d);
	}

	void set_wav_heights(bool unlock=true, bool lock=false)
	{
		if (unlock)
			this->unlock();
		if (lock)
			this->lock();
		//_display->set_wav_heights(&asio->_io_lock);
		_display->set_wav_heights(this->get_io());
		if (lock)
			this->unlock();
		if (unlock)
			this->lock();
	}

	const typename display_t::wav_height& get_wav_height(int pixel)
	{
		return _display->get_wav_height(pixel);
	}

protected: //should be private!!
	StreamMetadata<Chunk_T> *_meta;
	display_t *_display;
	int _display_width;
};

template <typename Chunk_T>
class BufferedSource : public T_source<Chunk_T>
{
public:
	BufferedSource() : 
		_src(0), 
		_src_buf(0), 
		_filename(0)
	{
	}

	void create(ASIOProcessor *io, const wchar_t *filename, Lock_T *lock)
	{
		T_source<Chunk_T> *src = 0;

		if (wcsstr(filename, L".mp3") == filename + wcslen(filename) - 4)
		{
			src = new mp3file_chunker<Chunk_T>(filename, lock);
		}
		else if (wcsstr(filename, L".wav") == filename + wcslen(filename) - 4)
		{
			src = new wavfile_chunker<Chunk_T>(filename);
			//src = new ifffile_chunker<Chunk_T>(filename);
		}
		else if (wcsstr(filename, L".flac") == filename + wcslen(filename) - 5)
		{
			src = new flacfile_chunker<Chunk_T>(filename, lock);
		}
		else
		{
			src = new ifffile_chunker<Chunk_T>(filename);
		}

		destroy();

		_src = src;

	//	_detector.reset_source(_src);
		_src_buf = new BufferedStream<Chunk_T>(io, _src);
	//	_src_buf->load_complete();

		_filename = filename;
	}

//	void createZero(ASIOProcessor *io)
//	{
//		_src = zero_source<Chunk_T>::get();
//		_src_buf = new BufferedStream<Chunk_T>(io, _src);
//		_filename = L"(No source)";
//	}

	void destroy()
	{
		delete _src_buf;
		_src_buf = 0;
		delete _src;
		_src = 0;

		_filename = 0;
	}

	virtual typename T_source<Chunk_T>::pos_info& len()
	{
		return _src_buf->len();
	}

	const std::vector<double>& beats()
	{
		return _detector.beats();
	}

protected:
	T_source<Chunk_T> *_src;
	BufferedStream<Chunk_T> *_src_buf;
	const wchar_t *_filename;

	BeatDetector<Chunk_T> _detector;
};

template <typename Chunk_T>
class SeekablePitchableFileSource : 
	public BufferedSource<Chunk_T>, 
	public PitchableMixin<Chunk_T>, 
	public SeekableMixin<Chunk_T>, 
	public ViewableMixin<Chunk_T>
{
public:
	typedef SeekablePitchableFileSource<Chunk_T> track_t;

	using SeekableMixin<Chunk_T>::goto_cuepoint;
	using SeekableMixin<Chunk_T>::seek_time;

	SeekablePitchableFileSource(ASIOProcessor *io, int track_id, const char *filename) :
		_io(io),
		_in_config(false),
		_paused(true),
		_loaded(true),
		_track_id(track_id),
		_last_time(0.0),
		_pause_monitor(false)
	{
		_future = new FutureExecutor;

	//	int sz = (char*)&_display - (char*)this;
	//	printf("sz = 0x%x\n", sz);

		if (filename)
			set_source_file_impl(std::string(filename), &_io->get_lock());
	}

	virtual ~SeekablePitchableFileSource()
	{
		_loading_lock.acquire();
		while (!_loaded) 
			_track_loaded.wait(_loading_lock);

		delete _future;
		_future = 0;

		ViewableMixin<Chunk_T>::destroy();
		PitchableMixin<Chunk_T>::destroy();
		
		BufferedSource<Chunk_T>::destroy();
	}

	void set_source_file(std::wstring filename, Lock_T &lock)
	{
		_future->submit(
			new deferred2<SeekablePitchableFileSource<Chunk_T>, std::wstring, Lock_T*>(
				this, 
				&SeekablePitchableFileSource<Chunk_T>::set_source_file_impl, 
				filename, 
				&lock));
	}

private:
	void set_source_file_impl(std::string filename, Lock_T *lock)
	{
		_loading_lock.acquire();
		while (_in_config || !_loaded)
		{
			_track_loaded.wait(_loading_lock);
		}
		
		_in_config = true;
		_loaded = false;
		_paused = true;

		try {
			BufferedSource<Chunk_T>::create(_io, filename.c_str(), lock);
		} catch (std::exception &e) {
			printf("Could not set_source_file due to %s\n", e.what());
			_in_config = false;
			_loaded = true;
			_paused = true;
		//	BufferedSource<Chunk_T>::createZero(_io);
			_loading_lock.release();
			return;
		}
	//	pthread_mutex_unlock(&_loading_lock);

	//	LOCK_IF_SMP(lock);
		PitchableMixin<Chunk_T>::create(_src_buf, _src->sample_rate());
		ViewableMixin<Chunk_T>::create(_src_buf);
		_cuepoint = 0.0;
	//	UNLOCK_IF_SMP(lock);

	//	pthread_mutex_lock(&_loading_lock);
		_in_config = false;
		_last_time = 0.0;
		_filename = filename;
		_loading_lock.release();

		if (!_loaded)
			Worker::do_job(new Worker::load_track_job<SeekablePitchableFileSource<Chunk_T> >(this, lock));
	}

public:
	Chunk_T* next(void *dummy=0)
	{
		Chunk_T *chk;

		_loading_lock.acquire();
		if (!_loaded || (_paused && !_pause_monitor))
		{
			_loading_lock.release();
			chk = zero_source<Chunk_T>::get()->next();
		}
		else
		{
			if (_paused && _pause_monitor)
			{
				double t = _resample_filter->get_time();
				chk = _resample_filter->next();
				_resample_filter->seek_time(t);
			}
			else
			{
				chk = _resample_filter->next();
				if (true && _resample_filter->get_time() >= len().time)
					_resample_filter->seek_time(0.0);
			}
			_loading_lock.release();
			render();
		}
		
		return chk;
	}

	bool play_pause(bool pause_monitor=false)
	{
		lock();//dont know if lock
		_pause_monitor = pause_monitor;
		_paused = !_paused;
		unlock();
		return _paused;
	}

	void pause(bool pause_monitor)
	{
		_pause_monitor=pause_monitor;
		_paused = true;
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

	bool load_step(Lock_T *lock=0)
	{
		while (_src_buf->len().chunks == -1)
		{
			_src_buf->load_next();
		}

		printf("len %d samples %d chunks\n", _src_buf->len().samples, _src_buf->len().chunks);

		_meta->load_metadata(lock, _io);
        
		_display->set_zoom(100.0);
		_display->set_left(0.0);
		//pthread_mutex_lock(lock);
		_display->set_wav_heights(_io, lock);
	//	if (!_display->set_next_height())
	//	{
		//	GenericUI *ui;
	//		pthread_mutex_unlock(lock);
			
			_resample_filter->set_output_scale(1.0f / _src->maxval());
			//_meta->load_metadata(lock);
			_loading_lock.acquire();
			_loaded = true;

			if (_io && _io->get_ui())
            {
				_io->get_ui()->render(_track_id);
            }

			_track_loaded.signal();
			_loading_lock.release();

			_io->get_ui()->get_track(_track_id).filename.set_text(_filename.c_str(), false);
			
			return false;
	//	}
		//pthread_mutex_unlock(lock);
	}

	void draw_if_loaded()
	{
		if (loaded())
		{
			set_wav_heights(false, true);
			_loading_lock.acquire();
			_io->get_ui()->render(_track_id);
			_loading_lock.release();
		}
	}

	//using ViewableMixin<Chunk_T>::get_display_pos;

	double get_display_pos(double tm)
	{
		return ViewableMixin<Chunk_T>::get_display_pos(tm);
	}

	void zoom_px(int d)
	{
	//	printf("track::zoom_px\n");
		if (!_loaded) return;
	//	pthread_mutex_lock(&_config_lock);
		ViewableMixin<Chunk_T>::zoom_px(d);
		Worker::do_job(new Worker::draw_waveform_job<SeekablePitchableFileSource<Chunk_T> >(this, 0));
    //	pthread_mutex_unlock(&_config_lock);
	}

	void move_px(int d)
	{
	//	printf("track::move_px\n");
	//	pthread_mutex_lock(&_config_lock);
		if (!_loaded) return;
		ViewableMixin<Chunk_T>::move_px(d);
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
		_io->get_ui()->set_position(this, get_display_pos(tm), true);
    }

	void deferred_call(deferred *d)
	{
		_future->submit(d);
	}

	typename T_source<Chunk_T>::pos_info& len()
	{
		return BufferedSource<Chunk_T>::len();
	}

	bool loaded() const
	{
		return _loaded;
	}

	void lock()
	{
		_loading_lock.acquire();
	}

	void unlock()
	{
		_loading_lock.release();
	}

	void set_clip(int id)
	{
		_io->_ui->set_clip(id);
	}

	filter_t *get_root_source()
	{
		return PitchableMixin<Chunk_T>::get_source();
	}

	/*void have_position(const typename ASIOProcessor::pos_info &pos)
	{
		if (!_loaded || _paused)
		{
			return;
		}
		filter_t *src = get_root_source();
		if (src)
			src->have_position(pos);
	}*/

	virtual bool is_paused() const { return _paused; }

	void goto_cuepoint(bool x)
	{
		this->SeekableMixin<Chunk_T>::goto_cuepoint(x);
	}

	void seek_time(double d)
	{
		SeekableMixin<Chunk_T>::seek_time(d);
	}

protected:
	ASIOProcessor *get_io() const
	{
		return _io;
	}
	ASIOProcessor *_io;
	bool _in_config;
	bool _paused;
	bool _loaded;
	int _track_id;
	
	Lock_T _loading_lock;
	Condition_T _track_loaded;
	
	FutureExecutor *_future;
	double _last_time;

	std::wstring _filename;
	bool _pause_monitor;
};

typedef SeekablePitchableFileSource<chunk_t> Track_T;

#endif // !defined(TRACK_H)
