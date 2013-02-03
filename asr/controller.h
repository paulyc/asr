#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include <cstdio>

#include "decoder.h"
#include "dsp/filter.h"
#include "midi.h"

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

template <typename Filter_T>
class FilterController : public IControlListener
{
public:
//	FilterController(ASIOProcessor *io) : _io(io) {}
	FilterController();

	virtual void HandlePlayPause(int channel, float64_t time);
	virtual void HandleCue(int channel, float64_t time);
	virtual void HandleSetPitch(int channel, float64_t time, float64_t pitch);
	virtual void HandleBendPitch(int channel, float64_t time, float64_t dpitch);

	struct ControlEvent
	{
		int channel;
		float64_t time;
		float64_t fparam1;
		float64_t fparam2;
		int32_t   iparam;
	};
	const static int MAX_EVENTS_PER_CHANNEL = 0x100;
	const static int NUM_CHANNELS = 2;

	ControlEvent *nextEvent(int ch);
private:
	struct ControlChannel
	{
		ControlChannel() : eventWrite(events), eventRead(events) {}
		ControlEvent events[MAX_EVENTS_PER_CHANNEL];
		ControlEvent *eventWrite;
		ControlEvent *eventRead;
	};
	ControlChannel _channels[NUM_CHANNELS];
//	ASIOProcessor *_io;
};

template <typename Filter_T, typename Decoder_T>
class controller : public T_sink<typename Filter_T::chunk_t>
{
public:
	controller(Decoder_T *dec) : _decoder(dec) {}
	void process()
	{
		T_allocator<Decoder_T::chunk_t>::free(_decoder->next());
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
