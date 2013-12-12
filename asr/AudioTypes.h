//
//  AudioTypes.h
//  mac
//
//  Created by Paul Ciarlo on 12/8/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#ifndef mac_AudioTypes_h
#define mac_AudioTypes_h

#include <string>
#include <vector>

#if IOS
#include <CoreAudio/CoreAudioTypes.h>
#include <AudioToolbox/AudioToolbox.h>
#else
#include <CoreAudio/CoreAudio.h>
#endif

class IAudioDeviceFactory;

class IAudioDriverDescriptor
{
public:
	virtual std::string GetName() const = 0;
	virtual IAudioDeviceFactory *Instantiate() const = 0;
};

class IAudioDriverFactory
{
public:
	virtual ~IAudioDriverFactory() {}
	
	virtual std::vector<const IAudioDriverDescriptor*> Enumerate() const = 0;
};

class IAudioDevice;
class IAudioStream;

class IAudioDeviceDescriptor
{
public:
	virtual std::string GetName() const = 0;
	virtual IAudioDevice *Instantiate() const = 0;
	virtual uint32_t GetBufferSizeFrames() const = 0;
};

class IAudioStreamDescriptor
{
public:
	enum StreamType		 { Input, Output };
	enum SampleType		 { SignedInt, UnsignedInt, Float, Unknown };
	enum SampleAlignment { LeastSignificant, MostSignificant };
	
	virtual IAudioDevice*   GetDevice() const = 0;
	virtual StreamType		GetStreamType() const = 0;
	virtual int				GetNumChannels() const = 0;
	virtual SampleType		GetSampleType() const = 0;
	virtual int				GetSampleSizeInBits() const = 0;
	virtual int				GetSampleWordSizeInBytes() const = 0;
	virtual SampleAlignment GetSampleAlignment() const = 0;
	virtual float64_t		GetSampleRate() const = 0;
	
	virtual IAudioStream *GetStream() const = 0;
};

class IAudioBuffer
{
public:
	virtual ~IAudioBuffer() {}
	
	virtual void *GetBuffer() = 0;
	virtual int GetBufferSize() const = 0;
};

class SimpleAudioBuffer : public IAudioBuffer
{
public:
	SimpleAudioBuffer() : _buffer(0) {}
	SimpleAudioBuffer(void *buf) : _buffer(buf) {}
	
	virtual void *GetBuffer() { return _buffer; }
	void SetBuffer(void *buf) { _buffer = buf; }
	virtual int GetBufferSize() const { return _bufferSize; }
	void SetBufferSize(int sz) { _bufferSize = sz; }
protected:
	void *_buffer;
	int _bufferSize;
};

class MultichannelAudioBuffer : public SimpleAudioBuffer
{
public:
	MultichannelAudioBuffer() : SimpleAudioBuffer(0) {}
	MultichannelAudioBuffer(void *buf) : SimpleAudioBuffer(buf) {}
	
	virtual int GetBufferSizeFrames() = 0;
	virtual void *GetChannelBuffer(int ch) = 0;
	virtual int GetStride() = 0;
	virtual int GetNumChannels() = 0;
};

class IAudioStreamProcessor
{
public:
	virtual ~IAudioStreamProcessor() {}
	
	virtual void ProcessInput(IAudioBuffer *buffer) = 0;
	virtual void ProcessOutput(IAudioBuffer *buffer) = 0;
};

class IAudioDeviceFactory
{
public:
	virtual ~IAudioDeviceFactory() {}
	
	virtual std::vector<const IAudioDeviceDescriptor*> Enumerate() const = 0;
};

class IAudioDevice
{
public:
	virtual ~IAudioDevice() {}
	
	virtual const IAudioDeviceDescriptor *GetDescriptor() const = 0;
	virtual std::vector<const IAudioStreamDescriptor*> GetStreams() const = 0;
	virtual void RegisterStream(IAudioStream *stream) = 0;
	
	virtual void Start() = 0;
	virtual void Stop() = 0;
};

class IAudioStream
{
public:
	virtual ~IAudioStream() {}
	
	virtual const IAudioStreamDescriptor *GetDescriptor() const = 0;
	virtual void SetProc(IAudioStreamProcessor *proc) = 0;
	
	virtual void Start() = 0;
	virtual void Stop() = 0;
};

#endif
