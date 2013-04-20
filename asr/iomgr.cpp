#include "iomgr.h"

extern IASIO *asiodrv;

std::vector<IODriver*> IODriver::enumerate()
{
	std::vector<IODriver*> drivers(1);
	drivers[0] = new ASIODriver(L"ASIO");
	return drivers;
}

IOManager<chunk_t>* ASIODriver::loadDriver()
{
#if WINDOWS
#define ASIODRV_DESC		L"description"
#define INPROC_SERVER		L"InprocServer32"
#define ASIO_PATH			L"software\\asio"
#define COM_CLSID			L"clsid"
#define MAXPATHLEN			512
#define MAXDRVNAMELEN		128

	CLSID clsid;// = {0x835F8B5B, 0xF546, 0x4881, 0xB4, 0xA3, 0xB6, 0xB5, 0xE8, 0xE3, 0x35, 0xEF};
#if 0
	HKEY hkEnum, hkSub;
	TCHAR keyname[MAXDRVNAMELEN];
	DWORD index = 0;

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
	//const char * drvdll = "d:\\program files\\presonus\\1394audiodriver_firebox\\ps_asio.dll";
#if !DUMMY_ASIO
#if !PARALLELS_ASIO
	CLSIDFromString(OLESTR("{23638883-0B8C-4324-BBD5-35C3CF1B73BF}"), &clsid);
#else
	CLSIDFromString(OLESTR("{232685C6-6548-49D8-846D-4141A3EF7560}"), &clsid);
#endif // !PARALLELS_ASIO
#endif // !DUMMY_ASIO
#endif

#if DUMMY_ASIO
	asiodrv = new DummyASIO;
	HRESULT rc = S_OK;
#else
	HRESULT rc = CoCreateInstance(clsid, 0, CLSCTX_INPROC_SERVER, clsid, (LPVOID *)&asiodrv);
#endif

	if (rc == S_OK) { // programmers making design decisions = $$$?
		printf("i got this %p\n", asiodrv);
	} else {
		abort(); 
	}

	return new ASIOManager<chunk_t>(asiodrv);
#endif
}

template <typename Chunk_T>
ASIOManager<Chunk_T>::ASIOManager(IASIO *drv) : 
	_drv(drv), 
	_buffer_infos(0),
	_input_channel_infos(0),
	_output_channel_infos(0)
{
	ASIOError e;

	_drv_info.asioVersion = 2;
	_drv_info.sysRef = 0;
	ASIO_ASSERT_NO_ERROR(ASIOInit(&_drv_info));

	enumerateIOs();
}

template <typename Chunk_T>
ASIOManager<Chunk_T>::~ASIOManager()
{
	ASIOError e = ASIODisposeBuffers();
	e = ASIOExit();
	free(_buffer_infos);
	free(_output_channel_infos);
	free(_input_channel_infos);


	for (std::list<IOInput*>::iterator i = _inputs.begin();
		i != _inputs.end();
		i++)
	{
		delete *i;
	}
	for (std::list<IOOutput*>::iterator i = _outputs.begin();
		i != _outputs.end();
		i++)
	{
		delete *i;
	}
}

template <typename Chunk_T>
void ASIOManager<Chunk_T>::enumerateIOs()
{
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

	ASIOError e;
	long minSize, maxSize, granularity;
	
	ASIO_ASSERT_NO_ERROR(ASIOGetBufferSize(&minSize, &maxSize, &_preferredSize, &granularity));
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
}

template <typename Chunk_T>
void ASIOManager<Chunk_T>::createBuffers(ASIOProcessor *io)
{
	ASIOError e;

	int ch_input_l, ch_input_r, ch_input2_l, ch_input2_r, ch_output1_l, ch_output1_r, ch_output2_l, ch_output2_r;
#if CHOOSE_CHANNELS
	printf("choose 4 channels (in L, in R, out L, out R) or -1 to quit, maybe: ");
	std::cin >> ch_input_l;
	std::cin >> ch_input_r >> ch_output_l >> ch_output_r;
#else
#if !PARALLELS_ASIO
	// mic/line in 2
	ch_input_l = 2;
	ch_input_r = 3;
	// aux 2
	ch_input2_l = 0;
	ch_input2_r = 1;
	// aux
	//ch_input_l = 18;
	//ch_input_r = 19;
	//ch_input_l = 0;
	//ch_input_r = 1;
	ch_output1_l = 2;
	ch_output1_r = 3;
	ch_output2_l = 4;
	ch_output2_r = 5;
	_buffer_infos_len = 8;
	_buffer_infos = (ASIOBufferInfo*)malloc(_buffer_infos_len*sizeof(ASIOBufferInfo));
	_buffer_infos[0].isInput = ASIOTrue;
	//_buffer_infos[0].channelNum = 2;
	_buffer_infos[0].channelNum = ch_input_l;
	_buffer_infos[1].isInput = ASIOTrue;
	//_buffer_infos[1].channelNum = 3;
	_buffer_infos[1].channelNum = ch_input_r;
	_buffer_infos[2].isInput = ASIOFalse;
	_buffer_infos[2].channelNum = ch_output1_l;
	_buffer_infos[3].isInput = ASIOFalse;
	_buffer_infos[3].channelNum = ch_output1_r;
	_buffer_infos[4].isInput = ASIOFalse;
	_buffer_infos[4].channelNum = ch_output2_l;
	_buffer_infos[5].isInput = ASIOFalse;
	_buffer_infos[5].channelNum = ch_output2_r;
	_buffer_infos[6].isInput = ASIOTrue;
	_buffer_infos[6].channelNum = ch_input2_l;
	_buffer_infos[7].isInput = ASIOTrue;
	_buffer_infos[7].channelNum = ch_input2_r;
#else
	ch_input_l = 0;
	ch_input_r = 0;
	ch_output1_l = 0;
	ch_output1_r = 1;
	ch_output2_l = 2;
	ch_output2_r = 3;
	_buffer_infos_len = 6;
	_buffer_infos = (ASIOBufferInfo*)malloc(_buffer_infos_len*sizeof(ASIOBufferInfo));
	_buffer_infos[0].isInput = ASIOTrue;
	//_buffer_infos[0].channelNum = 2;
	_buffer_infos[0].channelNum = ch_input_l;
	_buffer_infos[1].isInput = ASIOTrue;
	//_buffer_infos[1].channelNum = 3;
	_buffer_infos[1].channelNum = ch_input_r;
	_buffer_infos[2].isInput = ASIOFalse;
	_buffer_infos[2].channelNum = ch_output1_l;
	_buffer_infos[3].isInput = ASIOFalse;
	_buffer_infos[3].channelNum = ch_output1_r;
	_buffer_infos[4].isInput = ASIOFalse;
	_buffer_infos[4].channelNum = ch_output2_l;
	_buffer_infos[5].isInput = ASIOFalse;
	_buffer_infos[5].channelNum = ch_output2_r;

	ASIOSetSampleRate(48000.0);
#endif
#endif

	MyASIOCallbacks::io = io;
	_cb.bufferSwitch = MyASIOCallbacks::bufferSwitch;
	_cb.bufferSwitchTimeInfo = MyASIOCallbacks::bufferSwitchTimeInfo;
	_cb.sampleRateDidChange = MyASIOCallbacks::sampleRateDidChange;
	_cb.asioMessage = MyASIOCallbacks::asioMessage;
//	_bufSize = _preferredSize;
	_bufSize = 1024;
	ASIO_ASSERT_NO_ERROR(ASIOCreateBuffers(_buffer_infos, _buffer_infos_len, _bufSize, &_cb));

	ASIOSampleRate r;
/*		ASIOSampleType t;
	r = 44100.0;
	e = ASIOCanSampleRate(r); */
	ASIO_ASSERT_NO_ERROR(ASIOGetSampleRate(&r));
	printf("At a sampling rate of %f buffer size %d\n", r, _bufSize);
//	ASIOSetSampleRate(44100.0);

//	TRACE_FUNCTION(ASIOProcessor::BufferSwitch);
//	TRACE_FUNCTION(ASIOProcessor::GenerateOutput);
		/*__asm {
		push 0;offset string "ASIOProcessor::BufferSwitch"
		mov eax, ASIOProcessor::BufferSwitch
		push eax
		call Tracer::hook
		add esp, 8
		push 0;offset string "ASIOProcessor::GenerateOutput"
		mov eax, ASIOProcessor::GenerateOutput
		push eax
		call Tracer::hook
		add esp, 8
	;	push 0
	;	mov eax, lowpass_filter_td<chunk_t, double>::next
	;;	push eax
	;	call Tracer::hook
	;	add esp, 8
	}*/
}

template <typename Chunk_T>
typename ASIOManager<Chunk_T>::sample_t** ASIOManager<Chunk_T>::getBuffers(int index) const
{
	return (sample_t**)_buffer_infos[index].buffers;
}

template <typename Chunk_T>
void ASIOManager<Chunk_T>::processOutputs()
{
	for (std::list<IOOutput*>::iterator i = _outputs.begin();
		i != _outputs.end();
		i++)
	{
		(*i)->process();
	}
}

template <typename Chunk_T>
void ASIOManager<Chunk_T>::processInputs(long doubleBufferIndex)
{
	for (std::list<IOInput*>::iterator i = _inputs.begin();
		i != _inputs.end();
		i++)
	{
		(*i)->process(doubleBufferIndex);
	}
}

template <typename Chunk_T>
void ASIOManager<Chunk_T>::switchBuffers(long doubleBufferIndex)
{
	for (std::list<IOOutput*>::iterator i = _outputs.begin();
		i != _outputs.end();
		i++)
	{
		(*i)->switchBuffers(doubleBufferIndex);
	}
}

template <typename Chunk_T>
void ASIOManager<Chunk_T>::addInput(IOInput *input)
{
	_inputs.push_back(input);
}

template <typename Chunk_T>
void ASIOManager<Chunk_T>::addOutput(IOOutput *output)
{
	_outputs.push_back(output);
}