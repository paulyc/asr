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

#include "AudioDevice.h"

#include <iostream>

IAudioDeviceFactory *MacAudioDriverDescriptor::Instantiate() const
{
	return new CoreAudioDeviceFactory;
}

MacAudioDriverFactory::MacAudioDriverFactory()
{
	_drivers.push_back(MacAudioDriverDescriptor("CoreAudio"));
}

std::vector<const IAudioDriverDescriptor*> MacAudioDriverFactory::Enumerate() const
{
	std::vector<const IAudioDriverDescriptor*> drivers;
	drivers.push_back(&_drivers[0]);
	return drivers;
}

CoreAudioDeviceFactory::CoreAudioDeviceFactory()
{
	UInt32 propertySize;
	UInt32 numDevices;
	OSStatus result;
	AudioDeviceID *deviceList = 0;
	
	AudioObjectPropertyAddress propertyAddress = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
	result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
											&propertyAddress,
											0,
											NULL,
											&propertySize);
	if (result != 0)
	{
		throw std::runtime_error("Error finding number of audio devices");
	}
	
	numDevices = propertySize / sizeof(AudioDeviceID);
	deviceList = new AudioDeviceID[numDevices];
	// printf("size = %d devs = %d\n", propertySize, numDevices);
	result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
										&propertyAddress,
										0,
										NULL,
										&propertySize,
										deviceList);
	for (UInt32 i = 0; i < numDevices; ++i)
	{
		_devices.push_back(CoreAudioDeviceDescriptor(deviceList[i]));
	}
	
	delete [] deviceList;
}

std::vector<const IAudioDeviceDescriptor*> CoreAudioDeviceFactory::Enumerate() const
{
	std::vector<const IAudioDeviceDescriptor*> devs;
	
	for (std::vector<CoreAudioDeviceDescriptor>::const_iterator i = _devices.begin();
		 i != _devices.end();
		 i++)
	{
		devs.push_back(&*i);
	}
	
	return devs;
}

IAudioDevice *CoreAudioDeviceDescriptor::Instantiate() const
{
	return new CoreAudioDevice(*this);
}

CoreAudioDevice::CoreAudioDevice(const CoreAudioDeviceDescriptor &desc) :
_desc(desc)
{
	OSStatus result;
	UInt32 propertySize;
	AudioObjectPropertyAddress propertyAddress;
	propertyAddress.mSelector = kAudioDevicePropertyStreams;
	propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
	propertyAddress.mElement = kAudioObjectPropertyElementMaster;
	
	AudioObjectGetPropertyDataSize(_desc.GetID(), &propertyAddress, 0, NULL, &propertySize);
	UInt32 numStreams = propertySize / sizeof(AudioStreamID);
	printf("size = %d streams = %d\n", propertySize, numStreams);
	
	AudioStreamID * myStreams = new AudioStreamID[numStreams];
	AudioStreamBasicDescription streamDesc;
	AudioObjectGetPropertyData(_desc.GetID(), &propertyAddress, 0, NULL, &propertySize, myStreams);
	
	/*UInt32 running;
	 propertyAddress.mSelector = kAudioDevicePropertyDeviceIsRunning;
	 propertySize = sizeof(running);
	 AudioObjectGetPropertyData(_desc.GetID(), &propertyAddress, 0, NULL, &propertySize, &running);
	 printf("running %d\n", running);
	 running = 1;
	 AudioObjectSetPropertyData(_desc.GetID(), &propertyAddress, 0, NULL, propertySize, &running);*/
	
	for (UInt32 i = 0; i < numStreams; ++i)
	{
		propertySize = sizeof(streamDesc);
		//propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
		propertyAddress.mSelector = kAudioStreamPropertyVirtualFormat;
		
		result = AudioObjectGetPropertyData(myStreams[i], &propertyAddress, 0, NULL, &propertySize, &streamDesc);
		union {
			UInt32 id;
			char chs[4];
		} un;
		un.id = streamDesc.mFormatID;
		
		UInt32 isInput;
		propertyAddress.mSelector = kAudioStreamPropertyDirection;
		propertySize = sizeof(isInput);
		result = AudioObjectGetPropertyData(myStreams[i], &propertyAddress, 0, NULL, &propertySize, &isInput);
		
		printf("res = %d input = %d id = %c%c%c%c smp rate %f bits %d flags %08x bytes %d framespp %d bytespf %d cpf %d\n", result, isInput, un.chs[3], un.chs[2], un.chs[1], un.chs[0], streamDesc.mSampleRate, streamDesc.mBitsPerChannel, streamDesc.mFormatFlags, streamDesc.mBytesPerPacket, streamDesc.mFramesPerPacket, streamDesc.mBytesPerFrame, streamDesc.mChannelsPerFrame);
		
		IAudioStreamDescriptor::StreamType streamType =
		isInput ?
		IAudioStreamDescriptor::Input :
		IAudioStreamDescriptor::Output;
		int channels = streamDesc.mChannelsPerFrame;
		IAudioStreamDescriptor::SampleType sampleType;
		int sampleSizeBits = streamDesc.mBitsPerChannel;
		int sampleWordSizeBytes = streamDesc.mBytesPerFrame / channels;
		IAudioStreamDescriptor::SampleAlignment sampleAlignment;
		float64_t sampleRate = streamDesc.mSampleRate;
		
		switch (streamDesc.mFormatID)
		{
			case 'lpcm':
				break;
			default:
				printf("Can't handle format not LPCM\n");
				throw std::exception();
				break;
		}
		if (streamDesc.mFormatFlags & kAudioFormatFlagIsFloat)
			sampleType = IAudioStreamDescriptor::Float;
		else if (streamDesc.mFormatFlags & kAudioFormatFlagIsSignedInteger)
			sampleType = IAudioStreamDescriptor::SignedInt;
		else
			sampleType = IAudioStreamDescriptor::UnsignedInt;
		
		if (streamDesc.mFormatFlags & kAudioFormatFlagIsBigEndian)
		{
			printf("Can't handle big endian\n");
			//	  throw std::exception();
		}
		
		sampleAlignment =
		streamDesc.mFormatFlags & kAudioFormatFlagIsAlignedHigh ?
		IAudioStreamDescriptor::MostSignificant :
		IAudioStreamDescriptor::LeastSignificant;
		
		printf("%d\n", streamDesc.mFormatFlags);
		
		_streams.push_back(CoreAudioStreamDescriptor(myStreams[i], this, streamType, channels, sampleType, sampleSizeBits, sampleWordSizeBytes, sampleAlignment, sampleRate));
	}
	delete [] myStreams;
}

std::vector<const IAudioStreamDescriptor*> CoreAudioDevice::GetStreams() const
{
	std::vector<const IAudioStreamDescriptor*> streams;
	for (std::vector<CoreAudioStreamDescriptor>::const_iterator i = _streams.begin();
		 i != _streams.end();
		 i++)
	{
		streams.push_back(&*i);
	}
	return streams;
}

void CoreAudioDevice::Start()
{
	for (std::vector<IAudioStream*>::iterator i = _streamRegister.begin();
		 i != _streamRegister.end();
		 i++)
	{
		(*i)->Start();
	}
}

void CoreAudioDevice::Stop()
{
	for (std::vector<IAudioStream*>::iterator i = _streamRegister.begin();
		 i != _streamRegister.end();
		 i++)
	{
		(*i)->Stop();
	}
}

IAudioDevice *CoreAudioStreamDescriptor::GetDevice() const
{
	return _device;
}

IAudioStream *CoreAudioStreamDescriptor::GetStream() const
{
	IAudioStream *stream = new CoreAudioStream(*this, _device);
	_device->RegisterStream(stream);
	return stream;
}

AudioDeviceID CoreAudioStreamDescriptor::GetDeviceID() const
{
	return _device->GetID();
}

CoreAudioStream::~CoreAudioStream()
{
	if (_proc)
		AudioDeviceDestroyIOProcID(_desc.GetDeviceID(), _procID);
}

void CoreAudioStream::SetProc(IAudioStreamProcessor *proc)
{
	if (_proc)
		AudioDeviceDestroyIOProcID(_desc.GetDeviceID(), _procID);
	_proc = proc;
	AudioDeviceCreateIOProcID(_desc.GetDeviceID(), _audioCB, this, &_procID);
}

void CoreAudioStream::Start()
{
	if (_proc)
	{
		AudioDeviceStart(_desc.GetDeviceID(), _procID);
	}
}

void CoreAudioStream::Stop()
{
	if (_proc)
	{
		AudioDeviceStop(_desc.GetDeviceID(), _procID);
	}
}

OSStatus CoreAudioStream::_audioCB(AudioObjectID		   inDevice,
								   const AudioTimeStamp*   inNow,
								   const AudioBufferList*  inInputData,
								   const AudioTimeStamp*   inInputTime,
								   AudioBufferList*		   outOutputData,
								   const AudioTimeStamp*   inOutputTime,
								   void*				   inClientData)
{
	const CoreAudioStream* const stream = (const CoreAudioStream* const)inClientData;
	const int nChannels = stream->_desc.GetNumChannels();
	const int bytesPerFrame = nChannels * stream->_desc.GetSampleWordSizeInBytes();
	
	CoreAudioBuffer buf(nChannels, bytesPerFrame);
	UInt32 numBuffers = outOutputData->mNumberBuffers;
	for (UInt32 i = 0; i < numBuffers; ++i)
	{
		buf.SetBuffer(outOutputData->mBuffers[i].mData);
		buf.SetBufferSize(outOutputData->mBuffers[i].mDataByteSize);
		stream->_proc->ProcessOutput(&buf);
	}
	numBuffers = inInputData->mNumberBuffers;
	for (UInt32 i = 0; i < numBuffers; ++i)
	{
		buf.SetBuffer(inInputData->mBuffers[i].mData);
		buf.SetBufferSize(inInputData->mBuffers[i].mDataByteSize);
		stream->_proc->ProcessInput(&buf);
	}
	return 0;
}

#if 0
int main2()
{
	MacAudioDriverFactory driverFac;
	
	std::vector<const IAudioDriverDescriptor*> drivers = driverFac.Enumerate();
	IAudioDeviceFactory *deviceFac = drivers[0]->Instantiate();
	
	std::vector<const IAudioDeviceDescriptor*> devices = deviceFac->Enumerate();
	
	IAudioDevice *device = 0;
	
	for (std::vector<const IAudioDeviceDescriptor*>::iterator devIter = devices.begin();
		 devIter != devices.end();
		 devIter++)
	{
		const IAudioDeviceDescriptor *dev = *devIter;
		printf("%s\n", dev->GetName().c_str());
		//if (dev->GetName() == std::string("Built-in Output"))
		if (dev->GetName() == std::string("Saffire"))
		{
			device = dev->Instantiate();
		}
	}
	
	if (!device)
		return 1;
	
	delete deviceFac; // invalidates devices
	
	SineAudioOutputProcessor *proc = 0;
	
	std::vector<const IAudioStreamDescriptor*> streams = device->GetStreams();
	/*for (std::vector<const IAudioStreamDescriptor*>::iterator streamIter = streams.begin();
		 streamIter != streams.end();
		 streamIter++)
	{
		
	}*/
	IAudioStream *stream = (*streams.begin())->GetStream();
	
	proc = new SineAudioOutputProcessor(440.0f, stream->GetDescriptor());
	stream->SetProc(proc);
	
	std::string line;
	std::cin >> line;
	
	//stream->RemoveProc();
	delete stream;
	delete proc;
	delete device;
	
	return 0;
}
#endif

#if 0
int main()
{
	return main2();
}
#endif
