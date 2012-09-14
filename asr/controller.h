#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include <cstdio>

#include "decoder.h"
#include "filter.h"

template <typename Filter_T, typename Decoder_T>
class controller
{
public:
	void process(Filter_T *filt, Decoder_T *dec)
	{
		dec->next();

		if (dec->p_begin.mod != 0.0)
		{
		//	printf("mod %f\n", dec->p_begin.mod);
			double freq = 44100.0 / (dec->p_begin.mod * (44100.0f /48000.0f));
			printf("set freq %f\n", freq);
			set_output_sampling_frequency(filt, freq);
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
		while (!dec->_pos_stream.empty())
		{
			filt->get_root_source()->have_position(dec->_pos_stream.front().chk_ofs, dec->_pos_stream.front().smp, dec->_pos_stream.front().tm);
			dec->_pos_stream.pop();
		}
	}
	void set_output_sampling_frequency(Filter_T *filt, double freq)
	{
		filt->set_output_sampling_frequency((filt->get_output_sampling_frequency() + freq) * 0.5);
	}

	void set_output_time(Filter_T *filt, double t)
	{
		filt->seek_time(t);
	}
};

#endif // !defined(CONTROLLER_H)
