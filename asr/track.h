// ASR - Digital Signal Processor
// Copyright (C) 2002-2013	Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.	 If not, see <http://www.gnu.org/licenses/>.

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
	PitchableMixin(double sampleRateOut=48000.0) : _resample_filter(0), _pitchpoint(sampleRateOut), _sampleRateOut(sampleRateOut)
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
		_resample_filter = new filter_t(src, sample_rate, _sampleRateOut);
	//	_resample_filter->fill_coeff_tbl();
		_pitch = 1.0;
	}

	void destroy()
	{
		delete _resample_filter;
		_resample_filter = 0;
	}
	
	void set_sample_rate_out(double f)
	{
		_sampleRateOut = f;
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
			printf("output mod %f\n", _sampleRateOut/f);
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
		set_output_sampling_frequency(_sampleRateOut/mod);
		unlock();
	}

	void bend_pitch(double dp)
	{
		lock();
		set_output_sampling_frequency(_sampleRateOut/(_pitch+dp));
		unlock();
	}

	double get_pitch()
	{
		return _sampleRateOut/_resample_filter->get_output_sampling_frequency();
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

protected:
	filter_t *_resample_filter;
	controller_t *_filter_controller;
	double _pitchpoint;

	double _pitch;
	double _sampleRateOut;
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
	//virtual IOProcessor *get_io() const = 0;

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
		_display_width = width;
		_display->set_width(width);
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
		_display->set_wav_heights();
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

	void create(const char *filename, CriticalSectionGuard *lock);

//	void createZero(IOProcessor *io)
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
	
	void analyze()
	{
		_detector.analyze();
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
	const char *_filename;

	BeatDetector<Chunk_T> _detector;
};

template <typename Chunk_T>
class AudioTrack : 
	public BufferedSource<Chunk_T>, 
	public PitchableMixin<Chunk_T>, 
	public SeekableMixin<Chunk_T>, 
	public ViewableMixin<Chunk_T>
{
public:
	typedef AudioTrack<Chunk_T> track_t;

	using SeekableMixin<Chunk_T>::goto_cuepoint;
	using SeekableMixin<Chunk_T>::seek_time;

	AudioTrack(IOProcessor *io, int track_id, const char *filename);
	virtual ~AudioTrack();

	void set_source_file(std::string filename, CriticalSectionGuard &lock);
	Chunk_T* next();
	void load(CriticalSectionGuard *lock=0);

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

	void draw_if_loaded()
	{
		if (loaded())
		{
			ViewableMixin<Chunk_T>::set_wav_heights(false, true);
			_loading_lock.acquire();
			_io->get_ui()->render(_track_id);
			_loading_lock.release();
		}
	}

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
		Worker::do_job(new Worker::draw_waveform_job<AudioTrack<Chunk_T> >(this, 0));
	//	pthread_mutex_unlock(&_config_lock);
	}

	void move_px(int d)
	{
	//	printf("track::move_px\n");
	//	pthread_mutex_lock(&_config_lock);
		if (!_loaded) return;
		ViewableMixin<Chunk_T>::move_px(d);
		Worker::do_job(new Worker::draw_waveform_job<AudioTrack<Chunk_T> >(this, 0));
	//	pthread_mutex_unlock(&_config_lock);
	}

	void render()
	{
	//	printf("track::render\n");
		update_position();
		if (_io->get_ui()->want_render())
			Worker::do_job(new Worker::draw_waveform_job<AudioTrack<Chunk_T> >(this, 0));
	}
	
	void update_position()
	{
		const double tm = this->_resample_filter->get_time();
		if (tm > this->_cuepoint && _last_time < this->_cuepoint)
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

	virtual bool is_paused() const { return _paused; }

	void goto_cuepoint(bool x)
	{
		this->SeekableMixin<Chunk_T>::goto_cuepoint(x);
	}

	void seek_time(double d)
	{
		SeekableMixin<Chunk_T>::seek_time(d);
	}
	
	void quitting()
	{
		delete _future;
		_future = 0;
	}

protected:
	IOProcessor *_io;
	bool _in_config;
	bool _paused;
	bool _loaded;
	int _track_id;
	
	Lock_T _loading_lock;
	Condition_T _track_loaded;
	
	FutureExecutor *_future;
	double _last_time;

	std::string _filename;
	bool _pause_monitor;
	
private:
	void set_source_file_impl(std::string filename, CriticalSectionGuard *lock);
};

typedef AudioTrack<chunk_t> Track_T;

#include "track.cpp"

#endif // !defined(TRACK_H)
