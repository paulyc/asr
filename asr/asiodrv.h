#ifndef _ASIODRV_H
#define _ASIODRV_H

#include "config.h"



#if WINDOWS
#include <asio.h>
#include <windows.h>


interface IASIO : public IUnknown
{
	virtual ASIOBool init(void *sysHandle) = 0;
	virtual void getDriverName(char *name) = 0;	
	virtual long getDriverVersion() = 0;
	virtual void getErrorMessage(char *string) = 0;	
	virtual ASIOError start() = 0;
	virtual ASIOError stop() = 0;
	virtual ASIOError getChannels(long *numInputChannels, long *numOutputChannels) = 0;
	virtual ASIOError getLatencies(long *inputLatency, long *outputLatency) = 0;
	virtual ASIOError getBufferSize(long *minSize, long *maxSize,
		long *preferredSize, long *granularity) = 0;
	virtual ASIOError canSampleRate(ASIOSampleRate sampleRate) = 0;
	virtual ASIOError getSampleRate(ASIOSampleRate *sampleRate) = 0;
	virtual ASIOError setSampleRate(ASIOSampleRate sampleRate) = 0;
	virtual ASIOError getClockSources(ASIOClockSource *clocks, long *numSources) = 0;
	virtual ASIOError setClockSource(long reference) = 0;
	virtual ASIOError getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) = 0;
	virtual ASIOError getChannelInfo(ASIOChannelInfo *info) = 0;
	virtual ASIOError createBuffers(ASIOBufferInfo *bufferInfos, long numChannels_,
		long bufferSize, ASIOCallbacks *callbacks) = 0;
	virtual ASIOError disposeBuffers() = 0;
	virtual ASIOError controlPanel() = 0;
	virtual ASIOError future(long selector,void *opt) = 0;
	virtual ASIOError outputReady() = 0;
};

class DummyASIO : public IASIO
{
public:
	DummyASIO() : _refCount(1) {}
	virtual ASIOBool init(void *sysHandle) { return ASIOTrue; }
	virtual void getDriverName(char *name) { name = (char*)"Dummy"; }
	virtual long getDriverVersion() { return 0;}
	virtual void getErrorMessage(char *string) { string = (char*)"Dummy"; }
	virtual ASIOError start() { return ASE_OK; }
	virtual ASIOError stop() { return ASE_OK; }
	virtual ASIOError getChannels(long *numInputChannels, long *numOutputChannels) 
	{ 
		*numInputChannels = 0; 
		*numOutputChannels = 0; 
		return ASE_OK; 
	}
	virtual ASIOError getLatencies(long *inputLatency, long *outputLatency) { return ASE_OK; }
	virtual ASIOError getBufferSize(long *minSize, long *maxSize,
		long *preferredSize, long *granularity) { return ASE_OK; }
	virtual ASIOError canSampleRate(ASIOSampleRate sampleRate) { return ASE_OK; }
	virtual ASIOError getSampleRate(ASIOSampleRate *sampleRate) { double d = 48000.0; memcpy(sampleRate, &d, 8); return ASE_OK; }
	virtual ASIOError setSampleRate(ASIOSampleRate sampleRate) { return ASE_OK; }
	virtual ASIOError getClockSources(ASIOClockSource *clocks, long *numSources) { return ASE_OK; }
	virtual ASIOError setClockSource(long reference) { return ASE_OK; }
	virtual ASIOError getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) { return ASE_OK; }
	virtual ASIOError getChannelInfo(ASIOChannelInfo *info) { return ASE_OK; }
	virtual ASIOError createBuffers(ASIOBufferInfo *bufferInfos, long numChannels_,
		long bufferSize, ASIOCallbacks *callbacks) { return ASE_OK; }
	virtual ASIOError disposeBuffers() { return ASE_OK; }
	virtual ASIOError controlPanel() { return ASE_OK; }
	virtual ASIOError future(long selector,void *opt) { return ASE_OK; }
	virtual ASIOError outputReady() { return ASE_OK; }

	HRESULT STDMETHODCALLTYPE QueryInterface(const IID &iid,void **pVoid)
	{
		*pVoid = this;
		return S_OK;
	}
	ULONG STDMETHODCALLTYPE AddRef(void)
	{
		return ++_refCount;
	}
	ULONG STDMETHODCALLTYPE Release(void)
	{
		if (--_refCount == 0)
		{
			delete this;
			return 0;
		}
		return _refCount;
	}
private:
	ULONG _refCount;
};

#endif

#endif
