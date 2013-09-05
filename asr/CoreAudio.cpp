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

#if 0
int main()
{
	return main2();
}
#endif
