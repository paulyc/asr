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

class ASIOProcessor;
class GenericUI;

extern IASIO * asiodrv;

#define ASIO_ASSERT_NO_ERROR(call) { \
	e = call ; \
	if (e != ASE_OK) { \
		printf("Yo error %s\n", asio_error_str(e)); \
		assert(false); \
	} \
}

void bufferSwitch(long doubleBufferIndex, ASIOBool directProcess);
ASIOTime* bufferSwitchTimeInfo(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess);
void sampleRateDidChange(ASIOSampleRate sRate);
long asioMessage(long selector, long value, void *message, double *opt);

template <typename Chunk_T, typename Source_T, typename Output_Sample_T>
class asio_sink : public T_sink_sourceable<Chunk_T>
{
public:
	asio_sink(Source_T *src, Output_Sample_T **bufs1, Output_Sample_T **bufs2, long bufSz) :
	  T_sink_sourceable<Chunk_T>(src),
	  _buf_size(bufSz),
	  _src_t(src)
	{
	//	_chk = _src->next();
	//	_read = _chk->_data;
		_chk = 0;
		_read = 0;
		_buffers[0] = bufs1;
		_buffers[1] = bufs2;
	}
	virtual ~asio_sink()
	{
		T_allocator<Chunk_T>::free(_chk);
	}
	void process(int dbIndex);
    void process(Output_Sample_T *bufL, Output_Sample_T *bufR);
protected:
	Chunk_T *_chk;
	typename Chunk_T::sample_t *_read;
	Output_Sample_T **_buffers[2];
	long _buf_size;
	Source_T *_src_t;
};

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
class asio_source : public T_source<Chunk_T>
{
public:
	asio_source();
	~asio_source();
	
	void have_data();
	bool chunk_ready() { return _chks_next.size() > 0; }
	Chunk_T* next();
protected:
	Chunk_T *_chk_working;
	std::queue<Chunk_T *> _chks_next;
	Output_Sample_T *_chk_ptr;
	sem_t _next_sem;
};

template <typename Chunk_T>
class file_raw_output : public T_sink_sourceable<Chunk_T>
{
protected:
	FILE *_f;
public:
	file_raw_output(T_source<Chunk_T> *src, const wchar_t *filename=L"output.raw") :
	  T_sink_sourceable(src),
	  _f(_wfopen(filename, L"wb"))
	{
	}
	  ~file_raw_output()
	  {
		  fclose(_f);
	  }
	  bool eof()
	  {
		  return feof(_f);
	  }
	virtual void process()
	{
		Chunk_T *t = _src->next();
		fwrite(t->_data, 8, Chunk_T::chunk_size, _f);
		T_allocator<Chunk_T>::free(t);
	}
};

/*
template <typename T>
class asio_96_24_sink : public asio_sink<T>
{
public:
	void next(T* t)
	{
	}
};
*/

#define CREATE_BUFFERS 1
#define CHOOSE_CHANNELS 0
#define RUN 0
#define INPUT_PERIOD (1.0/44100.0)

class ASIOProcessor
{
public:
	struct sound_stream
	{
		sound_stream(int chan){}
		virtual int num_channels() = 0;
	//	int nChannels;
		int buffer_sz;
	};
	struct input : public sound_stream
	{
	};
	struct output : public sound_stream
	{
	};
	struct chk_mgr : public T_source<chunk_t>
	{
		chk_mgr():_c(0){}
		chunk_t* _c;
		chunk_t* next()
		{
			assert(_c);
			chunk_t *r = _c;
			_c = 0;
			return r;
		}
	} _main_mgr, _file_mgr;
	
	ASIOProcessor();
	virtual ~ASIOProcessor();

	ASIOError Start();
	ASIOError Stop();

	void BufferSwitch(long doubleBufferIndex, ASIOBool directProcess);
	void SetSrc(int ch, const wchar_t *fqpath);
	void AsyncGenerate();
	void GenerateLoop(pthread_t th);
	void GenerateOutput();

	/*void SetResamplerate(double rate)
	{
		_resamplerate = rate;
		_my_controller->set_output_sampling_frequency(_resample_filter, _resamplerate);
	}

	void SetPos(double tm)
	{
		_my_controller->set_output_time(_resample_filter, tm);
	}*/

	typedef SeekablePitchableFileSource<chunk_t> track_t;
	track_t* GetTrack(int t_id)
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

	GenericUI *get_ui()
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

	long _doubleBufferIndex;
	bool _running;
	double _speed;
	const wchar_t *_default_src;
	bool _resample;
	float _resamplerate;

public: // was protected
	void Init();
	void LoadDriver();
	void ProcessInput();
	void Destroy();
	void Finish();

	const static int inputBuffersize = 4*BUFFERSIZE;
	
	ASIODriverInfo _drv_info;
	size_t _buffer_infos_len;
	ASIOBufferInfo *_buffer_infos;
	ASIOCallbacks _cb;
	long _numInputChannels;
	long _numOutputChannels;
	ASIOChannelInfo *_input_channel_infos;
	ASIOChannelInfo *_output_channel_infos;
	long _bufSize;
	double _outputTime;
	bool _src_active;

	SpeedParser2<BUFFERSIZE> _sp;
	std::ofstream _log;

	asio_source<short, SamplePairf, chunk_t> *_my_source;
	peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size> *_my_pk_det;
	typedef controller<lowpass_filter_td<chunk_t, double>, 
		peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size> 
		> controller_t;
	controller_t *_my_controller;
	file_raw_output<chunk_t> *_file_out;
	asio_sink<chunk_t, chk_mgr, short> *_main_out;

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
    
#if BUFFER_BEFORE_COPY
    short *_bufL;
    short *_bufR;
    bool _need_buffers;
#endif
};

#endif // !defined(_IO_H)
