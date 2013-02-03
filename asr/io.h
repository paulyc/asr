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
#include "dsp/filter.h"
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
class IMIDIDevice;

template <typename Input_Chunk_T, typename Output_Chunk_T>
class ChunkConverter : public T_source<Output_Chunk_T>, public T_sink<Input_Chunk_T>
{
public:
	ChunkConverter(T_source<Input_Chunk_T> *src) : T_sink(src) {}
	Output_Chunk_T *next() { return 0; }
};

template <int chunk_size>
class ChunkConverter<chunk_time_domain_1d<SamplePairf, chunk_size>, chunk_time_domain_1d<SamplePairInt16, chunk_size> >  : 
	public T_source<chunk_time_domain_1d<SamplePairInt16, chunk_size> >, 
	public T_sink<chunk_time_domain_1d<SamplePairf, chunk_size> >
{
	typedef chunk_time_domain_1d<SamplePairf, chunk_size> input_chunk_t;
	typedef chunk_time_domain_1d<SamplePairInt16, chunk_size> output_chunk_t;
public:
	ChunkConverter(T_source<input_chunk_t> *src) : T_sink(src) {}
	output_chunk_t *next() 
	{
		input_chunk_t *in_chk = _src->next();
		SamplePairf *in_data = in_chk->_data;
		float in;
		output_chunk_t *out_chk = T_allocator<output_chunk_t>::alloc();
		SamplePairInt16 *out_data = out_chk->_data;

		for (int i=0; i<chunk_size; ++i)
		{
			in = in_data[i][0];
			if (in < 0.0f)
				out_data[i][0] = short(max(-1.0f, in) * -SHRT_MIN);
			else
				out_data[i][0] = short(min(1.0f, in) * SHRT_MAX);

			in = in_data[i][1];
			if (in < 0.0f)
				out_data[i][1] = short(max(-1.0f, in) * -SHRT_MIN);
			else
				out_data[i][1] = short(min(1.0f, in) * SHRT_MAX);
		}
		T_allocator<input_chunk_t>::free(in_chk);
		return out_chk;
	}
};

class ASIOProcessor
{
public:
	chunk_buffer _main_mgr, /*_file_mgr,*/ _2_mgr;
	chunk_buffer_t<chunk_time_domain_1d<SamplePairInt16, 4096> > _file_mgr;
	
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
		//_file_out = new file_raw_output<chunk_t>(&_file_mgr, filename);
		//_file_out = new file_raw_output<chunk_time_domain_1d<SamplePairInt16, 4096> >(&_file_mgr, filename);
		_file_out = new file_raw_output<chunk_time_domain_1d<SamplePairInt16, 4096> >(_aux, filename);
		pthread_mutex_unlock(&_io_lock);
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
			other_track->goto_cuepoint(false);
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
	static void ControllerCallback(ControlMsg *msg, void *cbParam);

	const static int inputBuffersize = 4*BUFFERSIZE;
	
	double _outputTime;
	bool _src_active;

	SpeedParser2<BUFFERSIZE> _sp;
	std::ofstream _log;

	peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size> *_my_pk_det;
	gain<asio_source<short, SamplePairf, chunk_t> > *_my_gain;
	//file_raw_output<chunk_t> *_file_out;
	file_raw_output<chunk_time_domain_1d<SamplePairInt16, chunk_t::chunk_size> > *_file_out;
	typedef peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size>::pos_info pos_info;

	const std::deque<pos_info>& get_pos_stream() const { return _my_pk_det->_pos_stream; }

	std::vector<track_t*> _tracks;
	xfader<track_t> *_master_xfader;
	xfader<track_t> *_cue;
	//xfader<track_t> *_aux;
	T_source<chunk_time_domain_1d<SamplePairInt16, chunk_t::chunk_size> > *_aux;

	xfader<track_t> *_main_src;
	//xfader<track_t> *_file_src;
	T_source<chunk_time_domain_1d<SamplePairInt16, chunk_t::chunk_size> > *_file_src;

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
	
	IMIDIController *_midi_controller;
public:
	track_t::controller_t *_filter_controller;
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
