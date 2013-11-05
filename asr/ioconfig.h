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

#ifndef __mac__ioconfig__
#define __mac__ioconfig__

#include "AudioDevice.h"
#include <sstream>
#include <unordered_map>

struct ChannelConfig
{
	IAudioStream *stream;
	int leftChannelIndex;
	int rightChannelIndex;
	
	std::string toString() const
	{
		if (!stream)
			return "None";
		std::ostringstream oss;
		oss << stream->GetDescriptor()->GetDevice()->GetDescriptor()->GetName() << " " <<
			leftChannelIndex << "/" << rightChannelIndex;
		return oss.str();
	}
};

class IOConfig
{
public:
	IOConfig();
	virtual ~IOConfig() throw();
	
	void initDriver(std::string name);
	void cleanupDriver();
	void setupDefaultConfig();
	std::vector<std::pair<int, std::string>> getChannels(IAudioStreamDescriptor::StreamType type) const;
	std::vector<std::pair<int, std::string>> getInputChannels() const;
	std::vector<std::pair<int, std::string>> getOutputChannels() const;
	enum ConfigOption
	{
		Output1Channel, Output2Channel, Input1Channel // correspond to index in channels vector
	};
	
	// Note you must call configure once after all setConfigOption calls or things may break!
	void setConfigOption(ConfigOption opt, int value) { _configs[opt] = value; }
	virtual void configure() = 0;
	
	IAudioDevice *getInputDevice();
	IAudioDevice *getOutputDevice();
	const ChannelConfig &getChannel(ConfigOption config) { return _channels[_configs[config]]; }
	
protected:
	MacAudioDriverFactory _fac;
	std::vector<const IAudioDriverDescriptor*> _drivers;
	std::vector<IAudioDevice*> _devices;
	std::vector<IAudioStream*> _streams;
	std::vector<ChannelConfig> _channels;
	
	std::unordered_map<ConfigOption, int, std::hash<int>> _configs;
	
	const ChannelConfig _noneChannel = {0, 0, 0};
};

#endif /* defined(__mac__ioconfig__) */
