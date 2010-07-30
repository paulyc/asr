#undef MAC 
#define PPC 0
#define WINDOWS 1
#define SGI 0
#define SUN 0
#define LINUX 0
#define BEOS 0

#if WINDOWS
#include <windows.h>
#endif

#define NATIVE_INT64 0
#define IEEE754_64FLOAT 1

#include <asio.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <string>
#include <exception>
#include <iostream>
#include <fstream>
using std::exception;
#include <objbase.h>
#include <fftw3.h>
#include <emmintrin.h>

#define CHANNELS 2
#define BUFFERSIZE 0x960
#define SAMPLERATE 48000.0f

#include "asr.h"
#include "util.h"

#include "fftw.cpp"

#include "wavfile.h"

typedef chunk_T_T_size<int, 2*BUFFERSIZE> my_chunk;
typedef T_allocator<my_chunk> my_allocator;
typedef T_source<my_chunk> my_source;
typedef T_sink<my_chunk> my_sink;

#define OLD_PATH 1
#define TEST 0

#define NON_SSE_INTS 1

#if OLD_PATH
#define C_LOOPS 1
#define USE_SSE2 0
#else
#define C_LOOPS 0
#define USE_SSE2 1
#endif
#if C_LOOPS && USE_SSE2
#define USE_SSE2 0
#endif

#if !USE_SSE2 && !NON_SSE_INTS
typedef int_N_wavfile_chunker_T<short, BUFFERSIZE> my_wavfile_chunker;
#else
typedef int_N_wavfile_chunker_T<int, BUFFERSIZE> my_wavfile_chunker;
#endif

#include "asiodrv.cpp"

#include "speedparser.h"

void bufferSwitch(long doubleBufferIndex, ASIOBool directProcess);
ASIOTime* bufferSwitchTimeInfo(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess);
void sampleRateDidChange(ASIOSampleRate sRate);
long asioMessage(long selector, long value, void *message, double *opt);

void copy_chunk_buffer()
{
}

void buf_copy(char *buf_src, char *buf_dst, int num_items, int item_sz=1, int stride=0)
{
	char *src_end = buf_src + num_items*(item_sz+stride), *item_end;
	while (buf_src != src_end) {
		item_end = buf_src+item_sz;
		while (buf_src != item_end) {
			*buf_dst++ = *buf_src++;
		}
		buf_src += stride;
	}
}
class ASIOThinger;
template <typename T>
class asio_sink : public T_sink<T>
{
public:
	asio_sink(ASIOThinger *thinger) :
		_thinger(thinger)
	{
	}
	void next(T* t)
	{
		// assume 2 channels non-interleaved same data format
		memcpy(thinger->_buffer_infos[0].buffers[thinger->_doubleBufferIndex], t->_data, sizeof(t->_data)/2);
		memcpy(thinger->_buffer_infos[1].buffers[thinger->_doubleBufferIndex], (char*)(t->_data)+sizeof(t->_data)/2, sizeof(t->_data)/2);
	}
protected:
	//ASIOBufferInfo *_buffer_infos;
	ASIOThinger *thinger;
};



template <typename T>
class asio_96_24_sink : public asio_sink<T>
{
public:
	void next(T* t)
	{
	}
};

#define CREATE_BUFFERS 1
#define CHOOSE_CHANNELS 0
#define RUN 0

#define ASIO_ASSERT_NO_ERROR(call) { \
	e = call ; \
	if (e != ASE_OK) { \
		printf("Yo error %s\n", asio_error_str(e)); \
		assert(false); \
	} \
}

class ASIOThinger
{
public:
	ASIOThinger() :
		_running(false),
		_speed(1.0),
		_default_src(L"G:\\My Music\\Sean Tyas - Melbourne (Original Mix).wav"),
		//_default_src(L"H:\\Music\\Super8 & Tab, Anton Sonin - Black Is The New Yellow (Activa Remix).wav"),
		_buffer_infos(0),
		_input_channel_infos(0),
		_output_channel_infos(0),
		_outputTime(0.0),
		_src1(0),
		_src1_active(false),
		_log("mylog.txt")
	{
		Init();
	}
	virtual ~ASIOThinger()
	{
		Destroy();
	}

	long _doubleBufferIndex;
	bool _running;
	double _speed;
	const wchar_t *_default_src;

	ASIOError Start()
	{
		_running = true;
		_src1_active = true;
		return ASIOStart();
	}

	ASIOError Stop()
	{
		_running = false;
		_src1_active = false;
		return ASIOStop();
	}
	
	template <typename T>
	void BufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
	{
		/*int *bufL, *bufR;
		double dt = 1.0 / 96000.0;
		int val;
		//output
		bufL = (int*)_buffer_infos[2].buffers[doubleBufferIndex];
		bufR = (int*)_buffer_infos[3].buffers[doubleBufferIndex];
		for (int i=0; i<_bufSize; ++i){
			val = (int)(INT_MAX * sin(2.0*M_PI*440.0*_outputTime));
			bufL[i] = val;
			bufR[i] = val;
			_outputTime += dt;
		}*/
		// process input
		//my_chunk *chk = my_allocator::alloc();
		//memcpy(chk->_data, 
		//printf("BufferSwitch\n");
		_doubleBufferIndex = doubleBufferIndex;
		
		this->ProcessInput();
		//process output
		// play soundfile
		//_src1.ld_data((short*)_buffer_infos[2].buffers[doubleBufferIndex], (short*)_buffer_infos[3].buffers[doubleBufferIndex]);
		//float outputrate = SAMPLERATE * (float)sp;
#define ECHO 0
#if !ECHO
		double sp = _speed * (48000.0f / 44100.0f);
		float resamplerate = 44100.0f  * sp;
		SetOutputSamplingFrequency(resamplerate);
		short *bufOut, *bufEnd;
		
		bufOut = (short*)_buffer_infos[2].buffers[doubleBufferIndex];
		bufEnd = bufOut + BUFFERSIZE;
		float smpOut;
		float myPeriod = 1.0f / resamplerate;
		CheckBuffers(resamplerate);
		double tt = _outputTime;
		while (bufOut < bufEnd)
		{
			EvalSignalN<1>(_outputTime, &smpOut, _inputBufTime, _bufLBegin, _bufLEnd - _bufLBegin);
			if (smpOut < 0.0f)
				*bufOut++ = short(smpOut * -SHRT_MIN);
			else
				*bufOut++ = short(smpOut * SHRT_MAX);
			_outputTime += myPeriod;
		}
		bufOut = (short*)_buffer_infos[3].buffers[doubleBufferIndex];
		bufEnd = bufOut + BUFFERSIZE;
		_outputTime = tt;
		while (bufOut < bufEnd)
		{
			EvalSignalN<1>(_outputTime, &smpOut, _inputBufTime, _bufRBegin, _bufREnd - _bufRBegin);
			if (smpOut < 0.0f)
				*bufOut++ = short(smpOut * -SHRT_MIN);
			else
				*bufOut++ = short(smpOut * SHRT_MAX);
			_outputTime += myPeriod;
		}
#else
		// copy input
		memcpy(_buffer_infos[2].buffers[doubleBufferIndex], _buffer_infos[0].buffers[doubleBufferIndex], BUFFERSIZE*sizeof(short));
		memcpy(_buffer_infos[3].buffers[doubleBufferIndex], _buffer_infos[1].buffers[doubleBufferIndex], BUFFERSIZE*sizeof(short));
#endif
	}
#define INPUT_PERIOD (1.0/44100.0)

#if !USE_SSE2 && !NON_SSE_INTS
	static float * LoadHelp(float *toBuf, short *fromBuf)
#else
	static float * LoadHelp(float *toBuf, int *fromBuf)
#endif
	{
#ifdef _DEBUG
		printf("ASIOThinger::LoadHelp(%p, %p)\n", toBuf, fromBuf);
#endif
#if C_LOOPS 
#if !NON_SSE_INTS
		short val;
		for (short *p=fromBuf; p<fromBuf+BUFFERSIZE; ++p) {
			val = *p;
			if (val >= 0)
				*toBuf++ = val / float(SHRT_MAX);
			else
				*toBuf++ = val / float(-SHRT_MIN);
		}
#else
		int val;
		for (int *p=fromBuf; p<fromBuf+BUFFERSIZE; ++p) {
			val = *p;
			if (val >= 0)
				*toBuf++ = val / float(INT_MAX);
			else
				*toBuf++ = val / float(-INT_MIN);
		}
#endif
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

	void CheckBuffers(float resamplerate)
	{
#ifdef _DEBUG
		printf("ASIOThinger::CheckBuffers(%p, %f)\n", this, resamplerate);
#endif
		double endInputTime = _inputBufTime + (_bufLEnd - _bufLBegin) / 44100.0;
		double endResampleTime = _outputTime + (BUFFERSIZE+15) / resamplerate;
		if (endResampleTime > endInputTime) {
			if (_outputTime < endInputTime) {
				int offset = int((_outputTime - _inputBufTime)*44100.0);
				float * moveFrom = _inputBufL + offset - 20;
			//	assert(moveFrom >= _inputBufL && moveFrom < _inputBufL + inputBuffersize);
				if (moveFrom > _inputBufL)
				{
					int sz = _bufLEnd - moveFrom;
					if (sz > 0)
					{
						while (sz%4)
						{
							if (moveFrom > _inputBufL)
							{
								--moveFrom;
								++sz;
							}
							else
							{
								++sz;
							}
						}
						assert(moveFrom >= _inputBufL && moveFrom + sz < (float*)((char*)_inputBufL + _inputBufLEffectiveSize));
						memmove(_inputBufL, moveFrom, sz * sizeof(float));
						_inputBufTime += (_bufLEnd - _bufLBegin - sz - 20) / 44100.0;
						_bufLBegin = _inputBufL;
						_bufLEnd = _bufLBegin + sz;
						
					}
				}
			
			
				moveFrom = _inputBufR + offset - 20;
			//	assert(moveFrom >= _inputBufR && moveFrom < _inputBufR + inputBuffersize);
				if (moveFrom > _inputBufR)
				{
					int sz = _bufREnd - moveFrom;
					if (sz > 0)
					{
						while (sz%4)
						{
							if (moveFrom > _inputBufR)
							{
								--moveFrom;
								++sz;
							}
							else
							{
								++sz;
							}
						}
						assert(moveFrom >= _inputBufR && moveFrom + sz < (float*)((char*)_inputBufR + _inputBufREffectiveSize));
						memmove(_inputBufR, moveFrom, sz * sizeof(float));
						_bufRBegin = _inputBufR;
						_bufREnd = _bufRBegin + sz;
					}
				}
			} else {
				_inputBufTime += (_bufLEnd - _bufLBegin) / 44100.0;
				_bufLEnd = _bufLBegin = _inputBufL;
				_bufREnd = _bufRBegin = _inputBufR;
			}

			// then load new data
#if !USE_SSE2 && !NON_SSE_INTS
			short *tmpL = T_allocator_N_16ba<short, BUFFERSIZE>::alloc(), 
				  *tmpR = T_allocator_N_16ba<short, BUFFERSIZE>::alloc(), 
				  val;
#else
			int *tmpL = T_allocator_N_16ba<int, BUFFERSIZE>::alloc(), 
				*tmpR = T_allocator_N_16ba<int, BUFFERSIZE>::alloc(), 
				val;
#endif
			_src1->ld_data(tmpL, tmpR);
			_bufLEnd = LoadHelp(_bufLEnd, tmpL);
			_bufREnd = LoadHelp(_bufREnd, tmpR);

			// do the same thing to next data
			_src1->ld_data(tmpL, tmpR);
			_bufLEnd = LoadHelp(_bufLEnd, tmpL);
			_bufREnd = LoadHelp(_bufREnd, tmpR);

#if !USE_SSE2 && !NON_SSE_INTS
			T_allocator_N_16ba<short, BUFFERSIZE>::free(tmpL);
			T_allocator_N_16ba<short, BUFFERSIZE>::free(tmpR);
#else
			T_allocator_N_16ba<int, BUFFERSIZE>::free(tmpL);
			T_allocator_N_16ba<int, BUFFERSIZE>::free(tmpR);
#endif
		}
	}

	void SetSrc(int ch, wchar_t *fqpath)
	{
		bool was_active = _src1_active;
		if (was_active)
			Stop();
		delete _src1;
		_src1 = 0;
		try {
			_src1 = new my_wavfile_chunker(fqpath);
		} catch (exception e) {
		}
		if (was_active && _src1)
			Start();
	}

protected:
	const static int inputBuffersize = 4*BUFFERSIZE;
	// move below
	float *_inputBufL, *_inputBufR;
	size_t _inputBufLEffectiveSize, _inputBufREffectiveSize;
	float *_bufLBegin, *_bufLEnd, *_bufRBegin, *_bufREnd;
	double _inputBufTime;
	
	void Init()
	{
		ASIOError e;
		long minSize, maxSize, preferredSize, granularity;

		try {
			_src1 = new my_wavfile_chunker(_default_src);
		} catch (exception e) {
			throw e;
		}

		CoInitialize(0);
		LoadDriver();
		//return;
		_inputBufTime = -13*INPUT_PERIOD;

		T_allocator_N_16ba<float, inputBuffersize>::alloc_info info;
		T_allocator_N_16ba<float, inputBuffersize>::alloc(&info);
		_inputBufL = info.ptr;
		_inputBufLEffectiveSize = info.sz;
		T_allocator_N_16ba<float, inputBuffersize>::alloc(&info);
		_inputBufR = info.ptr;
		_inputBufREffectiveSize = info.sz;
		_bufLBegin = _inputBufL;
		_bufRBegin = _inputBufR;
		_bufLEnd = _bufLBegin;
		_bufREnd = _bufRBegin;
		while (_bufLEnd < _bufLBegin + 20)
		{
			*_bufLEnd++ = 0.0f;
			*_bufREnd++ = 0.0f;
		}

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
		ASIOSampleRate r;
/*		ASIOSampleType t;
		r = 44100.0;
		e = ASIOCanSampleRate(r); */
		ASIO_ASSERT_NO_ERROR(ASIOGetSampleRate(&r));
		printf("At a sampling rate of %f\n", r);
	//	ASIOSetSampleRate(44100.0);
	}
	void LoadDriver()
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
	void ProcessInput()
	{
		float *myBuf = _sp._inBuf;
		short val;
		for (int i=0; i<_bufSize; ++i){
			val = ((short*)_buffer_infos[0].buffers[_doubleBufferIndex])[i];
			if (val >= 0)
				myBuf[i*2] = val / float(SHRT_MAX);
			else
				myBuf[i*2] = val / float(-SHRT_MIN);
			val = ((short*)_buffer_infos[1].buffers[_doubleBufferIndex])[i];
			if (val >= 0)
				myBuf[i*2+1] = val / float(SHRT_MAX);
			else		
				myBuf[i*2+1] = val / float(-SHRT_MIN);
		}
		_speed = _sp.Execute();
		//_log << _speed << "\n";
		//if (_speed < .9 || _speed > 1.1)
			_speed = 1.0;
	}
	void Destroy()
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

		delete _src1;

		T_allocator_N_16ba<float, inputBuffersize>::free(_inputBufL);
		T_allocator_N_16ba<float, inputBuffersize>::free(_inputBufR);
	}
	
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
	my_wavfile_chunker *_src1;
	bool _src1_active;
	SpeedParser2<BUFFERSIZE> _sp;
	std::ofstream _log;
};
const int ASIOThinger::inputBuffersize;

ASIOThinger * asio = NULL;

/*
Purpose:
bufferSwitchTimeInfo indicates that both input and output are to be processed. It also passes
extended time information (time code for synchronization purposes) to the host application and
back to the driver.
*/
void bufferSwitch(long doubleBufferIndex, ASIOBool directProcess)
{
	asio->BufferSwitch<short>(doubleBufferIndex, directProcess);
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

void begin()
{
	asio = new ASIOThinger;
#if RUN
	ASIOStart();
#endif
}

void end()
{
#if RUN
	ASIOStop();
#endif
	delete asio;
}

#if WINDOWS
#include <commctrl.h>
#include "resource.h"
#define SLEEP 0

HWND g_dlg = NULL;

INT_PTR CALLBACK MyDialogProc(HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
	switch (uMsg)
	{
		case WM_HSCROLL:
			switch (LOWORD(wParam))
			{
				case SB_THUMBPOSITION:
				case SB_THUMBTRACK:
					printf("%d\n", HIWORD(wParam));
					break;
			}
			return TRUE;
		case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
				
                case IDOK:
					DestroyWindow(hwndDlg);
					g_dlg = NULL;
					PostMessage(NULL, WM_QUIT, 0, 0);
					return TRUE;
				case IDC_BUTTON1:
					asio->Start();
					return TRUE;
				case IDC_BUTTON2:
					asio->Stop();
					return TRUE;
				case IDC_BUTTON3: // load src 1
				{
					TCHAR filepath[256] = {0};
					TCHAR filetitle[256];
					OPENFILENAME ofn = { //typedef struct tagOFNW {
						sizeof(ofn), // DWORD        lStructSize;
						hwndDlg, //   HWND         hwndOwner;
						NULL, //   HINSTANCE    hInstance;
						TEXT("Sound Files\0*.WAV;*.MP3;*.FLAC;*.AAC;*.MP4;*.AC3\0"
						TEXT("All Files\0*.*")), //   LPCWSTR      lpstrFilter;
						NULL, //LPWSTR       lpstrCustomFilter;
						0, //   DWORD        nMaxCustFilter;
						1, //   DWORD        nFilterIndex;
						filepath, //   LPWSTR       lpstrFile;
						sizeof(filepath), //   DWORD        nMaxFile;
						filetitle, //   LPWSTR       lpstrFileTitle;
						sizeof(filetitle), //   DWORD        nMaxFileTitle;
						NULL, //   LPCWSTR      lpstrInitialDir;
						NULL, //   LPCWSTR      lpstrTitle;
						OFN_FILEMUSTEXIST, //   DWORD        Flags;
						0, //   WORD         nFileOffset;
						0, //   WORD         nFileExtension;
						NULL, //   LPCWSTR      lpstrDefExt;
						NULL, //   LPARAM       lCustData;
						NULL, //   LPOFNHOOKPROC lpfnHook;
						NULL, //   LPCWSTR      lpTemplateName;
						//#ifdef _MAC
						//   LPEDITMENU   lpEditInfo;
						//   LPCSTR       lpstrPrompt;
						//#endif
						//#if (_WIN32_WINNT >= 0x0500)
						NULL, //   void *        pvReserved;
						0, //   DWORD        dwReserved;
						0, //   DWORD        FlagsEx;
						//#endif // (_WIN32_WINNT >= 0x0500)
					};
					//memset(&ofn, 0, sizeof(ofn));
					if (GetOpenFileName(&ofn)) {
						SetDlgItemText(hwndDlg, IDC_EDIT1, filepath);
						asio->SetSrc(1, filepath);
					}
					return TRUE;
				}
			}
	}
	return FALSE;
}

void CreateUI()
{
	INITCOMMONCONTROLSEX iccx;
	iccx.dwSize = sizeof(iccx);
	iccx.dwICC = ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&iccx);
	g_dlg = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), NULL, MyDialogProc);
	if (!g_dlg) {
		//TCHAR foo[256];
		//FormatMessage(dwFlags, lpSource, dwMessageId, dwLanguageId, foo, sizeof(foo), Arguments);
		//printf("Yo error %s\n", foo);
		printf("Yo error %d\n", GetLastError());
		return;
	}
	SetDlgItemText(g_dlg, IDC_EDIT1, asio->_default_src);
	ShowWindow(g_dlg, SW_SHOW);
}

void MainLoop_Win32()
{
	CreateUI();

#if SLEEP
	Sleep(10000);
#else
	BOOL bRet;
	MSG msg;

	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) 
	{ 
		if (bRet == -1)
		{
			// Handle the error and possibly exit
			break;
		}
		else if (!IsWindow(g_dlg) || !IsDialogMessage(g_dlg, &msg)) 
		{ 
			TranslateMessage(&msg); 
			DispatchMessage(&msg); 
		} 
	} 

#endif // !SLEEP
}
#endif // WINDOWS

void testf()
{
#if 0
	char buf[128];
	char *f1 = buf;
	while ((int)f1 & 0xFF) ++f1;
	int *foo = (int*)f1, bar;
	foo[0] = 1;
	foo[1] = 2;
	foo[2] = 3;
	foo[3] = 4;
	foo[4] = 5;
	foo[5] = 6;
	foo[6] = 7;
	foo[7] = 8;

	__m128i a, b, *c;
	a.m128i_i32[0] = 1;
	a.m128i_i32[1] = 2;
	a.m128i_i32[2] = 3;
	a.m128i_i32[3] = 4;
	b = *(__m128i*)foo;
	1;
	__asm
	{
		;ldmxcsr 0x1f80
		movdqa xmm1, b

	}
	bar = 10;
	//c = (__m128i*)foo;
	//a = _mm_load_si128(c);
	//__asm
	//{
	//	movdqa xmm1, [foo]
	//}
	bar = 50;
	return;
#else
	FILE *output = fopen("foo.txt", "w");
#if !USE_SSE2 && !NON_SSE_INTS
		short *tmp = T_allocator_N_16ba<short, BUFFERSIZE>::alloc();
#else
		int *tmp = T_allocator_N_16ba<int, BUFFERSIZE>::alloc();
#endif
		float *buffer = T_allocator_N_16ba<float, BUFFERSIZE>::alloc(), *end;
		for (int i=0;i<BUFFERSIZE;++i)
		{
			tmp[i] = BUFFERSIZE/2 - i;
			fprintf(output, "%d ", tmp[i]);
		}
		fprintf(output, "\ncalling ASIOThinger::LoadHelp\n");
		end = ASIOThinger::LoadHelp(buffer, tmp);
		for (int i=0;i<BUFFERSIZE;++i)
		{
			fprintf(output, "%f ", buffer[i]);
		}
		fprintf(output, "\n%p %p %x\n", buffer, end, end-buffer);
	fclose(output);
#endif
}

int main()
{
	//chunk_T_T_size<int, 128> sthing;
	//printf("%d\n", sizeof(sthing._data));
	//printf("%x\n", _WIN32_WINNT);
#if TEST
	try{
		testf();
	}catch(exception&e){
		throw e;
	}
	return 0;
#endif
	init();
	begin();

#if WINDOWS
	MainLoop_Win32();
#endif

	end();
	return 0;
}
