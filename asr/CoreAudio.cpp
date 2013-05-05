//
//  CoreAudio.cpp
//  mac
//
//  Created by Paul Ciarlo on 5/1/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#include "AudioDevice.h"

#include <iostream>

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

int main()
{
    return main2();
}
