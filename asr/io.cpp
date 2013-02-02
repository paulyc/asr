#include "config.h"
#include "io.h"
#include "tracer.h"

#include <queue>

#include <pthread.h>
#include <semaphore.h>

#include "asio.cpp"
#include "iomgr.cpp"

ASIOProcessor::ASIOProcessor() :
	_running(false),
	_speed(1.0),
	_default_src(L"F:\\Beatport Music\\Sander van Doorn - Riff (Original Mix).mp3"),
	//_default_src(L"H:\\Music\\Heatbeat - Hadoken (Original Mix).wav"),
	//_default_src(L"H:\\Music\\Sean Tyas - Melbourne (Original Mix).wav"),
	//_default_src(L"H:\\Music\\Super8 & Tab, Anton Sonin - Black Is The New Yellow (Activa Remix).wav"),
	_resample(false),
	_resamplerate(48000.0),
	_outputTime(0.0),
	_src_active(false),
	_log("mylog.txt"),
	_file_out(0),
	_ui(0),
	_finishing(false),
	_iomgr(0),
	_sync_cue(false),
	_dummy_sink(0),
	_dummy_sink2(0)
{
	Init();
}

ASIOProcessor::~ASIOProcessor()
{
	Destroy();
}

void ASIOProcessor::CreateTracks()
{
	_tracks.push_back(new SeekablePitchableFileSource<chunk_t>(this, 1, _default_src));
	_tracks.push_back(new SeekablePitchableFileSource<chunk_t>(this, 2, _default_src));
	
	_master_xfader = new xfader<track_t>(_tracks[0], _tracks[1]);
	_cue = new xfader<track_t>(_tracks[0], _tracks[1]);
	//_aux = new xfader<track_t>(_tracks[0], _tracks[1]);

	_iomgr->createIOs(&_main_mgr, &_2_mgr);
	_aux = new ChunkConverter<chunk_time_domain_1d<SamplePairf, 4096>, chunk_time_domain_1d<SamplePairInt16, 4096> >(_iomgr->_my_source2);

	try {
	//	_my_gain = new gain<asio_source<short, SamplePairf, chunk_t> >(_my_source);
	//	_my_gain->set_gain_db(36.0);
		_my_pk_det = new peak_detector<SamplePairf, chunk_time_domain_1d<SamplePairf, 4096>, 4096>(_iomgr->_my_source);

	//	_my_raw_output = new file_raw_output<chunk_t>(_my_pk_det);
	} catch (std::exception e) {
		throw e;
	}

	_main_src = _master_xfader;
	_file_src = 0;

	_dummy_sink = new T_sink<chunk_t>(_my_pk_det);
	_dummy_sink2 = new T_sink<chunk_t>(_iomgr->_my_source2);

#if GENERATE_LOOP
	Worker::do_job(new Worker::callback_th_job<ASIOProcessor>(
		functor1<ASIOProcessor, pthread_t>(this, &ASIOProcessor::GenerateLoop)),
		false, true
	);
#endif
}

void ASIOProcessor::Init()
{
	pthread_mutex_init(&_io_lock, 0);
	pthread_cond_init(&_gen_done, 0);
	pthread_cond_init(&_do_gen, 0);

	CoInitialize(0);
	ASIODriver drv(L"CreativeASIO");
	_iomgr = dynamic_cast<ASIOManager<chunk_t>*>(drv.loadDriver());
	
	_iomgr->createBuffers();

#if BUFFER_BEFORE_COPY
    _need_buffers = true;
#endif

	Win32MIDIDeviceFactory fac;
//	IMIDIDevice *dev = fac.Instantiate(0, true);
//	if (dev)
//	{
//		_midi_controller = new CDJ350MIDIController(dev);
//		_midi_controller->RegisterCallback(ControllerCallback, this);
//	}
}

void ASIOProcessor::ControllerCallback(ControlMsg *msg, void *cbParam)
{
	ASIOProcessor *io = static_cast<ASIOProcessor*>(cbParam);
}

void ASIOProcessor::Reconfig()
{
}

// stop using ui before destroying
void ASIOProcessor::Finish()
{
	_finishing = true;
	if (_running)
		Stop();

#if GENERATE_LOOP
	pthread_cond_signal(&_do_gen);
	pthread_join(_gen_th, 0);
#endif

	delete _tracks[0];
	delete _tracks[1];
	_tracks.resize(0);
}

void ASIOProcessor::Destroy()
{
	delete _iomgr;
	CoUninitialize();

	delete _dummy_sink;
	delete _dummy_sink2;
	delete _my_pk_det;
//	delete _my_gain;
	
	delete _file_out;
	delete _cue;
	delete _master_xfader;
	delete _aux;

	pthread_cond_destroy(&_do_gen);
	pthread_cond_destroy(&_gen_done);
	pthread_mutex_destroy(&_io_lock);

	T_allocator<chunk_t>::gc();
	T_allocator<chunk_t>::dump_leaks();

	Tracer::printTrace();
}

ASIOError ASIOProcessor::Start()
{
	_running = true;
	_src_active = true;
	return ASIOStart();
}

ASIOError ASIOProcessor::Stop()
{
	_running = false;
	_src_active = false;
	return ASIOStop();
}

void ASIOProcessor::BufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
{
	_waiting = true;
	pthread_mutex_lock(&_io_lock);
	_waiting = false;
	_doubleBufferIndex = doubleBufferIndex;

#if CARE_ABOUT_INPUT
	_iomgr->processInputs(_doubleBufferIndex, _dummy_sink, _dummy_sink2);
	while (_file_out && _iomgr->_my_source2->chunk_ready())
	{
		_file_out->process();
	}
#endif	

#if DO_OUTPUT
	
#if !BUFFER_BEFORE_COPY
	while (_main_mgr._c == 0)
	{
		pthread_cond_wait(&_gen_done, &_io_lock);
	}
	
	//chunk_t *cue_chk = _cue->next(chk1, chk2);
	_main_out->process(_doubleBufferIndex);
	
	ASIOOutputReady();
	
	if (_file_out && _file_src && _file_mgr._c)
	{
		_file_out->process();
	}
#else
    while (_need_buffers)
    {
        pthread_cond_wait(&_gen_done, &_io_lock);
    }
	_iomgr->switchBuffers(doubleBufferIndex);
    _need_buffers = true;
#endif

	pthread_cond_signal(&_do_gen);
#endif

#if ECHO_INPUT
	// copy input
	memcpy(_buffer_infos[2].buffers[doubleBufferIndex], _buffer_infos[0].buffers[doubleBufferIndex], BUFFERSIZE*sizeof(short));
	memcpy(_buffer_infos[3].buffers[doubleBufferIndex], _buffer_infos[1].buffers[doubleBufferIndex], BUFFERSIZE*sizeof(short));
#endif

	pthread_mutex_unlock(&_io_lock);
}

void ASIOProcessor::GenerateOutput()
{
#ifdef BUFFER_BEFORE_COPY
	if (!_main_mgr._c) {
#endif
	chunk_t *chk1 = _tracks[0]->next();
	chk1->add_ref();
	chunk_t *chk2 = _tracks[1]->next();
	chk2->add_ref();
	
	// fix 2nd output
	chk2->add_ref();
	_2_mgr._c = chk2;

	chunk_t *out = _main_src->next(chk1, chk2);
	if (_main_src->_clip)
		_tracks[0]->set_clip(_main_src==_master_xfader?1:2);
    
	_main_mgr._c = out;
/*	if (_file_src == _main_src) 
	{
		out->add_ref();
		_file_mgr._c = out;
		T_allocator<chunk_t>::free(chk1);
		T_allocator<chunk_t>::free(chk2);
	}
	else if (_file_src)
	{
		_file_mgr._c = _file_src->next(chk1, chk2);
		if (_file_src->_clip)
			_tracks[0]->set_clip(_file_src==_master_xfader?1:2);
	}
	else
*/	{
		T_allocator<chunk_t>::free(chk1);
		T_allocator<chunk_t>::free(chk2);
	}
#ifdef BUFFER_BEFORE_COPY
	}
	//else
//	{
//		assert("shouldnt really happen" && false);
//	}
#endif

#if BUFFER_BEFORE_COPY
    _iomgr->processOutputs();
    _need_buffers = false;
	pthread_cond_signal(&_gen_done);
    
    if (_file_out && _file_src && _file_mgr._c)
	{
		_file_out->process();
	}
#endif
}

void ASIOProcessor::GenerateLoop(pthread_t th)
{
#ifdef WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
	pthread_mutex_lock(&_io_lock);
	_gen_th = th;
	while (!_finishing)
	{
#if BUFFER_BEFORE_COPY
		while (_need_buffers)
#else
		if (_main_mgr._c == 0)
#endif
		{
			GenerateOutput();
			pthread_cond_signal(&_gen_done);
		}
#if !BUFFER_BEFORE_COPY
		else
#endif
		{
			pthread_cond_wait(&_do_gen, &_io_lock);
		}
	}
	pthread_mutex_unlock(&_io_lock);
	pthread_exit(0);
}

#if 0
void fAStIOProcessor::MainLoop()
{
	while (!_finishing)
	{
		if (_buf_q.count() == 0)
		{
Generate:
			GenerateOutput();
		}

		// render

		if (_buf_q.count() == 0)
			goto Generate;

		// load track
	}
}

void fAStIOProcessor::GenerateOutput()
{
	chunk_t *chk1 = _tracks[0]->next();
	chk1->add_ref();
	chunk_t *chk2 = _tracks[1]->next();
	chk2->add_ref();
	chunk_t *out = _main_src->next(chk1, chk2);
	if (_main_src->_clip)
		_tracks[0]->set_clip(_main_src==_master_xfader?1:2);
	//_main_mgr._c = out;
	if (_file_src == _main_src) 
	{
		out->add_ref();
		_file_mgr._c = out;
		T_allocator<chunk_t>::free(chk1);
		T_allocator<chunk_t>::free(chk2);
	}
	else if (_file_src)
	{
		_file_mgr._c = _file_src->next(chk1, chk2);
		if (_file_src->_clip)
			_tracks[0]->set_clip(_file_src==_master_xfader?1:2);
	}
	else
	{
		T_allocator<chunk_t>::free(chk1);
		T_allocator<chunk_t>::free(chk2);
	}
}

void fAStIOProcessor::BufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
{
	//sem_wait(&_sem);
	char *bufs = _buf_q.pop_wait();
	memcpy(_buffer_infos[2].buffers[doubleBufferIndex], bufs, _bufSize);
	memcpy(_buffer_infos[3].buffers[doubleBufferIndex], bufs+_bufSize, _bufSize);
	//sem_post(&_sem);
}
#endif
