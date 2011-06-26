#include "config.h"
#include "io.h"

#include <queue>

#include <pthread.h>

extern ASIOProcessor<SamplePairf, short> * asio;

/*
template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T, int chunk_size>
void asio_sink<Input_Sample_T, Output_Sample_T, Chunk_T, chunk_size, false>::process()
{
	// assume 2 channels non-interleaved same data format
	memcpy(asio->_buffer_infos[2].buffers[asio->_doubleBufferIndex], t->_data, sizeof(t->_data)/2);
	memcpy(asio->_buffer_infos[3].buffers[asio->_doubleBufferIndex], (char*)(t->_data)+sizeof(t->_data)/2, sizeof(t->_data)/2);
}
*/

template <typename Chunk_T, typename Source_T, typename Output_Sample_T>
void asio_sink<Chunk_T, Source_T, Output_Sample_T>::process(int dbIndex)
{
	size_t to_write = _buf_size, loop_write, written=0;
	int stride = 2;
	Output_Sample_T *write, *end_write;
	typename Chunk_T::sample_t *read;
	while (to_write > 0)
	{
		if (!_chk || _read - _chk->_data >= chunk_t::chunk_size)
		{
			T_allocator<chunk_t>::free(_chk);
			_chk = 0;

			pthread_mutex_lock(&_src_lock);
			_chk = _src_t->next();
			pthread_mutex_unlock(&_src_lock);
			_read = _chk->_data;
		}
		loop_write = min(to_write, chunk_t::chunk_size - (_read - _chk->_data));
		for (write = _buffers[0][dbIndex] + written,
			end_write = write + loop_write,
			read = _read;
			write != end_write;
			++write, ++read)
		{
			if ((*read)[0] < 0.0f)
				*write = Output_Sample_T(max(-1.0f, (*read)[0]) * -SHRT_MIN);
			else
				*write = Output_Sample_T(min(1.0f, (*read)[0]) * SHRT_MAX);
		}
		for (write = _buffers[1][dbIndex] + written,
			end_write = write + loop_write,
			read = _read;
			write != end_write;
			++write, ++read)
		{
			if ((*read)[1] < 0.0f)
				*write = Output_Sample_T(max(-1.0f, (*read)[1]) * -SHRT_MIN);
			else
				*write = Output_Sample_T(min(1.0f, (*read)[1]) * SHRT_MAX);
		}
		_read += loop_write;
		to_write -= loop_write;
		written += loop_write;
	}
}

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
asio_source<Input_Sample_T, Output_Sample_T, Chunk_T>::asio_source()
{
	_chk_working = T_allocator<Chunk_T>::alloc();
	_chk_ptr = _chk_working->_data;
	sem_init(&_next_sem, 0, 1);
}

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
asio_source<Input_Sample_T, Output_Sample_T, Chunk_T>::~asio_source()
{
	sem_destroy(&_next_sem);
	T_allocator<Chunk_T>::free(_chk_working);
}

template <>
void asio_source<short, SamplePairf, chunk_t>::have_data()
{
	size_t buf_sz = asio->_bufSize;
	short *input0 = (short*)asio->_buffer_infos[0].buffers[asio->_doubleBufferIndex], 
		*input1 = (short*)asio->_buffer_infos[1].buffers[asio->_doubleBufferIndex], 
		*end0 = input0 + buf_sz, 
		*end1 = input1 + buf_sz;
	SamplePairf *end_chk = _chk_working->_data + chunk_t::chunk_size;
	do
	{
		while (input0 < end0 && input1 < end1 && _chk_ptr < end_chk)
		{
			if (*input0 >= 0)
				(*_chk_ptr)[0] = *input0++ / float(SHRT_MAX);
			else
				(*_chk_ptr)[0] = *input0++ / float(-SHRT_MIN);

			if (*input1 >= 0)
				(*_chk_ptr++)[1] = *input1++ / float(SHRT_MAX);
			else
				(*_chk_ptr++)[1] = *input1++ / float(-SHRT_MIN);
		}
		if (_chk_ptr == end_chk)
		{
			sem_wait(&_next_sem);
			_chks_next.push(_chk_working);
			sem_post(&_next_sem);
			_chk_working = T_allocator<chunk_t>::alloc();
			_chk_ptr = _chk_working->_data;
			end_chk = _chk_ptr + chunk_t::chunk_size;
		}
		if (input0 == end0)
			break;
	} while (true);
}

template <typename Input_Sample_T, typename Output_Sample_T, typename Chunk_T>
Chunk_T* asio_source<Input_Sample_T, Output_Sample_T, Chunk_T>::next()
{
	while (1)
	{
		sem_wait(&_next_sem);
		if (_chks_next.size() > 0)
			break;
		sem_post(&_next_sem);
	}
	Chunk_T *n = _chks_next.front();
	_chks_next.pop();
	sem_post(&_next_sem);
	return n;
}

/*
Purpose:
bufferSwitchTimeInfo indicates that both input and output are to be processed. It also passes
extended time information (time code for synchronization purposes) to the host application and
back to the driver.
*/
void bufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
{
	asio->BufferSwitch(doubleBufferIndex, directProcess);
}

ASIOTime* bufferSwitchTimeInfo(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess)
{
	bufferSwitch(doubleBufferIndex, directProcess);
	return params;
}

/*
This callback will inform the host application that a sample rate change was detected (e.g. sample
rate status bit in an incoming S/PDIF or AES/EBU signal changes).
*/
void sampleRateDidChange(ASIOSampleRate sRate)
{// dont care
}

long asioMessage(long selector, long value, void *message, double *opt)
{
	switch (selector)
	{
	case kAsioSelectorSupported:
		switch (value)
		{
		case kAsioSelectorSupported:
		case kAsioSupportsTimeInfo:
		case kAsioEngineVersion:
			return 1L;
		default:
			return 0;
		}
		break;
	case kAsioSupportsTimeInfo:
//		return 1L;
		return 0;
	case kAsioEngineVersion:
		return 2;
	default:
		return 0;
	}
	return ASE_OK;
}

template <>
ASIOProcessor<SamplePairf, short>::ASIOProcessor() :
	_running(false),
	_speed(1.0),
	_default_src(L"F:\\My Music\\Flip & Fill - I Wanna Dance With Somebody (Resource Mix).wav"),
	//_default_src(L"H:\\Music\\Heatbeat - Hadoken (Original Mix).wav"),
	//_default_src(L"H:\\Music\\Sean Tyas - Melbourne (Original Mix).wav"),
	//_default_src(L"H:\\Music\\Super8 & Tab, Anton Sonin - Black Is The New Yellow (Activa Remix).wav"),
	_resample(false),
	_resamplerate(48000.0),
	_buffer_infos(0),
	_input_channel_infos(0),
	_output_channel_infos(0),
	_outputTime(0.0),
	_src_active(false),
	_log("mylog.txt"),
	_my_source(0),
	_main_out(0),
	_ui(0)
{
	Init();
}

template <typename Input_Buffer_T, typename Output_Buffer_T>
ASIOProcessor<Input_Buffer_T, Output_Buffer_T>::~ASIOProcessor()
{
	Destroy();
}

template <typename Input_Buffer_T, typename Output_Buffer_T>
void ASIOProcessor<Input_Buffer_T, Output_Buffer_T>::Init()
{
	ASIOError e;
	long minSize, maxSize, preferredSize, granularity;

	pthread_mutex_init(&_io_lock, 0);

	try {
		_my_controller = new controller_t;

		_tracks.push_back(new SeekablePitchableFileSource<chunk_t>(1, _default_src, &_io_lock));
		_tracks.push_back(new SeekablePitchableFileSource<chunk_t>(2, _default_src, &_io_lock));

#if CARE_ABOUT_INPUT
		_my_source = new asio_source<short, SamplePairf, chunk_t>;
		_my_pk_det = new peak_detector<SamplePairf, chunk_time_domain_1d<SamplePairf, 4096>, 4096>(_my_source);
		_my_raw_output = new file_raw_output<chunk_t>(_my_pk_det);
#endif
	} catch (std::exception e) {
		throw e;
	}

	CoInitialize(0);
	LoadDriver();

	_drv_info.asioVersion = 2;
	_drv_info.sysRef = 0;
	ASIO_ASSERT_NO_ERROR(ASIOInit(&_drv_info));

	/*
		ASIOGetChannels();
		ASIOGetBufferSize();
		ASIOCanSampleRate();
		ASIOGetSampleRate();
		ASIOGetClockSources();
		ASIOGetChannelInfo();
		ASIOSetSampleRate();
		ASIOSetClockSource();
		ASIOGetLatencies();
		ASIOFuture();
	*/
	
	ASIO_ASSERT_NO_ERROR(ASIOGetBufferSize(&minSize, &maxSize, &preferredSize, &granularity));
	ASIO_ASSERT_NO_ERROR(ASIOGetChannels(&_numInputChannels, &_numOutputChannels));

	_input_channel_infos = (ASIOChannelInfo*)malloc(_numInputChannels*sizeof(ASIOChannelInfo));
	for (long ch = 0; ch < _numInputChannels; ++ch)
	{
		_input_channel_infos[ch].channel = ch;
		_input_channel_infos[ch].isInput = ASIOTrue;
		ASIO_ASSERT_NO_ERROR(ASIOGetChannelInfo(&_input_channel_infos[ch]));
		printf("Input Channel %d\nName: %s\nSample Type: %d\n\n", ch, _input_channel_infos[ch].name, _input_channel_infos[ch].type);
	}

	_output_channel_infos = (ASIOChannelInfo*)malloc(_numOutputChannels*sizeof(ASIOChannelInfo));
	for (long ch = 0; ch < _numOutputChannels; ++ch)
	{
		_output_channel_infos[ch].channel = ch;
		_output_channel_infos[ch].isInput = ASIOFalse;
		ASIO_ASSERT_NO_ERROR(ASIOGetChannelInfo(&_output_channel_infos[ch]));
		printf("Output Channel %d\nName: %s\nSample Type: %d\n\n", ch, _output_channel_infos[ch].name, _output_channel_infos[ch].type);
	}
	
	int ch_input_l, ch_input_r, ch_output_l, ch_output_r;
#if CHOOSE_CHANNELS
	printf("choose 4 channels (in L, in R, out L, out R) or -1 to quit, maybe: ");
	std::cin >> ch_input_l;
	std::cin >> ch_input_r >> ch_output_l >> ch_output_r;
#else
	ch_input_l = 18;
	ch_input_r = 19;
	ch_output_l = 0;
	ch_output_r = 1;
#endif

#if CREATE_BUFFERS
	_buffer_infos_len = 4;
	_buffer_infos = (ASIOBufferInfo*)malloc(_buffer_infos_len*sizeof(ASIOBufferInfo));
	_buffer_infos[0].isInput = ASIOTrue;
	//_buffer_infos[0].channelNum = 2;
	_buffer_infos[0].channelNum = ch_input_l;
	_buffer_infos[1].isInput = ASIOTrue;
	//_buffer_infos[1].channelNum = 3;
	_buffer_infos[1].channelNum = ch_input_r;
	_buffer_infos[2].isInput = ASIOFalse;
	_buffer_infos[2].channelNum = ch_output_l;
	_buffer_infos[3].isInput = ASIOFalse;
	_buffer_infos[3].channelNum = ch_output_r;

	_cb.bufferSwitch = bufferSwitch;
	_cb.bufferSwitchTimeInfo = bufferSwitchTimeInfo;
	_cb.sampleRateDidChange = sampleRateDidChange;
	_cb.asioMessage = asioMessage;
	_bufSize = preferredSize;
	ASIO_ASSERT_NO_ERROR(ASIOCreateBuffers(_buffer_infos, _buffer_infos_len, _bufSize, &_cb));
#endif

//	_bus_matrix = new io_matrix<track_t, bus<chunk_t> >;
	_master_xfader = new xfader<track_t>(_tracks[0], _tracks[1]);
	_cue = new xfader<track_t>(_tracks[0], _tracks[1]);
	_aux = new xfader<track_t>(_tracks[0], _tracks[1]);
//	_master_bus = new bus<chunk_t>(_bus_matrix);
//	_cue_bus = new bus<chunk_t>(_bus_matrix);
//	_aux_bus = new bus<chunk_t>(_bus_matrix);
	//_main_out = new asio_sink<chunk_t, bus<chunk_t>, short>(_master_bus, 
	_main_out = new asio_sink<chunk_t, chk_mgr, short>(&_main_mgr,
		(short**)_buffer_infos[2].buffers, 
		(short**)_buffer_infos[3].buffers,
		_bufSize);
	_file_out = new file_raw_output<chunk_t>(_master_bus, "out.raw");
//	_bus_matrix->map(_tracks[0], _master_bus);
//	_bus_matrix->map(_tracks[1], _master_bus);
//	_master_bus->map_output(_main_out);

	_main_src = _master_xfader;
	_file_src = 0;

	ASIOSampleRate r;
/*		ASIOSampleType t;
	r = 44100.0;
	e = ASIOCanSampleRate(r); */
	ASIO_ASSERT_NO_ERROR(ASIOGetSampleRate(&r));
	printf("At a sampling rate of %f\n", r);
//	ASIOSetSampleRate(44100.0);
}

template <typename Input_Buffer_T, typename Output_Buffer_T>
ASIOError ASIOProcessor<Input_Buffer_T, Output_Buffer_T>::Start()
{
	_running = true;
	_src_active = true;
	return ASIOStart();
}

template <typename Input_Buffer_T, typename Output_Buffer_T>
ASIOError ASIOProcessor<Input_Buffer_T, Output_Buffer_T>::Stop()
{
	_running = false;
	_src_active = false;
	return ASIOStop();
}


template <typename Input_Buffer_T, typename Output_Buffer_T>
void ASIOProcessor<Input_Buffer_T, Output_Buffer_T>::ProcessInput()
{
	if (_my_source)
	{
		_my_source->have_data();
		while (_my_source->chunk_ready())
		{
			_my_raw_output->process();
			_my_controller->process(_resample_filter, _my_pk_det);
		}
		_speed = _my_pk_det->p_begin.mod;
	}
}

template <typename Input_Buffer_T, typename Output_Buffer_T>
void ASIOProcessor<Input_Buffer_T, Output_Buffer_T>::BufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
{
	pthread_mutex_lock(&_io_lock);
	_doubleBufferIndex = doubleBufferIndex;

#if CARE_ABOUT_INPUT
	this->ProcessInput();
#endif	

#if DO_OUTPUT
	if (_main_mgr._c == 0)
	{
		chunk_t *chk1 = _tracks[0]->next();
		chk1->add_ref();
		chunk_t *chk2 = _tracks[1]->next();
		chk2->add_ref();
		chunk_t *out = _main_src->next(chk1, chk2);
		if (_main_src->_clip)
			_ui->set_clip(1);
		_main_mgr._c = out;
		if (_file_src == _main_src) 
		{
			out->add_ref();
			_file_out->process(out);
			T_allocator<chunk_t>::free(chk1);
			T_allocator<chunk_t>::free(chk2);
		}
		else if (_file_src)
		{
			_file_out->process(_file_src->next(chk1, chk2));
			if (_main_src->_clip)
				_ui->set_clip(2);
		}
		else
		{
			T_allocator<chunk_t>::free(chk1);
			T_allocator<chunk_t>::free(chk2);
		}
	}
	
	//chunk_t *cue_chk = _cue->next(chk1, chk2);
	_main_out->process(_doubleBufferIndex);
#endif


#if ECHO_INPUT
	// copy input
	memcpy(_buffer_infos[2].buffers[doubleBufferIndex], _buffer_infos[0].buffers[doubleBufferIndex], BUFFERSIZE*sizeof(short));
	memcpy(_buffer_infos[3].buffers[doubleBufferIndex], _buffer_infos[1].buffers[doubleBufferIndex], BUFFERSIZE*sizeof(short));
#endif
	pthread_mutex_unlock(&_io_lock);
}

#if !USE_SSE2 && !NON_SSE_INTS
static float * LoadHelp(float *toBuf, short *fromBuf)
#else
static float * LoadHelp(float *toBuf, int *fromBuf)
#endif
{
#ifdef _DEBUG
	printf("ASIOProcessor::LoadHelp(%p, %p)\n", toBuf, fromBuf);
#endif

#if C_LOOPS 
#if !NON_SSE_INTS
#error hello1
	short val;
	for (short *p=fromBuf; p<fromBuf+BUFFERSIZE; ++p) {
		val = *p;
		if (val >= 0)
			*toBuf++ = val / float(SHRT_MAX);
		else
			*toBuf++ = val / float(-SHRT_MIN);
	}
#else
//#error hello2
	int val;
	for (int *p=fromBuf; p<fromBuf+BUFFERSIZE; ++p) {
		val = *p;
		if (val >= 0)
			*toBuf++ = val / float(INT_MAX);
		else
			*toBuf++ = val / float(-INT_MIN);
	}
#endif
//#error hello3
#else
	int dummy = 0;
	++dummy;
#if USE_SSE2
	__m128 div_init = {(float)INT_MAX, (float)INT_MAX, (float)INT_MAX, (float)INT_MAX};
#else
	__m128 div_init = {32767.0f, 32767.0f, 32767.0f, 32767.0f};
#endif
	__asm
	{
		mov eax, fromBuf
		mov ecx, eax
#if !USE_SSE2
		add ecx, BUFFERSIZE*2
#else
		add ecx, BUFFERSIZE*4
#endif
		//lea ebx, this
		//mov ebx, [ebx]._bufLEnd
		lea edx, div_init
		movaps xmm1, [edx]
		
		mov edx, toBuf
top1:
		cmp eax, ecx
		jz end1
#if !USE_SSE2
		fild word ptr[eax]
		fstp [ebx]
		add ebx, 4
		add eax, 2
#else
		cvtdq2ps xmm0, [eax]
		divps xmm0, xmm1
		movaps [edx], xmm0
		add edx, 16
		add eax, 16
#endif
		jmp top1
end1:
			
		//lea edx, this
		//mov [edx]._bufLEnd, ebx
		mov toBuf, edx
	}
#endif
	return toBuf;
}

template <>
void ASIOProcessor<SamplePairf, short>::SetSrc(int ch, const wchar_t *fqpath)
{
	bool was_active = _src_active;
	if (was_active)
		Stop();
	try {
		_tracks[ch-1]->set_source_file(fqpath, &_io_lock);
	} catch (std::exception e) {
	}
	if (was_active)
		Start();
}

template <typename Input_Buffer_T, typename Output_Buffer_T>
void ASIOProcessor<Input_Buffer_T, Output_Buffer_T>::LoadDriver()
{
#if WINDOWS
#define ASIODRV_DESC		L"description"
#define INPROC_SERVER		L"InprocServer32"
#define ASIO_PATH			L"software\\asio"
#define COM_CLSID			L"clsid"
#define MAXPATHLEN			512
#define MAXDRVNAMELEN		128
	CLSID clsid;// = {0x835F8B5B, 0xF546, 0x4881, 0xB4, 0xA3, 0xB6, 0xB5, 0xE8, 0xE3, 0x35, 0xEF};
	HKEY hkEnum, hkSub;
	TCHAR keyname[MAXDRVNAMELEN];
	DWORD index = 0;
#if 0
	cr = RegOpenKey(HKEY_LOCAL_MACHINE, ASIO_PATH, &hkEnum);
	while (cr == ERROR_SUCCESS) {
		if ((cr = RegEnumKey(hkEnum, index++, keyname, MAXDRVNAMELEN))== ERROR_SUCCESS) {
		//	lpdrvlist = newDrvStruct (hkEnum,keyname,0,lpdrvlist);
			if ((cr = RegOpenKeyEx(hkEnum, keyname, 0, KEY_READ, &hkEnum)) == ERROR_SUCCESS) {
				atatype = REG_SZ; datasize = 256;
				cr = RegQueryValueEx(hkSub, COM_CLSID, 0, &datatype,(LPBYTE)databuf,&datasize);
				if (cr == ERROR_SUCCESS) {
					MultiByteToWideChar(CP_ACP,0,(LPCSTR)databuf,-1,(LPWSTR)wData,100);
					if ((cr = CLSIDFromString((LPOLESTR)wData,(LPCLSID)&clsid)) == S_OK) {
						memcpy(&lpdrv->clsid,&clsid,sizeof(CLSID));
					}

					datatype = REG_SZ; datasize = 256;
					cr = RegQueryValueEx(hkSub,ASIODRV_DESC,0,&datatype,(LPBYTE)databuf,&datasize);
					if (cr == ERROR_SUCCESS) {
						strcpy(lpdrv->drvname,databuf);
					}
					else strcpy(lpdrv->drvname,keyname);
				}
				RegCloseKey(hksub);
			}
		}
		else fin = TRUE;
	}
	if (hkEnum) RegCloseKey(hkEnum);
#else
	// fixme, these ids change every reboot
	// Firebox {835F8B5B-F546-4881-B4A3-B6B5E8E335EF}
	// Creative ASIO {48653F81-8A2E-11D3-9EF0-00C0F02DD390}
	// SB Audigy 2 ZS ASIO {4C46559D-03A7-467A-8C58-2ABC5B749A1E}
	// SB Audigy 2 ZS ASIO 24/96 {98B6A52A-4FF7-4147-B224-CC256C3C4C64}
	const char * drvdll = "d:\\program files\\presonus\\1394audiodriver_firebox\\ps_asio.dll";
	CLSIDFromString(OLESTR("{48653F81-8A2E-11D3-9EF0-00C0F02DD390}"), &clsid);
#endif
	HRESULT rc = CoCreateInstance(clsid, 0, CLSCTX_INPROC_SERVER, clsid, (LPVOID *)&asiodrv);

	if (rc == S_OK) { // programmers making design decisions = $$$?
		printf("i got this %p\n", asiodrv);
	} else {
		abort(); 
	}
#endif
}

template <typename Input_Buffer_T, typename Output_Buffer_T>
void ASIOProcessor<Input_Buffer_T, Output_Buffer_T>::Destroy()
{
	ASIOError e;
	if (_running)
		Stop();
#if CREATE_BUFFERS
	e = ASIODisposeBuffers();
	free(_buffer_infos);
#endif
	free(_output_channel_infos);
	free(_input_channel_infos);
	e = ASIOExit();
	if (asiodrv) {
		asiodrv->Release();
		asiodrv = 0;
	}
	CoUninitialize();

	pthread_mutex_destroy(&_io_lock);

	for (std::vector<track_t*>::iterator i = _tracks.begin(); i != _tracks.end(); i++)
	{
		delete (*i);
	}
	_tracks.resize(0);
	delete _main_out;
//	delete _aux_bus;
//	delete _cue_bus;
//	delete _master_bus;
//	delete _bus_matrix;

	T_allocator<chunk_t>::dump_leaks();
}
