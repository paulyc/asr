// ASR - Digital Signal Processor
// Copyright (C) 2002-2013	Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.	 If not, see <http://www.gnu.org/licenses/>.

#ifndef __mac__AudioDevice__
#define __mac__AudioDevice__

#include "config.h"

#include "type.h"
#include "stream.h"

#include "AudioTypes.h"

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

#if IOS

class iOSAudioStreamDescriptor : public IAudioStreamDescriptor
{
public:
	virtual int				GetNumChannels() const { return _format.mChannelsPerFrame; }
	virtual SampleType		GetSampleType() const { return SignedInt; }
	virtual int				GetSampleSizeInBits() const { return _format.mBitsPerChannel; }
	virtual int				GetSampleWordSizeInBytes() const { return sizeof(AudioSampleType); }
	virtual SampleAlignment GetSampleAlignment() const { return LeastSignificant; }
	virtual float64_t		GetSampleRate() const { return _format.mSampleRate; }
	
	virtual IAudioStream   *GetStream() const;
	
	const AudioStreamBasicDescription* GetStreamDescription() const { return &_format; }
	
	const AudioStreamBasicDescription _format = {
		.mSampleRate       = 44100.0,
		.mFormatID         = kAudioFormatLinearPCM,
		.mFormatFlags      = kAudioFormatFlagsCanonical, //  kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked,
		.mBytesPerPacket   = 2 * sizeof (AudioSampleType),
		.mFramesPerPacket  = 1,
		.mBytesPerFrame    = 2 * sizeof (AudioSampleType),
		.mChannelsPerFrame = 2,
		.mBitsPerChannel   = 8 * sizeof (AudioSampleType),
		.mReserved         = 0
	};
};

class iOSAudioOutputStreamDescriptor : public iOSAudioStreamDescriptor
{
public:
	virtual StreamType GetStreamType() const { return Output; }
};

class iOSAudioOutputStream : public IAudioStream
{
public:
	iOSAudioOutputStream(const iOSAudioOutputStreamDescriptor &desc) : _desc(desc), _q(NULL)
	{
		if (AudioQueueNewOutput(desc.GetStreamDescription(), _bufferCallback, (void*)this, CFRunLoopGetCurrent(), kCFRunLoopCommonModes, 0, &_q) != noErr)
		{
			throw std::runtime_error("AudioQueueNewOutput failed");
		}
	}
	
	virtual ~iOSAudioOutputStream()
	{
		if (_q)
			AudioQueueDispose(_q, true);
	}
	
	virtual const IAudioStreamDescriptor *GetDescriptor() const { return &_desc; }
	virtual void SetProc(IAudioStreamProcessor *proc) {}
	
	virtual void Start() {}
	virtual void Stop() {}
private:
	static void _bufferCallback(void *                  inUserData,
								AudioQueueRef           inAQ,
								AudioQueueBufferRef     inCompleteAQBuffer);
	
	const iOSAudioOutputStreamDescriptor &_desc;
	
	AudioQueueRef _q;
};

class iOSAudioDevice : public IAudioDevice
{
public:
	iOSAudioDevice(const IAudioDeviceDescriptor &desc) : _desc(desc) {}
	
	virtual const IAudioDeviceDescriptor *GetDescriptor() const { return &_desc; }
	virtual std::vector<const IAudioStreamDescriptor*> GetStreams() const {
		std::vector<const IAudioStreamDescriptor*> streams;
		streams.push_back(&_streamDesc);
		return streams;
	}
	virtual void RegisterStream(IAudioStream *stream) {}
	
	virtual void Start() {}
	virtual void Stop() {}
private:
	const IAudioDeviceDescriptor &_desc;
	iOSAudioOutputStreamDescriptor _streamDesc;
};

class iOSAudioDeviceDescriptor : public IAudioDeviceDescriptor
{
public:
	virtual std::string GetName() const { return "iOSAudioDevice"; }
	virtual IAudioDevice *Instantiate() const { return new iOSAudioDevice(*this); }
	virtual uint32_t GetBufferSizeFrames() const { return 0; }
};

#else

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
	std::string	  _name;
	AudioDeviceID _deviceID;
	UInt32		  _bufferSizeFrames;
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
	CoreAudioStreamDescriptor(AudioStreamID	  id,
							  CoreAudioDevice *device,
							  StreamType	  type,
							  int			  channels,
							  SampleType	  sampleType,
							  int			  sampleSizeBits,
							  int			  sampleWordSizeBytes,
							  SampleAlignment alignment,
							  float64_t		  sampleRate) :
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
	
	virtual IAudioDevice*   GetDevice() const;
	virtual StreamType		GetStreamType() const { return _type; }
	virtual int				GetNumChannels() const { return _nChannels; }
	virtual SampleType		GetSampleType() const { return _sampleType; }
	virtual int				GetSampleSizeInBits() const { return _sampleSizeBits; }
	virtual int				GetSampleWordSizeInBytes() const { return _sampleWordSizeBytes; }
	virtual SampleAlignment GetSampleAlignment() const { return _alignment; }
	virtual float64_t		GetSampleRate() const { return _sampleRate; }
	
	virtual IAudioStream *GetStream() const;
	
	AudioStreamID GetID() const { return _id; }
	AudioDeviceID GetDeviceID() const;
private:
	AudioStreamID	 _id;
	CoreAudioDevice* _device;
	StreamType		 _type;
	int				 _nChannels;
	SampleType		 _sampleType;
	int				 _sampleSizeBits;
	int				 _sampleWordSizeBytes;
	SampleAlignment	 _alignment;
	float64_t		 _sampleRate;
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
	static OSStatus _audioCB(AudioObjectID			 inDevice,
							 const AudioTimeStamp*	 inNow,
							 const AudioBufferList*	 inInputData,
							 const AudioTimeStamp*	 inInputTime,
							 AudioBufferList*		 outOutputData,
							 const AudioTimeStamp*	 inOutputTime,
							 void*					 inClientData);

	CoreAudioStreamDescriptor _desc;
	IAudioStreamProcessor *_proc;
	AudioDeviceIOProcID _procID;
	CoreAudioDevice *_device;
};

#endif // !IOS

#endif /* defined(__mac__AudioDevice__) */
