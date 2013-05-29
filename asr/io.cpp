#include "config.h"
#include "io.h"
#include "tracer.h"

#include <queue>
#include <algorithm>

#include <pthread.h>
#include <semaphore.h>

#include "asio.cpp"
#include "iomgr.cpp"

#include "compressor.h"

#include "track.h"

ASIOProcessor::ASIOProcessor() :
	_running(false),
	_speed(1.0),
	//_default_src(L"F:\\Beatport Music\\Sander van Doorn - Riff (Original Mix).mp3"),
	_default_src("/Users/paulyc/Downloads/3863535_Fall_With_Me_feat__The_Glass_Child_Original_Mix.aiff"),
//4233537_Mesocyclone_Original_Mix.aiff
//4045992_Wayfarer_Original_Mix.aiff"),
//_default_src("clip.wav"),
	//_default_src(L"H:\\Music\\Heatbeat - Hadoken (Original Mix).wav"),
	//_default_src(L"H:\\Music\\Sean Tyas - Melbourne (Original Mix).wav"),
	//_default_src(L"H:\\Music\\Super8 & Tab, Anton Sonin - Black Is The New Yellow (Activa Remix).wav"),
    _my_pk_det(0),
    _resample(false),
	_resamplerate(48000.0),
	_outputTime(0.0),
	_src_active(false),
	_log("mylog.txt"),
	_file_out(0),
    _master_xfader(0),
    _cue(0),
    _aux(0),
	_ui(0),
	_finishing(false),
	_device(0),
	_sync_cue(false),
	_midi_controller(0),
    _inputStream(0),
    _outputStream(0),
    _inputStreamProcessor(0),
    _outputStreamProcessor(0),
    _client(CFSTR("The MIDI Client")),
    _gen(0),
    _gain1(0), _gain2(0)
{
	Init();
}

ASIOProcessor::~ASIOProcessor()
{
	Destroy();
}

void ASIOProcessor::Init()
{
#if WINDOWS
	CoInitialize(0);
    WindowsDriverFactory fac;
    IAudioDeviceFactory *deviceFac = 0;
    std::vector<const IAudioDriverDescriptor*> drivers = fac.Enumerate();
    for (std::vector<const IAudioDriverDescriptor*>::iterator i = drivers.begin();
         i != drivers.end();
         i++)
    {
        if ((*i)->GetName() == std::string("ASIO"))
            deviceFac = (*i)->Instantiate();
    }
    
    if (!deviceFac)
    {
        throw string_exception("Failed to create ASIO device factory");
    }
    
    std::vector<const IAudioDeviceDescriptor*> devices = deviceFac->Enumerate();
    for (std::vector<const IAudioDeviceDescriptor*>::iterator devIter = devices.begin();
         devIter != devices.end();
         devIter++)
    {
        if (dev->GetName() == std::string("Saffire"))
        {
            _device = (*devIter)->Instantiate();
        }
    }
    if (!_device)
    {
        for (std::vector<const IAudioDeviceDescriptor*>::iterator devIter = devices.begin();
             devIter != devices.end();
             devIter++)
        {
            if (dev->GetName() == std::string("Built-in Output"))
            {
                _device = (*devIter)->Instantiate();
            }
        }
    }
    if (!_device)
    {
        throw string_exception("Failed to create audio device");
    }
    delete deviceFac;
	
    _iomgr->createBuffers(this);
#elif MAC
    MacAudioDriverFactory fac;
    IAudioDeviceFactory *deviceFac = 0;
    std::vector<const IAudioDriverDescriptor*> drivers = fac.Enumerate();
    for (std::vector<const IAudioDriverDescriptor*>::iterator i = drivers.begin();
         i != drivers.end();
         i++)
    {
        if ((*i)->GetName() == std::string("CoreAudio"))
            deviceFac = (*i)->Instantiate();
    }
    
    if (!deviceFac)
    {
        throw string_exception("Failed to create CoreAudio device factory");
    }
    
    std::vector<const IAudioDeviceDescriptor*> devices = deviceFac->Enumerate();
    for (std::vector<const IAudioDeviceDescriptor*>::iterator devIter = devices.begin();
         devIter != devices.end();
         devIter++)
    {
        if ((*devIter)->GetName() == std::string("Saffire"))
        {
            _device = (*devIter)->Instantiate();
        }
    }
    if (!_device)
    {
        for (std::vector<const IAudioDeviceDescriptor*>::iterator devIter = devices.begin();
             devIter != devices.end();
             devIter++)
        {
            if ((*devIter)->GetName() == std::string("Built-in Output"))
            {
                _device = (*devIter)->Instantiate();
            }
        }
    }
    if (!_device)
    {
        throw string_exception("Failed to create audio device");
    }
    delete deviceFac;
#endif
    
    _need_buffers = true;
    
    IMIDIDevice *dev = 0; //midifac.Instantiate(1, true);
    
#if WINDOWS
	Win32MIDIDeviceFactory midifac;
#elif MAC
    std::vector<const IMIDIDeviceDescriptor*> midiDevices = _client.Enumerate();
    for (int i=0; i<midiDevices.size(); ++i)
    {
        //"Pro24"
        if (midiDevices[i]->GetDeviceName() == std::string("PIONEER CDJ-350"))
        {
            dev = midiDevices[i]->Instantiate();
            break;
        }
    }
#else
    DummyMIDIDeviceFactory midifac;
#endif
	
	if (dev)
	{
		printf("Have midi\n");
		_midi_controller = new CDJ350MIDIController(dev);
	}
}

// stop using ui before destroying
void ASIOProcessor::Finish()
{
	_finishing = true;
	if (_running)
		Stop();
    
#if GENERATE_LOOP
	_do_gen.signal();
	pthread_join(_gen_th, 0);
    
	// something weird with this? go before the last lines?
	_io_lock.acquire();
	_gen_done.signal();
	_io_lock.release();
#endif
}

void ASIOProcessor::Destroy()
{
    delete _midi_controller;
    delete _filter_controller;
    
    delete _inputStream;
    delete _inputStreamProcessor;
    delete _outputStream;
    delete _outputStreamProcessor;
    delete _device;
    
#if WINDOWS
	CoUninitialize();
#endif
    
	delete _my_pk_det;
    //	delete _my_gain;
	
	delete _file_out;
	delete _cue;
	delete _master_xfader;
	delete _aux;
    
    // probably want to join on worker threads exiting so we dont destroy this while generating
    delete _gen;
    delete _gain1;
    delete _gain2;
    
	delete _tracks[0];
	delete _tracks[1];
	_tracks.resize(0);
    
	T_allocator<chunk_t>::gc();
	T_allocator<chunk_t>::dump_leaks();
    
#if WINDOWS
	Tracer::printTrace();
#endif
}

void ASIOProcessor::CreateTracks()
{
	_tracks.push_back(new AudioTrack<chunk_t>(this, 1, _default_src));
	_tracks.push_back(new AudioTrack<chunk_t>(this, 2, _default_src));

	_filter_controller = new track_t::controller_t(_tracks[0], _tracks[1]);
    if (_midi_controller)
        _midi_controller->SetControlListener(_filter_controller);

	//_bp_filter = new bandpass_filter_td<chunk_t>(_tracks[0], 20.0, 200.0, 48000.0, 48000.0);
	
    //const double gain =
    _gain1 = new gain<T_source<chunk_t> >(_tracks[0]);
    _gain1->set_gain_db(-3.0);
    _gain2 = new gain<T_source<chunk_t> >(_tracks[1]);
    _gain2->set_gain_db(-3.0);
    
    _gen = new ChunkGenerator(_device->GetDescriptor()->GetBufferSizeFrames());
    _gen->AddChunkSource(_gain1, 1);
    _gen->AddChunkSource(_gain2, 2);
    
    /*
    _master_xfader = new xfader<T_source<chunk_t> >(_tracks[0], _tracks[1]);
    _master_xfader->set_mix(0);
	_cue = new xfader<T_source<chunk_t> >(_tracks[0], _tracks[1]);
	_cue->set_mix(1000);
	
	//const double gain = 1.0;
	_cue->set_gain(1, gain);
	_cue->set_gain(2, gain);
	_master_xfader->set_gain(1, gain);
	_master_xfader->set_gain(2, gain);
	//_aux = new xfader<track_t>(_tracks[0], _tracks[1]);
    */
    
    
    std::vector<const IAudioStreamDescriptor*> streams = _device->GetStreams();
    for (std::vector<const IAudioStreamDescriptor*>::iterator i = streams.begin();
         i != streams.end();
         i++)
    {
        if ((*i)->GetStreamType() == IAudioStreamDescriptor::Input)
        {
            //_inputStream = (*i)->GetStream();
        }
        else
        {
            _outputStream = (*i)->GetStream();
            _outputStreamProcessor = new CoreAudioOutputProcessor(_outputStream->GetDescriptor());
            _outputStreamProcessor->AddOutput(CoreAudioOutput(_gen, 1, 0, 1));
            _outputStreamProcessor->AddOutput(CoreAudioOutput(_gen, 2, 2, 3));
        }
    }
    _outputStream->SetProc(_outputStreamProcessor);

	/*asio_sink<chunk_t, chunk_buffer, int32_t> *main_out = new asio_sink<chunk_t, chunk_buffer, int32_t>(
		&_main_mgr,
		_iomgr->getBuffers(2), 
		_iomgr->getBuffers(3),
		_iomgr->getBufferSize());
	_iomgr->addOutput(main_out);

	asio_sink<chunk_t, chunk_buffer, int32_t> *out_2 = new asio_sink<chunk_t, chunk_buffer, int32_t>(
		&_2_mgr,
		_iomgr->getBuffers(4), 
		_iomgr->getBuffers(5),
		_iomgr->getBufferSize());
	_iomgr->addOutput(out_2);
	
	T_sink<chunk_t> *dummy_sink = new T_sink<chunk_t>(0);
	asio_source<int32_t, SamplePairf, chunk_t> *input1 = new asio_source<int32_t, SamplePairf, chunk_t>(
		dummy_sink,
		_iomgr->getBufferSize(),
		_iomgr->getBuffers(0),
		_iomgr->getBuffers(1));
	_iomgr->addInput(input1);*/

#if !PARALLELS_ASIO
	/*T_sink<chunk_t> *dummy_sink2 = new T_sink<chunk_t>(input1);
	asio_source<int32_t, SamplePairf, chunk_t> *input2 = new asio_source<int32_t, SamplePairf, chunk_t>(
		dummy_sink2,
		_iomgr->getBufferSize(),
		_iomgr->getBuffers(6),
		_iomgr->getBuffers(7));
	_iomgr->addInput(input2);
*/
	//_aux = new ChunkConverter<chunk_t, chunk_time_domain_1d<SamplePairInt16, chunk_t::chunk_size> >(input2);
#endif

	try {
	//	_my_gain = new gain<asio_source<short, SamplePairf, chunk_t> >(_my_source);
	//	_my_gain->set_gain_db(36.0);
        
	//	_my_pk_det = new peak_detector<SamplePairf, chunk_t, chunk_t::chunk_size>(input1);
	//	dummy_sink->set_src(_my_pk_det);

	//	_my_raw_output = new file_raw_output<chunk_t>(_my_pk_det);
	} catch (std::exception e) {
		throw e;
	}

	_main_src = _master_xfader;
	_file_src = 0;

#if GENERATE_LOOP
	Worker::do_job(new Worker::callback_th_job<ASIOProcessor>(
		functor1<ASIOProcessor, pthread_t>(this, &ASIOProcessor::GenerateLoop)),
		false, true
	);
#endif
}

void ASIOProcessor::Reconfig()
{
}

void ASIOProcessor::Start()
{
	_running = true;
	_src_active = true;
	if (_midi_controller)
		_midi_controller->Start();
	_device->Start();
}

void ASIOProcessor::Stop()
{
	_running = false;
	_src_active = false;
	if (_midi_controller)
		_midi_controller->Stop();
    _device->Stop();
}

void ASIOProcessor::BufferSwitch(long doubleBufferIndex)
{
#if 0
	_waiting = true;
	_io_lock.acquire();
	_waiting = false;
	_doubleBufferIndex = doubleBufferIndex;

#define BROKEN 1
#if CARE_ABOUT_INPUT && !BROKEN
	_iomgr->processInputs(_doubleBufferIndex);
	while (_file_out && _iomgr->_my_source2->chunk_ready())
	{
		_file_out->process();
	}
#endif	

#if DO_OUTPUT
	
    while (_need_buffers)
    {
		_gen_done.wait(_io_lock);
    }
	_iomgr->switchBuffers(doubleBufferIndex);
    _need_buffers = true;

	_do_gen.signal();
#endif

#if ECHO_INPUT
	// copy input
	memcpy(_buffer_infos[2].buffers[doubleBufferIndex], _buffer_infos[0].buffers[doubleBufferIndex], BUFFERSIZE*sizeof(short));
	memcpy(_buffer_infos[3].buffers[doubleBufferIndex], _buffer_infos[1].buffers[doubleBufferIndex], BUFFERSIZE*sizeof(short));
#endif

	_io_lock.release();
#endif
}

void ASIOProcessor::GenerateOutput()
{
	if (!_main_mgr._c) {
		chunk_t *chk1 = _tracks[0]->next();
		chunk_t *chk2 = _tracks[1]->next();

		// just replace this with output gain
		chunk_t *out = _main_src->next(chk1, chk2);
	//	NormalizingCompressor<chunk_t>::process(out);

		if (_main_src->_clip)
			_tracks[0]->set_clip(_main_src==_master_xfader?1:2);
		_main_mgr._c = out;

		out = _cue->next(chk1, chk2);
	//	NormalizingCompressor<chunk_t>::process(out);
		_2_mgr._c = out;

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
	}

    //_iomgr->processOutputs();
    _need_buffers = false;
	_gen_done.signal();
    
    if (_file_out && _file_src && _file_mgr._c)
	{
		_file_out->process();
	}
}

void ASIOProcessor::GenerateLoop(pthread_t th)
{
#ifdef WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
	_io_lock.acquire();
	_gen_th = th;
	while (!_finishing)
	{
		while (_need_buffers)
		{
			GenerateOutput();
			_gen_done.signal();
		}
		_do_gen.wait(_io_lock);
	}
	_io_lock.release();
	pthread_exit(0);
}

int ASIOProcessor::get_track_id_for_filter(void *filt) const
{
    return _tracks[0]->get_source() == filt ? 1 : 2;
}

void ASIOProcessor::trigger_sync_cue(int caller_id)
{
    if (_sync_cue)
    {
        track_t *other_track = GetTrack(caller_id == 1 ? 2 : 1);
        other_track->goto_cuepoint(false);
        other_track->play();
    }
}
