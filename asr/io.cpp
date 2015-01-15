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

#include "config.h"
#include "io.h"
#include "tracer.h"

#include <queue>
#include <algorithm>

#include <pthread.h>
#include <semaphore.h>

#include "compressor.h"
#include "track.h"

IOProcessor::IOProcessor() :
	_running(false),
	//_default_src("/Users/paulyc/Downloads/z C#m 130 Armin van Buuren - Pulsar (Original Mix).aiff"),
	_default_src(0),
	_my_pk_det(0),
	_outputTime(0.0),
	_src_active(false),
	_log("mylog.txt"),
	_file_out(0),
	_master_xfader(0),
	_cue(0),
	_aux(0),
	_ui(0),
	_finishing(false),
	_sync_cue(false),
	_midi_controller(0),
	_input(0),
	_inputStreamProcessor(0),
	_outputStreamProcessor(0),
	_client(CFSTR("The MIDI Client")),
	_gen(0),
	_gain1(0), _gain2(0),
	_fileWriter(0)
{
	Init();
}

IOProcessor::~IOProcessor() throw()
{
	Destroy();
}

void IOProcessor::Init()
{
	IMIDIDevice *dev = 0; //midifac.Instantiate(1, true);
	
	std::vector<const IMIDIDeviceDescriptor*> midiDevices = _client.Enumerate();
	for (int i=0; i<midiDevices.size(); ++i)
	{
		//"Pro24"
		if (midiDevices[i]->GetDeviceName() == std::string("PIONEER CDJ-350"))
		{
			dev = midiDevices[i]->Instantiate();
			break;
		}
	}
	
	if (dev)
	{
		printf("Have midi\n");
		_midi_controller = new CDJ350MIDIController(dev);
	}
#if SPOTIFY_ENABLED
	_spotify = new Spotify;
#if 0
	_spotify->login("username", "password");
#endif
#endif
}

// stop using ui before destroying
void IOProcessor::Finish()
{
	_finishing = true;
	Stop();
}

void IOProcessor::Destroy()
{
#if SPOTIFY_ENABLED
	delete _spotify;
#endif
	delete _midi_controller;
	delete _filter_controller;
	
	if (_fileWriter) _fileWriter->stop(); // deletes _fileWriter and _input
	
	delete _inputStreamProcessor;
	delete _outputStreamProcessor;
	
	delete _my_pk_det;
	//	delete _my_gain;
	
	delete _file_out;
	delete _cue;
	delete _master_xfader;
	delete _aux;
	
	_gen->kill();
	_tracks[0]->quitting();
	_tracks[1]->quitting();
	
	Worker::destroy();
	
	delete _gen;
	delete _gain1;
	delete _gain2;
	
	delete _tracks[0];
	delete _tracks[1];
	_tracks.resize(0);
	
	T_allocator<chunk_t>::gc();
	T_allocator<chunk_t>::dump_leaks();
	
#if 0
	Tracer::printTrace();
#endif
}

void IOProcessor::CreateTracks()
{
	_tracks.push_back(new AudioTrack<chunk_t>(this, 1, _default_src));
	_tracks.push_back(new AudioTrack<chunk_t>(this, 2, _default_src));

	_filter_controller = new track_t::controller_t(_tracks[0], _tracks[1]);
	if (_midi_controller)
		_midi_controller->SetControlListener(_filter_controller);

	//_bp_filter = new bandpass_filter_td<chunk_t>(_tracks[0], 20.0, 200.0, 48000.0, 48000.0);
	
	//const double gain =
	_gain1 = new gain<T_source<chunk_t> >(_tracks[0]);
	_gain1->set_gain_db(-3.0); // due to certain tracks resampling outside [-1.0,1.0]
	_gain2 = new gain<T_source<chunk_t> >(_tracks[1]);
	_gain2->set_gain_db(-3.0);
	
	configure();
	
	/*
	_master_xfader = new xfader<T_source<chunk_t> >(_tracks[0], _tracks[1]);
	_master_xfader->set_mix(0);
	_cue = new xfader<T_source<chunk_t> >(_tracks[0], _tracks[1]);
	_cue->set_mix(1000);
	
	//const double gain = 1.0;
	_cue->set_gain(1, gain);
	_cue->set_gain(2, gain);
	_master_xfader->set_gain(1, gain);
	_master_xfader->set_gain(2, gain);
	//_aux = new xfader<track_t>(_tracks[0], _tracks[1]);
	*/

	_main_src = _master_xfader;
	_file_src = 0;
}

// TODO Crashes if configure while loading, due to trying to stop twice and start again
void IOProcessor::configure()
{
	auto output1Channel = getChannel(Output1Channel);
	auto output1Stream = output1Channel.stream;
	
	auto output2Channel = getChannel(Output2Channel);
	auto output2Stream = output2Channel.stream;
	
	// should always be an output 1, but not necessarily 2
	// TODO this is not going to work if output1Stream != output2Stream
	assert(output1Stream);
	//assert(!output2Stream || output1Stream == output2Stream);
	
	Stop();
	if (_fileWriter) _fileWriter->stop(); // deletes _fileWriter and _input
	
	if (_gen) _gen->kill();
	delete _gen;
	_gen = 0;
	delete _outputStreamProcessor;
	_outputStreamProcessor = 0;
	delete _inputStreamProcessor;
	_inputStreamProcessor = 0;
	
	const auto bufferSizeFrames = getOutputDevice()->GetDescriptor()->GetBufferSizeFrames();
	printf("BufferSizeFrames = %d\n", bufferSizeFrames);
	_gen = new ChunkGenerator(bufferSizeFrames, &_io_lock);
	_gen->AddChunkSource(_gain1, 1);
	_gen->AddChunkSource(_gain2, 2);
	
	_outputStreamProcessor = new CoreAudioOutputProcessor;
	_outputStreamProcessor->AddOutput(new CoreAudioOutput(output1Stream, _gen, 1, output1Channel.leftChannelIndex, output1Channel.rightChannelIndex));
	
	if (output2Stream)
	{
		_outputStreamProcessor->AddOutput(new CoreAudioOutput(output2Stream, _gen, 2, output2Channel.leftChannelIndex, output2Channel.rightChannelIndex));
	}
	
	output1Stream->SetProc(_outputStreamProcessor);
	if (output2Stream && output2Stream != output1Stream)
	{
		output2Stream->SetProc(_outputStreamProcessor);
	}
	
	_tracks[0]->set_sample_rate_out(output1Stream->GetDescriptor()->GetSampleRate());
	_tracks[0]->set_output_sampling_frequency(output1Stream->GetDescriptor()->GetSampleRate());
	if (output2Stream)
	{
		_tracks[1]->set_sample_rate_out(output2Stream->GetDescriptor()->GetSampleRate());
		_tracks[1]->set_output_sampling_frequency(output2Stream->GetDescriptor()->GetSampleRate());
	}
	
	auto input1Channel = getChannel(Input1Channel);
	auto input1Stream = input1Channel.stream;
	
	if (input1Stream)
	{
		_inputStreamProcessor = new CoreAudioInputProcessor(input1Stream->GetDescriptor());
		_input = new CoreAudioInput(1, input1Channel.leftChannelIndex, input1Channel.rightChannelIndex);
		_fileWriter = new chunk_file_writer(_input, "output.raw");
		_inputStreamProcessor->AddInput(_input);
		input1Stream->SetProc(_inputStreamProcessor);
	}
}

void IOProcessor::Start()
{
	if (!_running)
	{
		_running = true;
		_src_active = true;
		if (_midi_controller)
			_midi_controller->Start();
		auto stream1 = _channels[_configs[Output1Channel]].stream;
		auto device1 = stream1->GetDescriptor()->GetDevice();
		device1->Start();
		auto stream2 = _channels[_configs[Output2Channel]].stream;
		if (stream2)
		{
			auto device2 = stream2->GetDescriptor()->GetDevice();
			if (device1 != device2) device2->Start();
		}
		if (getInputDevice()) getInputDevice()->Start();
	}
}

void IOProcessor::Stop()
{
	if (_running)
	{
		_running = false;
		_src_active = false;
		if (_midi_controller)
			_midi_controller->Stop();
		auto stream1 = _channels[_configs[Output1Channel]].stream;
		auto device1 = stream1->GetDescriptor()->GetDevice();
		device1->Stop();
		auto stream2 = _channels[_configs[Output2Channel]].stream;
		if (stream2)
		{
			auto device2 = stream2->GetDescriptor()->GetDevice();
			if (device1 != device2) device2->Stop();
		}
		if (getInputDevice()) getInputDevice()->Stop();
	}
}

int IOProcessor::get_track_id_for_filter(void *filt) const
{
	return _tracks[0]->get_source() == filt ? 1 : 2;
}

void IOProcessor::trigger_sync_cue(int caller_id)
{
	if (_sync_cue)
	{
		track_t *other_track = GetTrack(caller_id == 1 ? 2 : 1);
		other_track->goto_cuepoint(false);
		other_track->play();
	}
}
