//
//  CoreAudio.cpp
//  mac
//
//  Created by Paul Ciarlo on 5/1/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#include <CoreAudio/CoreAudio.h>

#include "AudioDevice.h"

struct dat {
    int x;
} myData;

OSStatus
 myAudioCb(   AudioObjectID           inDevice,
                     const AudioTimeStamp*   inNow,
                     const AudioBufferList*  inInputData,
                     const AudioTimeStamp*   inInputTime,
                     AudioBufferList*        outOutputData,
                     const AudioTimeStamp*   inOutputTime,
                     void*                   inClientData)
{
    printf("hello ");
    UInt32 numBuffers = outOutputData->mNumberBuffers;
    for (UInt32 i = 0; i < numBuffers; ++i)
    {
        printf("ch %d size %d\n", outOutputData->mBuffers[i].mNumberChannels,
               outOutputData->mBuffers[i].mDataByteSize);
    }
    return 0;
}

int main1()
{
    // get size of
    UInt32 data;
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
    numDevices = propertySize / sizeof(AudioDeviceID);
    deviceList = new AudioDeviceID[numDevices];
    printf("size = %d devs = %d\n", propertySize, numDevices);
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
        CFShow(deviceName);
    }
    
    AudioDeviceID myDevice = deviceList[1];
    myData.x = 666;
    
    propertyAddress.mSelector = kAudioDevicePropertyStreams;
    AudioObjectGetPropertyDataSize(myDevice, &propertyAddress, 0, NULL, &propertySize);
    UInt32 numStreams = propertySize / sizeof(AudioStreamID);
    printf("size = %d streams = %d\n", propertySize, numStreams);
    
    AudioStreamID myStream;
    AudioStreamBasicDescription streamDesc;
    AudioObjectGetPropertyData(myDevice, &propertyAddress, 0, NULL, &propertySize, &myStream);
    
    
    propertySize = sizeof(streamDesc);
    propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
    result = AudioObjectGetPropertyData(myStream, &propertyAddress, 0, NULL, &propertySize, &streamDesc);
    union {
        UInt32 id;
        char chs[4];
    } un;
    un.id = streamDesc.mFormatID;
    printf("res = %d id = %c%c%c%c smp rate %f bits %d flags %08x bytes %d framespp %d bytespf %d\n", result, un.chs[3], un.chs[2], un.chs[1], un.chs[0], streamDesc.mSampleRate, streamDesc.mBitsPerChannel, streamDesc.mFormatFlags, streamDesc.mBytesPerPacket, streamDesc.mFramesPerPacket, streamDesc.mBytesPerFrame);
    
    AudioDeviceIOProcID myProcId;
    AudioDeviceCreateIOProcID(myDevice, myAudioCb, &myData, &myProcId);
    AudioDeviceStart(myDevice, myProcId);
    
    while(true){}
    
    delete [] deviceList;
    
    return 0;
}

#include "AudioDevice.h"

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
        if (dev->GetName() == std::string("Built-in Output"))
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
    
    while (true) {}
    
    //stream->RemoveProc();
    delete stream;
    delete proc;
    delete device;
    
    return 0;
}

int main()
{
    return main2();
}
