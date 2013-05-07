#ifndef _IO_H
#define _IO_H

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <fstream>
#include <queue>
#include <exception>
#include <semaphore.h>

#ifdef WIN32
#include <objbase.h>
#include <asio.h>
#endif

#include "iomgr.h"

#include "asr.h"
#include "type.h"
#include "dsp/filter.h"
#include "buffer.h"
#include "decoder.h"
#include "asiodrv.h"
#include "controller.h"
#include "speedparser.h"
#include "wavformdisplay.h"

class ASIOProcessor;
class GenericUI;
class IMIDIDevice;
template <typename T>
class SeekablePitchableFileSource;

class ChunkGenerator
{
public:
    void AddChunkSource(T_source<chunk_t> *src, int id);
    chunk_t* GetNextChunk(int streamID);
};

class BlockingChunkStream : public T_source<chunk_t>
{
public:
    BlockingChunkStream(ChunkGenerator *gen, int streamID) : _gen(gen), _streamID(streamID) {}
    chunk_t *next()
    {
        return _gen->GetNextChunk(_streamID);
    }
private:
    ChunkGenerator *_gen;
    int _streamID;
};

class AudioInput
{
public:
	virtual ~AudioInput() {}
    
	virtual void process(int doubleBufferIndex) {}
};

class AudioOutput : public T_sink_sourceable<chunk_t>
{
public:
    AudioOutput(T_source<chunk_t> *src, int ch1id, int ch2id) :
        T_sink_sourceable<chunk_t>(src),
        _ch1id(ch1id),
        _ch2id(ch2id)
    {
        _chk = 0;
        _read = 0;
    }
	virtual ~AudioOutput()
    {
        T_allocator<chunk_t>::free(_chk);
    }
    
	virtual void process(MultichannelAudioBuffer *buf);
	virtual void switchBuffers(int dbIndex) {}
protected:
    int _ch1id;
    int _ch2id;
	chunk_t *_chk;
	typename chunk_t::sample_t *_read;
};

#if MAC
class CoreAudioInput : public AudioInput
{
};

class CoreAudioOutput : public AudioOutput
{
public:
    CoreAudioOutput(T_source<chunk_t> *src, int ch1ofs, int ch2ofs) : AudioOutput(src, ch1ofs, ch2ofs) {}
    virtual void process(MultichannelAudioBuffer *buf);
};

class CoreAudioOutputProcessor : public IAudioStreamProcessor
{
public:
    CoreAudioOutputProcessor(const IAudioStreamDescriptor *streamDesc);
    
    void AddOutput(const CoreAudioOutput &out) { _outputs.push_back(out); }
    virtual void Process(IAudioBuffer *buffer);
private:
    int _channels;
    int _frameSize;
    std::vector<CoreAudioOutput> _outputs;
    
    chunk_t *_chk;
	typename chunk_t::sample_t *_read;
};

class CoreAudioInputProcessor : public IAudioStreamProcessor
{
public:
    void AddInput(const CoreAudioInput &in);
    virtual void Process(IAudioBuffer *buffer);
};
#elif WINDOWS

#endif // WINDOWS

template <typename Input_Chunk_T, typename Output_Chunk_T>
class ChunkConverter : public T_source<Output_Chunk_T>, public T_sink<Input_Chunk_T>
{
public:
	ChunkConverter(T_source<Input_Chunk_T> *src) : T_sink<Input_Chunk_T>(src) {}
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
	ChunkConverter(T_source<input_chunk_t> *src) : T_sink<chunk_time_domain_1d<SamplePairf, chunk_size> >(src) {}
	output_chunk_t *next() 
	{
		input_chunk_t *in_chk = this->_src->next();
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
	chunk_buffer_t<chunk_time_domain_1d<SamplePairInt16, chunk_t::chunk_size> > _file_mgr;
	
	ASIOProcessor();
	virtual ~ASIOProcessor();

public:
	void Start();
	void Stop();

	void BufferSwitch(long doubleBufferIndex);
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
};

#endif // !defined(_IO_H)
