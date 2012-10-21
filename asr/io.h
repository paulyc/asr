#ifndef _IO_H
#define _IO_H

#include <asio.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <objbase.h>
#include <fstream>
#include <queue>
#include <exception>
#include <semaphore.h>

#include "asr.h"
#include "type.h"
#include "filter.h"
#include "buffer.h"
#include "decoder.h"
#include "asiodrv.h"
#include "controller.h"
#include "speedparser.h"
#include "wavformdisplay.h"
#include "track.h"

#include "iomgr.h"

class ASIOProcessor;
class GenericUI;

class ASIOProcessor
{
public:
	chunk_buffer _main_mgr, _file_mgr, _2_mgr;
	
	ASIOProcessor();
	virtual ~ASIOProcessor();

public:
	ASIOError Start();
	ASIOError Stop();

	void BufferSwitch(long doubleBufferIndex, ASIOBool directProcess);
	void GenerateLoop(pthread_t th);
	void GenerateOutput();

	typedef SeekablePitchableFileSource<chunk_t> track_t;
	track_t* GetTrack(int t_id) const
	{
		return _tracks[t_id-1];
	}

	void SetMix(int m)
	{
		_master_xfader->set_mix(m);
	//	_bus_matrix->xfade_2(m, _tracks[0], _tracks[1], _master_bus);
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
		pthread_mutex_lock(&_io_lock);
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
					_file_src = _master_xfader;
					break;
				case Cue:
					_file_src = _cue;
					break;
				case Aux:
				//	_file_src = _aux;
					break;
			}
		}
		pthread_mutex_unlock(&_io_lock);
	}

	int get_track_id_for_filter(void *filt) const
	{
		return _tracks[0]->get_source() == filt ? 1 : 2;
	}

	void set_file_output(const wchar_t *filename)
	{
		pthread_mutex_lock(&_io_lock);
		if (_file_out)
		{
			delete _file_out;
			_file_out = 0;
		}
		_file_out = new file_raw_output<chunk_t>(&_file_mgr, filename);
		pthread_mutex_unlock(&_io_lock);
	}
    
    void load_step()
    {
        _tracks[0]->load_step_if();
        _tracks[1]->load_step_if();
    }

	pthread_mutex_t *get_lock()
	{
		return &_io_lock;
	}

	void set_sync_cue(bool sync)
	{
		_sync_cue = sync;
	}

	bool get_sync_cue() const
	{
		return _sync_cue;
	}

	void trigger_sync_cue(int caller_id)
	{
		if (_sync_cue)
		{
			track_t *other_track = GetTrack(caller_id == 1 ? 2 : 1);
			other_track->goto_cuepoint();
			other_track->play();
		}
	}

	bool is_waiting() const { return true; }

	long _doubleBufferIndex;
	bool _running;
	double _speed;
	const wchar_t *_default_src;
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
	//typedef controller<track_t, 
	//	peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size> 
	//	> controller_t;
	gain<asio_source<short, SamplePairf, chunk_t> > *_my_gain;
	//controller_t *_my_controller;
	file_raw_output<chunk_t> *_file_out;
	typedef peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size>::pos_info pos_info;

	const std::deque<pos_info>& get_pos_stream() const { return _my_pk_det->_pos_stream; }

	std::vector<track_t*> _tracks;
	xfader<track_t> *_master_xfader;
	xfader<track_t> *_cue;
	xfader<track_t> *_aux;

	xfader<track_t> *_main_src;
	xfader<track_t> *_file_src;

	pthread_mutex_t _io_lock;
	GenericUI *_ui;

	pthread_cond_t _do_gen;
	pthread_cond_t _gen_done;
	bool _finishing;
	pthread_t _gen_th;
    bool _need_buffers;

	ASIOManager<chunk_t>* _iomgr;
	bool _sync_cue;

	bool _waiting;

	T_sink<chunk_t> *_dummy_sink;
	T_sink<chunk_t> *_dummy_sink2;
	
};

class fAStIOProcessor : public ASIOProcessor
{
public:
	void MainLoop();
	void GenerateOutput();
	void BufferSwitch(long doubleBufferIndex, ASIOBool directProcess);
protected:
	ts_queue<char*> _buf_q;
	chunk_t *_main_out_chunk;
	sem_t _sem;
};

#endif // !defined(_IO_H)
