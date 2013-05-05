//
//  AudioDevice.cpp
//  mac
//
//  Created by Paul Ciarlo on 5/1/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#include "AudioDevice.h"

#include <math.h>

SineAudioOutputProcessor::SineAudioOutputProcessor(float32_t frequency, const IAudioStreamDescriptor *streamDesc) :
    _frequency(frequency),
    _time(0.0)
{
    _sampleRate = streamDesc->GetSampleRate();
    _frameSize = streamDesc->GetNumChannels() * streamDesc->GetSampleWordSizeInBytes();
}
    
void SineAudioOutputProcessor::Process(IAudioBuffer *buffer)
{
    float32_t *sample = (float32_t*)buffer->GetBuffer();
    int bufferSizeFrames = buffer->GetBufferSize() / _frameSize;
    for (int i = 0; i < bufferSizeFrames; ++i)
    {
        const float32_t smp = sin(2*M_PI*_frequency*_time);
        sample[i*2] = smp;
        sample[i*2+1] = smp;
        _time += 1.0 / _sampleRate;
    }
}

#if MAC
#include <CoreAudio/CoreAudio.h>

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
    std::vector<AudioDeviceDescriptor> devices;
    
    AudioObjectPropertyAddress propertyAddress = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                            &propertyAddress,
                                            0,
                                            NULL,
                                            &propertySize);
    if (result != 0)
    {
        // should throw exception really
        printf("Error finding number of audio devices");
        throw std::exception();
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
    CFStringRef deviceName;
    for (UInt32 i = 0; i < numDevices; ++i)
    {
        propertySize = sizeof(CFStringRef);
        propertyAddress.mSelector = kAudioObjectPropertyName;
        propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
        propertyAddress.mElement = kAudioObjectPropertyElementMaster;
        AudioObjectGetPropertyData(deviceList[i], &propertyAddress, 0, NULL, &propertySize, &deviceName);
        
        const char *cStrName = CFStringGetCStringPtr(deviceName, CFStringGetFastestEncoding(deviceName));
        if (cStrName == 0)
        {
            printf("bad c string device name");
            continue;
        }
        _devices.push_back(CoreAudioDeviceDescriptor(cStrName, deviceList[i]));
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
            throw std::exception();
        }
        
        sampleAlignment =
            streamDesc.mFormatFlags & kAudioFormatFlagIsAlignedHigh ?
            IAudioStreamDescriptor::MostSignificant :
            IAudioStreamDescriptor::LeastSignifcant;
        
        printf("%d\n", streamDesc.mFormatFlags);
        
        _streams.push_back(CoreAudioStreamDescriptor(myStreams[i], _desc.GetID(), streamType, channels, sampleType, sampleSizeBits, sampleWordSizeBytes, sampleAlignment, sampleRate));
    }
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
    
}

void CoreAudioDevice::Stop()
{
    
}

IAudioStream *CoreAudioStreamDescriptor::GetStream() const
{
    return new CoreAudioStream(*this);
}

CoreAudioStream::~CoreAudioStream()
{
    if (_procID != 0)
        AudioDeviceStop(_desc.GetDeviceID(), _procID);
}

void CoreAudioStream::SetProc(IAudioStreamProcessor *proc)
{
    _proc = proc;
    
    AudioDeviceCreateIOProcID(_desc.GetDeviceID(), _audioCB, this, &_procID);
    AudioDeviceStart(_desc.GetDeviceID(), _procID);
}

OSStatus CoreAudioStream::_audioCB(AudioObjectID           inDevice,
                                   const AudioTimeStamp*   inNow,
                                   const AudioBufferList*  inInputData,
                                   const AudioTimeStamp*   inInputTime,
                                   AudioBufferList*        outOutputData,
                                   const AudioTimeStamp*   inOutputTime,
                                   void*                   inClientData)
{
    SimpleAudioBuffer buf;
    UInt32 numBuffers = outOutputData->mNumberBuffers;
    for (UInt32 i = 0; i < numBuffers; ++i)
    {
        //printf("ch %d size %d\n", outOutputData->mBuffers[i].mNumberChannels,
        //       outOutputData->mBuffers[i].mDataByteSize);
        //outOutputData->mBuffers[i].m
        buf.SetBuffer(outOutputData->mBuffers[i].mData);
        buf.SetBufferSize(outOutputData->mBuffers[i].mDataByteSize);
        ((CoreAudioStream*)inClientData)->_proc->Process(&buf);
    }
    return 0;
}

#endif // MAC