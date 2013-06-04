// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo <paul.ciarlo@gmail.com>
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

#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include <cstdio>

#include "midi.h"
#include "lock.h"

class MagicController
{
public:
	MagicController(ASIOProcessor *io) : _io(io), _magic_count(-1) {}
	
	void do_magic();
	void next_time(double t, int t_id);
protected:
	ASIOProcessor *_io;
	int _magic_count;
	double _magic_times[4];
	int _first_track;
	int _second_track;
};

class ITrackController
{
public:
	virtual void set_pitch(double) = 0;
	virtual void bend_pitch(double) = 0;
	virtual void goto_cuepoint(bool) = 0;
	virtual bool play_pause(bool) = 0;
	virtual void seek_time(double) = 0;
};

template <typename Filter_T>
class FilterController : public IControlListener
{
public:
//	FilterController(ASIOProcessor *io) : _io(io) {}
	FilterController(ITrackController *t1, ITrackController *t2)
	{
		_tracks[0] = t1;
		_tracks[1] = t2;
	}

	virtual void HandlePlayPause(int channel, float64_t time)
	{
		_tracks[channel]->play_pause(true);
		/*
		pthread_mutex_lock(&_lock);
		ControlEvent &cev = *_channels[channel].eventWrite++;

		if (_channels[channel].eventWrite >= _channels[channel].events + MAX_EVENTS_PER_CHANNEL)
			_channels[channel].eventWrite = _channels[channel].events;
		++_channels[channel].numEvents;
		if (_channels[channel].numEvents >= MAX_EVENTS_PER_CHANNEL) printf("help!!\n");
		cev.ev = IMIDIController::PlayPauseEvent;
		cev.time = time;
		pthread_mutex_unlock(&_lock);
		*/
	}
	virtual void HandleCue(int channel, float64_t time)
	{
		_tracks[channel]->goto_cuepoint(true);
		/*
		pthread_mutex_lock(&_lock);
		ControlEvent &cev = *_channels[channel].eventWrite++;
		if (_channels[channel].eventWrite >= _channels[channel].events + MAX_EVENTS_PER_CHANNEL)
			_channels[channel].eventWrite = _channels[channel].events;
		++_channels[channel].numEvents;
		if (_channels[channel].numEvents >= MAX_EVENTS_PER_CHANNEL) printf("help!!\n");
		cev.ev = IMIDIController::CueEvent;
		cev.time = time;
		pthread_mutex_unlock(&_lock);
		*/
	}
	virtual void HandleSetPitch(int channel, float64_t time, float64_t pitch)
	{
		printf("channel %d time %f pitch %f\n", channel, time, pitch);
		_tracks[channel]->set_pitch(pitch);
		/*
		pthread_mutex_lock(&_lock);
		ControlEvent &cev = *_channels[channel].eventWrite++;
		if (_channels[channel].eventWrite >= _channels[channel].events + MAX_EVENTS_PER_CHANNEL)
			_channels[channel].eventWrite = _channels[channel].events;
		++_channels[channel].numEvents;
		if (_channels[channel].numEvents >= MAX_EVENTS_PER_CHANNEL) printf("help!!\n");
		cev.ev = IMIDIController::SetPitchEvent;
		cev.time = time;
		cev.fparam1 = pitch;
		pthread_mutex_unlock(&_lock);
		*/
	}
	virtual void HandleBendPitch(int channel, float64_t time, float64_t dpitch)
	{
		_tracks[channel]->bend_pitch(dpitch);
		/*
		pthread_mutex_lock(&_lock);
		ControlEvent &cev = *_channels[channel].eventWrite++;
		if (_channels[channel].eventWrite >= _channels[channel].events + MAX_EVENTS_PER_CHANNEL)
			_channels[channel].eventWrite = _channels[channel].events;
		++_channels[channel].numEvents;
		if (_channels[channel].numEvents >= MAX_EVENTS_PER_CHANNEL) printf("help!!\n");
		cev.ev = IMIDIController::BendPitchEvent;
		cev.time = time;
		cev.fparam1 = dpitch;
		pthread_mutex_unlock(&_lock);
		*/
	}

	virtual void HandleSeekTrack(int channel, float64_t time, bool backward)
	{
		if (backward)
			_tracks[channel]->seek_time(0.0);
	}

	struct ControlEvent
	{
		IMIDIController::Event ev;
		float64_t time;
		float64_t fparam1;
		float64_t fparam2;
		int32_t   iparam;
	};
	const static int MAX_EVENTS_PER_CHANNEL = 0x1000;
	const static int NUM_CHANNELS = 10;

	ControlEvent* nextEvent(int ch)
	{
		return 0;
		/*
		ControlEvent *cev = 0;
		pthread_mutex_lock(&_lock);
		if (_channels[ch].numEvents > 0)
		{
			cev = _channels[ch].eventRead++;
			if (_channels[ch].eventRead >= _channels[ch].events + MAX_EVENTS_PER_CHANNEL)
				_channels[ch].eventRead = _channels[ch].events;
			--_channels[ch].numEvents;
		}
		pthread_mutex_unlock(&_lock);
		return cev;
		*/
	}
private:
	struct ControlChannel
	{
		ControlChannel() : eventWrite(events), eventRead(events), numEvents(0) {}
		ControlEvent events[MAX_EVENTS_PER_CHANNEL];
		ControlEvent *eventWrite;
		ControlEvent *eventRead;
		int numEvents;
	};
	ControlChannel _channels[NUM_CHANNELS];
//	ASIOProcessor *_io;
	PthreadLock _lock;

	ITrackController *_tracks[2];
};

template <typename Filter_T, typename Decoder_T>
class controller : public T_sink<typename Filter_T::chunk_t>
{
public:
	controller(Decoder_T *dec) : _decoder(dec) {}
	void process()
	{
		T_allocator<typename Decoder_T::chunk_t>::free(_decoder->next());
	}
	void process(Filter_T *filt, Filter_T *filt2)
	{
	//	if (dec->p_begin.mod != 0.0)
		{
		//	printf("mod %f\n", dec->p_begin.mod);
	//		double freq = 44100.0 / (dec->p_begin.mod * (44100.0f /48000.0f));
	//		printf("set freq %f\n", freq);
	//		set_output_sampling_frequency(filt, freq);
		}
		/*if (dec->p_begin.valid)
		{
			double pos = dec->p_begin.tm - 2.0;
			pos -= dec->p_begin.smp * 1.0/48000.0;
			printf("set output time %f\n", pos);
			set_output_time(filt, pos);
			//p_begin.forward = b.forward;
			dec->p_begin.valid = false;
		}*/
		//filt->use_decoder(dec);
	/*	GenericUI *ui = ASR::get_io_instance()->get_ui();
		while (!dec->_pos_stream.empty())
		{
			double mod = dec->_pos_stream.front().mod;
			if (filt)
			{
				dec->_pos_stream.front().sync_time = ui->get_sync_time(1);
				if (ui->get_add_pitch(1))
				{
					dec->_pos_stream.front().mod = mod + ui->_track1.get_pitch(); 
					dec->_pos_stream.front().freq = 48000.0 / dec->_pos_stream.front().mod;
				}
				filt->have_position(dec->_pos_stream.front());
			}
			if (filt2)
			{
				dec->_pos_stream.front().sync_time = ui->get_sync_time(2);
				if (ui->get_add_pitch(2))
				{
					dec->_pos_stream.front().mod = mod + ui->_track2.get_pitch(); 
					dec->_pos_stream.front().freq = 48000.0 / dec->_pos_stream.front().mod;
				}
				filt2->have_position(dec->_pos_stream.front());
			}
			dec->_pos_stream.pop_front();
		}*/
	}
	void set_output_sampling_frequency(Filter_T *filt, double freq)
	{
		filt->set_output_sampling_frequency((filt->get_output_sampling_frequency() + freq) * 0.5);
	}

	void set_output_time(Filter_T *filt, double t)
	{
		filt->seek_time(t);
	}

	Decoder_T *_decoder;
};

#endif // !defined(CONTROLLER_H)
