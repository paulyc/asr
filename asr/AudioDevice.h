// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef __mac__AudioDevice__
#define __mac__AudioDevice__

#include <string>
#include <vector>

#include "config.h"

#include <CoreAudio/CoreAudio.h>

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
    virtual void ProcessInput(IAudioBuffer *buffer) = 0;
    virtual void ProcessOutput(IAudioBuffer *buffer) = 0;
};

class SineAudioOutputProcessor : public IAudioStreamProcessor
{
public:
    SineAudioOutputProcessor(float32_t frequency, const IAudioStreamDescriptor *streamDesc);
    
    virtual void ProcessOutput(IAudioBuffer *buffer);
    virtual void ProcessInput(IAudioBuffer *buffer) {}
private:
    float32_t _frequency;
    float64_t _time;
    float64_t _sampleRate;
    int _frameSize;
};

class IChunkHandler
{
public:
    virtual void process(chunk_t *chk) = 0;
};

class CoreAudioInput : public AudioInput
{
public:
    CoreAudioInput(int id, int ch1ofs, int ch2ofs);
	virtual ~CoreAudioInput();
    
	virtual void process(MultichannelAudioBuffer *buf);
    chunk_t *next();
    void stop();
private:
    int _id;
    int _ch1ofs;
    int _ch2ofs;
    std::queue<chunk_t*> _chunkQ;
    Lock_T _lock;
    Condition_T _chunkAvailable;
    chunk_t *_nextChunk;
    typename chunk_t::sample_t *_writePtr;
};

// can be combined with CoreAudioOutputProcessor
class CoreAudioInputProcessor : public IAudioStreamProcessor
{
public:
    CoreAudioInputProcessor(const IAudioStreamDescriptor *streamDesc);
    
    void AddInput(CoreAudioInput *in) { _inputs.push_back(in); }
    virtual void ProcessInput(IAudioBuffer *buffer);
    virtual void ProcessOutput(IAudioBuffer *buffer) {}
private:
    int _channels;
    int _frameSize;
    std::vector<CoreAudioInput*> _inputs;
};

class CoreAudioOutput : public AudioOutput
{
public:
    CoreAudioOutput(ChunkGenerator *gen, int id, int ch1ofs, int ch2ofs) : AudioOutput(0, ch1ofs, ch2ofs), _gen(gen), _id(id), _clip(false) {}
    virtual void process(MultichannelAudioBuffer *buf);
private:
    ChunkGenerator *_gen;
    int _id;
    bool _clip;
};

class CoreAudioOutputProcessor : public IAudioStreamProcessor
{
public:
    CoreAudioOutputProcessor(const IAudioStreamDescriptor *streamDesc);
    
    void AddOutput(const CoreAudioOutput &out) { _outputs.push_back(out); }
    virtual void ProcessOutput(IAudioBuffer *buffer);
    virtual void ProcessInput(IAudioBuffer *buffer) {}
private:
    int _channels;
    int _frameSize;
    std::vector<CoreAudioOutput> _outputs;
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

#endif /* defined(__mac__AudioDevice__) */
