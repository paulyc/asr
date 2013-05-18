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
#elif WINDOWS
#include <asio.h>
#endif

#include "type.h"
#include "stream.h"

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
    enum StreamType      { Input, Output };
    enum SampleType      { SignedInt, UnsignedInt, Float };
    enum SampleAlignment { LeastSignificant, MostSignificant };
    
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

class IAudioStream;

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

class MultichannelAudioBuffer : public IAudioBuffer
{
public:
    MultichannelAudioBuffer() : _buffer(0) {}
    MultichannelAudioBuffer(void *buf) : _buffer(buf) {}
    
    virtual void *GetBuffer() { return _buffer; }
    void SetBuffer(void *buf) { _buffer = buf; }
    virtual int GetBufferSize() const { return _bufferSize; }
    void SetBufferSize(int sz) { _bufferSize = sz; }
    
    virtual int GetBufferSizeFrames() = 0;
    virtual void *GetChannelBuffer(int ch) = 0;
    virtual int GetStride() = 0;
    virtual int GetNumChannels() = 0;
protected:
    void *_buffer;
    int _bufferSize;
};

class CoreAudioBuffer : public MultichannelAudioBuffer
{
public:
    CoreAudioBuffer(int channels, int bytesPerFrame) : _channels(channels), _bytesPerFrame(bytesPerFrame) {}
    
    virtual int GetBufferSizeFrames() { return _bufferSize / _bytesPerFrame; }
    virtual void *GetChannelBuffer(int ch) { return ((float32_t*)_buffer) + ch; }
    virtual int GetStride() { return _channels; }
    virtual int GetNumChannels() { return _channels; }
private:
    int _channels;
    int _bytesPerFrame;
};

class AudioInput : public T_source<chunk_t>
{
public:
	virtual ~AudioInput() {}
    
	virtual void process(MultichannelAudioBuffer *buf) = 0;
    chunk_t *next() = 0;
};

class AudioOutput : public T_sink_source<chunk_t>
{
public:
    AudioOutput(T_source<chunk_t> *src, int ch1id, int ch2id) :
    T_sink_source<chunk_t>(src),
    _ch1id(ch1id),
    _ch2id(ch2id)
    {
        _chk = 0;
        _read = 0;
    }
	virtual ~AudioOutput()
    {
        T_allocator<chunk_t>::free(_chk);
    }
    
	virtual void process(MultichannelAudioBuffer *buf) = 0;
	virtual void switchBuffers(int dbIndex) {}
protected:
    int _ch1id;
    int _ch2id;
	chunk_t *_chk;
	typename chunk_t::sample_t *_read;
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

#if MAC
class CoreAudioInput : public AudioInput
{
public:
    CoreAudioInput(int id, int ch1ofs, int ch2ofs) : _id(id), _ch1ofs(ch1ofs), _ch2ofs(ch2ofs) {}
	virtual ~CoreAudioInput();
    
	virtual void process(MultichannelAudioBuffer *buf);
    chunk_t *next();
private:
    int _id;
    int _ch1ofs;
    int _ch2ofs;
    std::queue<chunk_t*> _chunkQ;
    Lock_T _lock;
};

class CoreAudioOutput : public AudioOutput
{
public:
    CoreAudioOutput(ChunkGenerator *gen, int id, int ch1ofs, int ch2ofs) : AudioOutput(0, ch1ofs, ch2ofs), _gen(gen), _id(id) {}
    virtual void process(MultichannelAudioBuffer *buf);
private:
    ChunkGenerator *_gen;
    int _id;
};

class CoreAudioOutputProcessor : public IAudioStreamProcessor
{
public:
    CoreAudioOutputProcessor(const IAudioStreamDescriptor *streamDesc);
    
    void AddOutput(const CoreAudioOutput &out) { _outputs.push_back(out); }
    virtual void Process(IAudioBuffer *buffer);
private:
    int _channels;
    int _frameSize;
    std::vector<CoreAudioOutput> _outputs;
    
    chunk_t *_chk;
	typename chunk_t::sample_t *_read;
};

class CoreAudioInputProcessor : public IAudioStreamProcessor
{
public:
    void AddInput(const CoreAudioInput &in);
    virtual void Process(IAudioBuffer *buffer);
};
#elif WINDOWS

#endif // WINDOWS

class IAudioStream
{
public:
    virtual ~IAudioStream() {}
    
    virtual const IAudioStreamDescriptor *GetDescriptor() const = 0;
    virtual void SetProc(IAudioStreamProcessor *proc) = 0;
    
    virtual void Start() = 0;
    virtual void Stop() = 0;
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
    CoreAudioDeviceDescriptor(AudioDeviceID devId) :
        _deviceID(devId)
    {
        CFStringRef deviceName;
        AudioObjectPropertyAddress propertyAddress;
        UInt32 propertySize = sizeof(CFStringRef);
        propertyAddress.mSelector = kAudioObjectPropertyName;
        propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
        propertyAddress.mElement = kAudioObjectPropertyElementMaster;
        AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &propertySize, &deviceName);
        
        _name = CFStringRefToString(deviceName);

        propertySize = sizeof(UInt32);
        propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
        AudioObjectGetPropertyData(_deviceID, &propertyAddress, 0, NULL, &propertySize, &_bufferSizeFrames);
    }
    
    virtual std::string GetName() const { return _name; }
    virtual IAudioDevice *Instantiate() const;
    
    AudioDeviceID GetID() const { return _deviceID; }
    virtual uint32_t GetBufferSizeFrames() const { return _bufferSizeFrames; }
    
private:
    std::string   _name;
    AudioDeviceID _deviceID;
    UInt32        _bufferSizeFrames;
};

class CoreAudioDeviceFactory : public IAudioDeviceFactory
{
public:
    CoreAudioDeviceFactory();
    
    virtual std::vector<const IAudioDeviceDescriptor*> Enumerate() const;
    
private:
    std::vector<CoreAudioDeviceDescriptor> _devices;
};

class CoreAudioDevice;

class CoreAudioStreamDescriptor : public IAudioStreamDescriptor
{
public:
    CoreAudioStreamDescriptor(AudioStreamID   id,
                              CoreAudioDevice *device,
                              StreamType      type,
                              int             channels,
                              SampleType      sampleType,
                              int             sampleSizeBits,
                              int             sampleWordSizeBytes,
                              SampleAlignment alignment,
                              float64_t       sampleRate) :
        _id(id),
        _device(device),
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
    AudioDeviceID GetDeviceID() const;
private:
    AudioStreamID    _id;
    CoreAudioDevice* _device;
    StreamType       _type;
    int              _nChannels;
    SampleType       _sampleType;
    int              _sampleSizeBits;
    int              _sampleWordSizeBytes;
    SampleAlignment  _alignment;
    float64_t        _sampleRate;
};

class CoreAudioDevice : public IAudioDevice
{
public:
    CoreAudioDevice(const CoreAudioDeviceDescriptor &desc);
    
    virtual AudioDeviceID GetID() const { return _desc.GetID(); }
    virtual const IAudioDeviceDescriptor *GetDescriptor() const { return &_desc; }
    virtual std::vector<const IAudioStreamDescriptor*> GetStreams() const;
    virtual void RegisterStream(IAudioStream *stream) { _streamRegister.push_back(stream); }
    
    virtual void Start();
    virtual void Stop();
private:
    CoreAudioDeviceDescriptor _desc;
    std::vector<CoreAudioStreamDescriptor> _streams;
    std::vector<IAudioStream*> _streamRegister;
};

class CoreAudioStream : public IAudioStream
{
public:
    CoreAudioStream(const CoreAudioStreamDescriptor &desc, CoreAudioDevice *dev) : _desc(desc), _proc(0), _procID(0), _device(dev) {}
    virtual ~CoreAudioStream();
    
    virtual const IAudioStreamDescriptor *GetDescriptor() const { return &_desc; }
    virtual void SetProc(IAudioStreamProcessor *proc);
    
    virtual void Start();
    virtual void Stop();
    
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
    CoreAudioDevice *_device;
};

#elif WINDOWS

class ASIODriverDescriptor : public IAudioDriverDescriptor
{
public:
    virtual std::string GetName() const { return std::string("ASIO"); }
    virtual IAudioDeviceFactory *Instantiate() const;
};

class WindowsDriverFactory : public IAudioDriverFactory
{
public:
    WindowsDriverFactory();
    virtual ~WindowsDriverFactory();
    
    virtual std::vector<const IAudioDriverDescriptor*> Enumerate() const { return _drivers; }
private:
    std::vector<const IAudioDriverDescriptor*> _drivers;
};

class ASIODeviceDescriptor : public IAudioDeviceDescriptor
{
public:
    ASIODeviceDescriptor(const char *name, const char *clsid) : _name(name), _clsID(clsid) {}
    
    virtual std::string GetName() const { return _name; }
    virtual IAudioDevice *Instantiate() const;
private:
    std::string _name;
    const char *_clsID;
};

class DummyASIODeviceDescriptor : public IAudioDeviceDescriptor
{
public:
    virtual std::string GetName() const { return std::string("Dummy ASIO"); }
    virtual IAudioDevice *Instantiate() const;
}

class ASIODeviceFactory : public IAudioDeviceFactory
{
public:
    ASIODeviceFactory();
    virtual ~ASIODeviceFactory();
    
    virtual std::vector<const IAudioDeviceDescriptor*> Enumerate() const { return _devices; }
private:
    std::vector<const IAudioDeviceDescriptor*> _devices;
};

class ASIODevice;

class ASIOStreamDescriptor : public IAudioStreamDescriptor
{
public:
    ASIOStreamDescriptor(int             id,
                         ASIODevice*     device,
                         StreamType      type,
                         int             channels,
                         SampleType      sampleType,
                         int             sampleSizeBits,
                         int             sampleWordSizeBytes,
                         SampleAlignment alignment,
                         float64_t       sampleRate) :
    _id(id),
    _device(device),
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
    
private:
    int             _id;
    ASIODevice*     _device;
    StreamType      _type;
    int             _nChannels;
    SampleType      _sampleType;
    int             _sampleSizeBits;
    int             _sampleWordSizeBytes;
    SampleAlignment _alignment;
    float64_t       _sampleRate;
};

class ASIODevice : public IAudioDevice
{
public:
    ASIODevice(const ASIODeviceDescriptor &desc, IASIO *asio);
    virtual ~ASIODevice() {}
    
    virtual const IAudioDeviceDescriptor *GetDescriptor() const { return &_desc; }
    virtual std::vector<const IAudioStreamDescriptor*> GetStreams() const;
    virtual void RegisterStream(IAudioStream *stream);
    
    virtual void Start() { ASIOStart(); }
    virtual void Stop() { ASIOStop(); }
private:
    ASIODeviceDescriptor _desc;
    IASIO *_asio;
    std::vector<ASIOStreamDescriptor> _streams;
};

class ASIOStream
{
public:
    virtual ~ASIOStream() {}
    
    virtual const IAudioStreamDescriptor *GetDescriptor() const;
    virtual void SetProc(IAudioStreamProcessor *proc);
    
    virtual void Start();
    virtual void Stop();
};

#endif // WINDOWS

#endif /* defined(__mac__AudioDevice__) */
