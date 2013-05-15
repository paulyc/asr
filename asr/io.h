#ifndef _IO_H
#define _IO_H

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <fstream>
#include <queue>
#include <exception>
#include <unordered_map>
#include <semaphore.h>

#ifdef WIN32
#include <objbase.h>
#include <asio.h>
#endif

#include "iomgr.h"

#include "asr.h"
#include "type.h"
#include "dsp/filter.h"
#include "dsp/gain.h"
#include "buffer.h"
#include "decoder.h"
#include "asiodrv.h"
#include "controller.h"
#include "speedparser.h"
#include "wavformdisplay.h"
#include "worker.h"

class ASIOProcessor;
class GenericUI;
class IMIDIDevice;
template <typename T>
class AudioTrack;

class ASIOProcessor
{
public:
	chunk_buffer _main_mgr, /*_file_mgr,*/ _2_mgr;
	chunk_buffer_t<chunk_time_domain_1d<SamplePairInt16, chunk_t::chunk_size> > _file_mgr;
	
	ASIOProcessor();
	virtual ~ASIOProcessor();

public:
	void Start();
	void Stop();

	void BufferSwitch(long doubleBufferIndex);
	void GenerateLoop(pthread_t th);
	void GenerateOutput();

	typedef AudioTrack<chunk_t> track_t;
	track_t* GetTrack(int t_id) const
	{
		return _tracks[t_id-1];
	}

	void SetMix(int m)
	{
		_master_xfader->set_mix(m);
	}

	void set_ui(GenericUI *ui)
	{
		_ui = ui;
		CreateTracks();
	}

	void CreateTracks();

	GenericUI *get_ui() const
	{
		return _ui;
	}

	enum Source { Off, Master, Cue, Aux };
	enum Output { Main, File };

	void set_gain(int t_id, double g)
	{
		_master_xfader->set_gain(t_id, g);
		_cue->set_gain(t_id, g);
	}

	void set_source(Output o, Source s)
	{
		_io_lock.acquire();
		if (o==Main)
		{
			switch (s)
			{
				case Master:
					_main_src = _master_xfader;
					break;
				case Cue:
					_main_src = _cue;
					break;
				case Aux:
				//	_main_src = _aux;
					break;
			}
		}
		else
		{
			if (!_file_out)
				return;
			switch (s)
			{
				case Off:
					_file_src = 0;
					break;
				case Master:
				//	_file_src = _master_xfader;
					break;
				case Cue:
				//	_file_src = _cue;
					break;
				case Aux:
					_file_src = _aux;
					break;
			}
		}
		_io_lock.release();
	}

	int get_track_id_for_filter(void *filt) const;

	void set_file_output(const char *filename)
	{
	//	pthread_mutex_lock(&_io_lock);
		if (_file_out)
		{
			delete _file_out;
			_file_out = 0;
		}
		//_file_out = new file_raw_output<chunk_t>(&_file_mgr, filename);
		//_file_out = new file_raw_output<chunk_time_domain_1d<SamplePairInt16, 4096> >(&_file_mgr, filename);
		_file_out = new file_raw_output<chunk_time_domain_1d<SamplePairInt16, chunk_t::chunk_size> >(_aux, filename);
	//	pthread_mutex_unlock(&_io_lock);
	}

	Lock_T& get_lock()
	{
		return _io_lock;
	}

	void set_sync_cue(bool sync)
	{
		_sync_cue = sync;
	}

	bool get_sync_cue() const
	{
		return _sync_cue;
	}

	void trigger_sync_cue(int caller_id);

	bool is_waiting() const { return true; }

	long _doubleBufferIndex;
	bool _running;
	double _speed;
	const char *_default_src;
	bool _resample;
	float _resamplerate;

public: // was protected
	void Init();
	void Reconfig();
	void Destroy();
	void Finish();

	const static int inputBuffersize = 4*BUFFERSIZE;
	
	double _outputTime;
	bool _src_active;

	SpeedParser2<BUFFERSIZE> _sp;
	std::ofstream _log;

	peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size> *_my_pk_det;
	gain<T_source<chunk_t> > *_my_gain;
	//file_raw_output<chunk_t> *_file_out;
	file_raw_output<chunk_time_domain_1d<SamplePairInt16, chunk_t::chunk_size> > *_file_out;
	typedef peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size>::pos_info pos_info;

	const std::deque<pos_info>& get_pos_stream() const { return _my_pk_det->_pos_stream; }

	std::vector<track_t*> _tracks;
	xfader<T_source<chunk_t> > *_master_xfader;
	xfader<T_source<chunk_t> > *_cue;
	//xfader<track_t> *_aux;
	T_source<chunk_time_domain_1d<SamplePairInt16, chunk_t::chunk_size> > *_aux;

	xfader<T_source<chunk_t> > *_main_src;
	//xfader<track_t> *_file_src;
	T_source<chunk_time_domain_1d<SamplePairInt16, chunk_t::chunk_size> > *_file_src;

	GenericUI *_ui;

	Lock_T _io_lock;
	Condition_T _do_gen;
	Condition_T _gen_done;
	bool _finishing;
	pthread_t _gen_th;
    bool _need_buffers;

    IAudioDevice *_device;
	bool _sync_cue;

	bool _waiting;
	
	IMIDIController *_midi_controller;
public:
	FilterController<resampling_filter_td<chunk_t, double> > *_filter_controller;
    
    IAudioStream *_inputStream;
    IAudioStream *_outputStream;
    
#if MAC
    CoreAudioInputProcessor *_inputStreamProcessor;
    CoreAudioOutputProcessor *_outputStreamProcessor;
#elif WINDOWS
    
#endif // WINDOWS
    ChunkGenerator *_gen;
    gain<T_source<chunk_t> > *_gain1, *_gain2;
};

#endif // !defined(_IO_H)
