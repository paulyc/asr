// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo
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

#include <iostream>
#include <string>

#include "ioconfig.h"

IOConfig::IOConfig()
{
	_drivers = _fac.Enumerate();
	initDriver("CoreAudio");
}

IOConfig::~IOConfig() throw()
{
	cleanupDriver();
}

// Load all devices, streams, and channels for selected audio driver
void IOConfig::initDriver(std::string name)
{
	cleanupDriver();
	
	IAudioDeviceFactory *deviceFac = 0;
	
	for (auto driver : _drivers)
	{
		if (driver->GetName() == name)
			deviceFac = driver->Instantiate();
	}
	
	if (!deviceFac)
	{
		throw std::runtime_error("No such driver: " + name);
	}
	
	for (auto deviceDesc : deviceFac->Enumerate())
	{
		_devices.push_back(deviceDesc->Instantiate());
	}
	
	if (_devices.empty())
	{
		throw std::runtime_error("Failed to create an audio device");
	}
	
	delete deviceFac;
	
	// Get all streams and channels
	_channels.push_back(_noneChannel);
	
	for (auto dev: _devices)
	{
		for (auto streamDesc: dev->GetStreams())
		{
			IAudioStream *stream = streamDesc->GetStream();
			_streams.push_back(stream);
			
			// iterate stereo channels in stream
			for (int i=0; i < stream->GetDescriptor()->GetNumChannels(); i += 2)
			{
				_channels.push_back({stream, i, i+1});
			}
		}
	}
	
	for (const auto &config : _channels)
	{
		std::cout << config.toString() << std::endl;
	}
	
	setupDefaultConfig();
}

void IOConfig::cleanupDriver()
{
	_channels.clear();
	
	for (auto stream: _streams)
	{
		delete stream;
	}
	_streams.clear();
	
	for (auto device: _devices)
	{
		delete device;
	}
	_devices.clear();
}

// automatically select the right configuration for my (Pauls) hardware,
// otherwise use sensible defaults
void IOConfig::setupDefaultConfig()
{
	_configs[Output1Channel] = 0;
	_configs[Output2Channel] = 0;
	_configs[Input1Channel] = 0;
	
	for (int i=0; i < _channels.size(); ++i)
	{
		auto config = _channels[i];
		if (!config.stream) continue;
		auto streamDesc = config.stream->GetDescriptor();
		auto deviceDesc = streamDesc->GetDevice()->GetDescriptor();
		
		if (streamDesc->GetStreamType() == IAudioStreamDescriptor::Input)
		{
			// use 2/3 input indices for Saffire, no input if not present
			if (deviceDesc->GetName() == "Saffire" && config.leftChannelIndex == 2)
			{
				_configs[Input1Channel] = i;
			}
		}
		else
		{
			// use built-in output for channel 1 if no Saffire present
			if (deviceDesc->GetName() == "Built-in Output" && config.leftChannelIndex == 0 && _configs[Output1Channel] == 0)
			{
				_configs[Output1Channel] = i;
			}
			else if (deviceDesc->GetName() == "Saffire")
			{
				if (config.leftChannelIndex == 0)
					_configs[Output1Channel] = i;
				else if (config.rightChannelIndex == 2)
					_configs[Output2Channel] = i;
			}
		}
	}
}

std::vector<std::pair<int, std::string>> IOConfig::getChannels(IAudioStreamDescriptor::StreamType type) const
{
	std::vector<std::pair<int, std::string>> channels;
	for (int i=0; i < _channels.size(); ++i)
	{
		if (!_channels[i].stream)
			channels.push_back({i, "None"});
		else if (_channels[i].stream->GetDescriptor()->GetStreamType() == type)
			channels.push_back({i, _channels[i].toString()});
	}
	return channels;
}

std::vector<std::pair<int, std::string>> IOConfig::getInputChannels() const
{
	return getChannels(IAudioStreamDescriptor::Input);
}

std::vector<std::pair<int, std::string>> IOConfig::getOutputChannels() const
{
	return getChannels(IAudioStreamDescriptor::Output);
}

IAudioDevice *IOConfig::getInputDevice()
{
	auto inputChannel = _channels[_configs[Input1Channel]];
	if (inputChannel.stream)
		return inputChannel.stream->GetDescriptor()->GetDevice();
	else
		return 0;
}

IAudioDevice *IOConfig::getOutputDevice()
{
	// TODO all outputs must share the same device
	auto outputChannel = _channels[_configs[Output1Channel]];
	if (outputChannel.stream)
		return outputChannel.stream->GetDescriptor()->GetDevice();
	else
		return 0;
}

