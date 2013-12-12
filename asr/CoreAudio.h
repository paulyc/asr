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

#ifndef __mac__CoreAudio__
#define __mac__CoreAudio__

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

class CoreAudioBufferOwner : public CoreAudioBuffer
{
public:
	CoreAudioBufferOwner(int channels, int bytesPerSample, int numFrames) : CoreAudioBuffer(channels, channels*bytesPerSample)
	{
		_bufferSize = channels * bytesPerSample * numFrames;
		_buffer = new uint8_t[_bufferSize];
	}
	virtual ~CoreAudioBufferOwner()
	{
		delete [] (uint8_t*)_buffer;
	}
};

#endif /* defined(__mac__CoreAudio__) */
