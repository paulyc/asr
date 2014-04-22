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
	
	// does not delete inputs
	void AddInput(CoreAudioInput *in) { _inputs.push_back(in); }
	virtual void ProcessInput(IAudioBuffer *buffer);
	virtual void ProcessOutput(IAudioStream *stream, IAudioBuffer *buffer) {}
private:
	int _channels;
	int _frameSize;
	std::vector<CoreAudioInput*> _inputs;
};

class CoreAudioOutput : public AudioOutput
{
public:
	CoreAudioOutput(IAudioStream *stream, ChunkGenerator *gen, int id, int ch1ofs, int ch2ofs) :
		AudioOutput(0, ch1ofs, ch2ofs),
		_stream(stream),
		_gen(gen),
		_id(id),
		_clip(false)
	{}
	virtual void process(MultichannelAudioBuffer *buf);
	virtual IAudioStream *getStream() const { return _stream; }
private:
	IAudioStream *_stream;
	ChunkGenerator *_gen;
	int _id;
	bool _clip;
};

class CoreAudioOutputProcessor : public IAudioStreamProcessor
{
public:
	CoreAudioOutputProcessor();
	virtual ~CoreAudioOutputProcessor();
	
	// does delete outputs
	void AddOutput(CoreAudioOutput *out) { _outputs.push_back(out); }
	virtual void ProcessOutput(IAudioStream *stream, IAudioBuffer *buffer);
	virtual void ProcessInput(IAudioBuffer *buffer) {}
private:
	std::vector<CoreAudioOutput*> _outputs;
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

#include "CoreAudio.h"

#endif // !IOS

#endif /* defined(__mac__AudioDevice__) */
