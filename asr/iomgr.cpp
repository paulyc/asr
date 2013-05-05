#include "iomgr.h"

#if WINDOWS

extern IASIO *asiodrv;

std::vector<IODriver*> IODriver::enumerate()
{
	std::vector<IODriver*> drivers(1);
	drivers[0] = new ASIODriver(L"ASIO");
	return drivers;
}

IOManager<chunk_t>* ASIODriver::loadDriver()
{
    //return new ASIOManager<chunk_t>(asiodrv);
}

template <typename Chunk_T>
ASIOManager<Chunk_T>::ASIOManager(IASIO *drv) : 
	_drv(drv), 
	_buffer_infos(0)
{

	enumerateIOs();
}

template <typename Chunk_T>
ASIOManager<Chunk_T>::~ASIOManager()
{
	ASIOStop();
	ASIOError e = ASIODisposeBuffers();
	e = ASIOExit();
	free(_buffer_infos);

	for (std::list<AudioInput*>::iterator i = _inputs.begin();
		i != _inputs.end();
		i++)
	{
		delete *i;
	}
	for (std::list<AudioOutput*>::iterator i = _outputs.begin();
		i != _outputs.end();
		i++)
	{
		delete *i;
	}
}

template <typename Chunk_T>
void ASIOManager<Chunk_T>::enumerateIOs()
{
	
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
#endif
#endif

	MyASIOCallbacks::io = io;
	_cb.bufferSwitch = MyASIOCallbacks::bufferSwitch;
	_cb.bufferSwitchTimeInfo = MyASIOCallbacks::bufferSwitchTimeInfo;
	_cb.sampleRateDidChange = MyASIOCallbacks::sampleRateDidChange;
	_cb.asioMessage = MyASIOCallbacks::asioMessage;
	_bufSize = CHUNK_SIZE;
//	_bufSize = 1024;
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
void ASIOManager<Chunk_T>::addInput(AudioInput *input)
{
	_inputs.push_back(input);
}

template <typename Chunk_T>
void ASIOManager<Chunk_T>::addOutput(AudioOutput *output)
{
	_outputs.push_back(output);
}

#endif // WINDOWS
