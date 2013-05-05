//
//  AudioDevice.h
//  mac
//
//  Created by Paul Ciarlo on 5/1/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#ifndef __mac__AudioDevice__
#define __mac__AudioDevice__

#include <string>
#include <vector>

#include "config.h"
#if MAC
#include <CoreAudio/CoreAudio.h>
#endif

#include "type.h"

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

class AudioInput
{
public:
	virtual ~AudioInput() {}
    
	virtual void process(int doubleBufferIndex) = 0;
};

class AudioOutput
{
public:
	virtual ~AudioOutput() {}
    
	virtual void process() = 0;
	virtual void switchBuffers(int dbIndex) = 0;
};

class IAudioDevice;
class IAudioStream;

class IAudioDeviceDescriptor
{
public:
    virtual std::string GetName() const = 0;
    virtual IAudioDevice *Instantiate() const = 0;
};

struct AudioDeviceDescriptor
{
    AudioDeviceDescriptor(std::string strName) : name(strName) {}
    AudioDeviceDescriptor(const char * cStrName) : name(cStrName) {}
    
    std::string name;
};

class IAudioStreamDescriptor
{
public:
    enum StreamType      { Input, Output };
    enum SampleType      { SignedInt, UnsignedInt, Float };
    enum SampleAlignment { LeastSignifcant, MostSignificant };
    
    virtual StreamType      GetStreamType() const = 0;
    virtual int             GetNumChannels() const = 0;
    virtual SampleType      GetSampleType() const = 0;
    virtual int             GetSampleSizeInBits() const = 0;
    virtual int             GetSampleWordSizeInBytes() const = 0;
    virtual SampleAlignment GetSampleAlignment() const = 0;
    virtual float64_t       GetSampleRate() const = 0;
    
    virtual IAudioStream *GetStream() const = 0;
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
    
    virtual void Start() = 0;
    virtual void Stop() = 0;
};

class IAudioBuffer
{
public:
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
private:
    void *_buffer;
    int _bufferSize;
};

class IAudioStreamProcessor
{
public:
    virtual void Process(IAudioBuffer *buffer) = 0;
};

class SineAudioOutputProcessor : public IAudioStreamProcessor
{
public:
    SineAudioOutputProcessor(float32_t frequency, const IAudioStreamDescriptor *streamDesc);
    
    virtual void Process(IAudioBuffer *buffer);
private:
    float32_t _frequency;
    float64_t _time;
    float64_t _sampleRate;
    int _frameSize;
};

class IAudioStream
{
public:
    virtual ~IAudioStream() {}
    
    virtual const IAudioStreamDescriptor *GetDescriptor() const = 0;
    virtual void SetProc(IAudioStreamProcessor *proc) = 0;
};

#if MAC
class IAudioDeviceFactory;

class MacAudioDriverDescriptor : public IAudioDriverDescriptor
{
public:
    MacAudioDriverDescriptor(const char *cStrName) : _name(cStrName) {}
    
    virtual std::string GetName() const { return _name; }
    virtual IAudioDeviceFactory *Instantiate() const;
private:
    std::string _name;
};

class MacAudioDriverFactory : public IAudioDriverFactory
{
public:
    MacAudioDriverFactory();
    
    virtual std::vector<const IAudioDriverDescriptor*> Enumerate() const;
private:
    std::vector<MacAudioDriverDescriptor> _drivers;
};

class CoreAudioDeviceDescriptor : public IAudioDeviceDescriptor
{
public:
    CoreAudioDeviceDescriptor(const char *cStrName, AudioDeviceID devId) :
        _name(cStrName),
        _deviceID(devId)
    {
    }
    
    virtual std::string GetName() const { return _name; }
    virtual IAudioDevice *Instantiate() const;
    
    AudioDeviceID GetID() const { return _deviceID; }
    
private:
    std::string   _name;
    AudioDeviceID _deviceID;
};

class CoreAudioDeviceFactory : public IAudioDeviceFactory
{
public:
    CoreAudioDeviceFactory();
    
    virtual std::vector<const IAudioDeviceDescriptor*> Enumerate() const;
    
private:
    std::vector<CoreAudioDeviceDescriptor> _devices;
};

class CoreAudioStreamDescriptor : public IAudioStreamDescriptor
{
public:
    CoreAudioStreamDescriptor(AudioStreamID   id,
                              AudioDeviceID   deviceID,
                              StreamType      type,
                              int             channels,
                              SampleType      sampleType,
                              int             sampleSizeBits,
                              int             sampleWordSizeBytes,
                              SampleAlignment alignment,
                              float64_t       sampleRate) :
        _id(id),
        _deviceID(deviceID),
        _type(type),
        _nChannels(channels),
        _sampleType(sampleType),
        _sampleSizeBits(sampleSizeBits),
        _sampleWordSizeBytes(sampleWordSizeBytes),
        _alignment(alignment),
        _sampleRate(sampleRate)
    {
    }
    
    virtual StreamType      GetStreamType() const { return _type; }
    virtual int             GetNumChannels() const { return _nChannels; }
    virtual SampleType      GetSampleType() const { return _sampleType; }
    virtual int             GetSampleSizeInBits() const { return _sampleSizeBits; }
    virtual int             GetSampleWordSizeInBytes() const { return _sampleWordSizeBytes; }
    virtual SampleAlignment GetSampleAlignment() const { return _alignment; }
    virtual float64_t       GetSampleRate() const { return _sampleRate; }
    
    virtual IAudioStream *GetStream() const;
    
    AudioStreamID GetID() const { return _id; }
    AudioDeviceID GetDeviceID() const { return _deviceID; }
private:
    AudioStreamID   _id;
    AudioDeviceID   _deviceID;
    StreamType      _type;
    int             _nChannels;
    SampleType      _sampleType;
    int             _sampleSizeBits;
    int             _sampleWordSizeBytes;
    SampleAlignment _alignment;
    float64_t       _sampleRate;
};

class CoreAudioDevice : public IAudioDevice
{
public:
    CoreAudioDevice(const CoreAudioDeviceDescriptor &desc);
    
    virtual const IAudioDeviceDescriptor *GetDescriptor() const { return &_desc; }
    virtual std::vector<const IAudioStreamDescriptor*> GetStreams() const;
    
    virtual void Start();
    virtual void Stop();
private:
    CoreAudioDeviceDescriptor _desc;
    std::vector<CoreAudioStreamDescriptor> _streams;
};

class CoreAudioStream : public IAudioStream
{
public:
    CoreAudioStream(const CoreAudioStreamDescriptor &desc) : _desc(desc), _proc(0), _procID(0) {}
    ~CoreAudioStream();
    
    virtual const IAudioStreamDescriptor *GetDescriptor() const { return &_desc; }
    virtual void SetProc(IAudioStreamProcessor *proc);
    
private:
    static OSStatus _audioCB(AudioObjectID           inDevice,
                             const AudioTimeStamp*   inNow,
                             const AudioBufferList*  inInputData,
                             const AudioTimeStamp*   inInputTime,
                             AudioBufferList*        outOutputData,
                             const AudioTimeStamp*   inOutputTime,
                             void*                   inClientData);

    CoreAudioStreamDescriptor _desc;
    IAudioStreamProcessor *_proc;
    AudioDeviceIOProcID _procID;
};

#endif // MAC

#endif /* defined(__mac__AudioDevice__) */
