#ifndef _DECODER_H
#define _DECODER_H

#include "asr.h"
#include "filter.h"

typedef unsigned int bits_t;

typedef unsigned int slot_no_t;

struct slot_t {
    unsigned int timecode;
    slot_no_t next; /* next slot with the same hash */
};

struct lut_t {
    struct slot_t *slot;
    slot_no_t *table, /* hash -> slot lookup */
        avail; /* next available slot */
};

typedef unsigned int bits_t;

int lut_init(struct lut_t *lut, int nslots);
void lut_clear(struct lut_t *lut);
void lut_push(struct lut_t *lut, unsigned int timecode);
unsigned int lut_lookup(struct lut_t *lut, unsigned int timecode);

inline bits_t lfsr(bits_t code, bits_t taps)
{
    bits_t taken;
    int xrs;

    taken = code & taps;
    xrs = 0;
    while (taken != 0x0) {
        xrs += taken & 0x1;
        taken >>= 1;
    }

    return xrs & 0x1;
}

inline bits_t fwd(bits_t current)
{
    bits_t l;

    /* New bits are added at the MSB; shift right by one */

    l = lfsr(current, 0x041040 | 0x1);
    return (current >> 1) | (l << (23 - 1));
}

inline bits_t rev(bits_t current)
{
    bits_t l, mask;

    /* New bits are added at the LSB; shift left one and mask */

    mask = (1 << 23) - 1;
    l = lfsr(current, (0x041040 >> 1) | (0x1 << (23 - 1)));
    return ((current << 1) & mask) | l;
}

template <typename Sample_T, typename Chunk_T, int chunk_size, int fft_size=1024>
class peak_detector;

template <typename Chunk_T, int chunk_size, int fft_size>
class peak_detector<SamplePairf, Chunk_T, chunk_size, fft_size> : public T_source<Chunk_T>, public T_sink<Chunk_T>
{
public:
	typedef typename Chunk_T chunk_t;
	struct pk_info
	{
		float val;
		int ch;
		smp_ofs_t smp;
		int chk_ofs;
		bool forward;
	};
	struct pk_dec_info
	{
		bool hi;
		int ch;
		bool forward;
	};
	struct bit_info
	{
		bool bit;
		bool forward;
		smp_ofs_t smp;
		int chk_ofs;
	};
	struct pos_info
	{
		double tm;
		smp_ofs_t smp;
		int chk_ofs;
		double freq;
		double mod;
		bool forward;
		bool valid;
		bool sync_time;
	};

	const static int resampler_taps = 13;

	dumb_resampler<double, double> _rs;

	peak_detector(T_source<Chunk_T> *src) :
		T_sink(src),
		_rs(resampler_taps),
		pk_out("peaks.txt"),
		_smp(0)
	{
		_max[0] = 0.0f;
		_max[1] = 0.0f;
	//	sync = false;
		lut_init(&_lut, 2110000);
		bits_t current, last;
		current = 0x32066c;
		for (unsigned n = 0; n < 2110000; n++) {
			/* timecode must not wrap */
			assert(lut_lookup(&_lut, current) == (unsigned)-1);
			lut_push(&_lut, current);
			last = current;
			current = fwd(current);
			assert(rev(current) == last);
		}
		_bitstream = 0;
		_timecode = 0;
		_valid_counter = 0;

		_inBuf = _inEnd = (float*)fftwf_malloc(sizeof(SamplePairf) * fft_size);
		_outBuf = _outEnd = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (fft_size*4));
		_plan = fftwf_plan_dft_r2c_2d(fft_size, 2, _inBuf, _outBuf, FFTW_MEASURE);

	//	_little_fft_size = 256;
	//	_little_fft_buf = (float*)fftwf_malloc(sizeof(SamplePairf) * _little_fft_size);
	//float *_little_fft_end = _little_fft_buf + 
	//float *_little_fft_ptr;
	//	_little_fft_buf = 

	/*	p_begin.tm = 2.0;
		p_begin.forward = true;
		p_begin.freq = 2000.0;
		p_begin.mod = 1.0;
		p_begin.valid = false;*/
	}
	~peak_detector()
	{
		fftwf_destroy_plan(_plan);
		fftwf_free(_inBuf);
		fftwf_free(_outBuf);
	}
	double fft_find_frequency()
	{
		float x, y, mag, maxL=0.0f, maxR=0.0f, phL, phR;
		double y1, y2, y3, dbin, fc, fcR, fcL;
		/*if (data) memcpy(_inBuf, data, sizeof(float) * chunk_size * 2);*/
		fftwf_execute(_plan);
		int i, maxLpos, maxRpos;

		double magsL[fft_size/2+1];
		double magsR[fft_size/2+1];

		for (i=0;i<(fft_size/2+1);++i)
		{
			x = _outBuf[i*2][0];
			y = _outBuf[i*2][1];
			magsL[i] = sqrt(x*x+y*y);
			if (magsL[i] > maxL)
			{
				maxL = magsL[i];
			//	phL = phase(x,y);
				maxLpos = i;
			}
			x = _outBuf[i*2+1][0];
			y = _outBuf[i*2+1][1];
			magsR[i] = sqrt(x*x+y*y);
			if (magsR[i] > maxR)
			{
				maxR = magsR[i];
			//	phR = phase(x,y);
				maxRpos = i;
			}
		}
	//	fc = ((double(maxLpos))*(SAMPLERATE/fft_size)+(double(maxRpos))*(SAMPLERATE/fft_size))*0.5;
#if 0
		// v1: linear interpolate
		y1 = magsL[maxLpos-1];
		y2 = magsL[maxLpos];
		y3 = magsL[maxLpos+1];
		dbin = ((y3-y1)/(y1+y2+y3));
		fcL = (double(maxLpos)+dbin)*(SAMPLERATE/fft_size);
		y1 = magsR[maxRpos-1];
		y2 = magsR[maxRpos];
		y3 = magsR[maxRpos+1];
		dbin = ((y3-y1)/(y1+y2+y3));
		fcR = (double(maxRpos)+dbin)*(SAMPLERATE/fft_size);
		fc = (fcL+fcR)*0.5;
#else
		// v2: resample DTFT -> DFT
		double *buf = _rs.get_tap_buffer(), *buf_end = buf+resampler_taps;
		
		int tap = maxLpos - resampler_taps/2;
		int last_tap = tap + resampler_taps;
		
		while (buf < buf_end)
		{
			*buf++ = (tap < 0 || tap > fft_size/2) ? 0.0 : magsL[tap];
			++tap;
		} 

		double max = 0.0;
		double maxpos = 0.0;
		for (double d=-7.0; d < -5.0; d += 0.001) {
			double x = _rs.apply(d);
			if (x > max) 
			{
				max = x;
				maxpos = d;
			}
		}
		double cntr = -maxpos - 6.0;
		//if (cntr < -1.0 || cntr > 1.0) {
		//	printf("L cntr %f +maxLpos %f\n", cntr, double(maxLpos)+cntr);
		//}
		fcL = (double(maxLpos)+cntr)*(SAMPLERATE/fft_size);
		

		buf = _rs.get_tap_buffer(), buf_end = buf+resampler_taps;
		tap = maxRpos - resampler_taps/2;
		last_tap = tap + resampler_taps;
		
		while (buf < buf_end)
		{
			*buf++ = (tap < 0 || tap > fft_size/2) ? 0.0 : magsR[tap];
			++tap;
		} 

		 max = 0.0;
		 maxpos = 0.0;
		for (double d=-7.0; d < -5.0; d += 0.001) {
			double x = _rs.apply(d);
			if (x > max) 
			{
				max = x;
				maxpos = d;
			}
		}
		 cntr = -maxpos - 6.0;
	//	 if (cntr < -1.0 || cntr > 1.0) {
		//	printf("R cntr %f +maxRpos %f\n", cntr, double(maxRpos)+cntr);
	//	}
		 fcR = (double(maxRpos)+cntr)*(SAMPLERATE/fft_size);
		// printf("maxLpos %d maxRpos %d fcL %f fcR %f\n", maxLpos, maxRpos, fcL, fcR);
		 fc = (fcL+fcR)*0.5;
		 // ???
		// printf("fcL == %f fcR == %f fc == %f maxL == %f maxposL == %d\n", fcL, fcR, fc, maxL, maxLpos);
#endif

		//printf("_speed %f fc %f magL %f magR %f phaseL - phaseR %f\n", 1000.0/fc, fc, maxL, maxR, phL- phR);
		if (maxL < 10.0f)
			return 0.0;
		return fc;
	}

	void process_samples(Chunk_T *chk)
	{
		bool forward;
		int ch = 0;
		int smp_ofs = 0;
		for (SamplePairf *ptr = chk->_data, *end = ptr + chunk_size; ptr != end; ++ptr, ++_smp, ++smp_ofs)
		{
		//	for (int ch = 0; ch <= 0; ++ch) // only care about 1 channel, cant error correct anyway
			{
				if (_max[ch] > 0.0f)
				{
					if ((*ptr)[ch] > _max[ch])
						_max[ch] = (*ptr)[ch];
					else if ((*ptr)[ch] < 0.0f)
					{
						if (ch == 1)
						{
							forward = (*ptr)[0] > 0.0f;
						}
						else
						{
							forward = (*ptr)[1] < 0.0f;
						}
						if (ch==0)
						{
							pk_info pi = {_max[ch], ch, _smp, smp_ofs, forward};
							_pks.push(pi);
						}
						if (_max[ch] > _rng_max[ch])
							_rng_max[ch] = _max[ch];
						if (_max[ch] < _rng_min[ch])
							_rng_min[ch] = _max[ch];
						//pk_out << "{" << max[i] << ", " << i << "} ";
						_max[ch] = (*ptr)[ch];
					}
				}
				else
				{
					if ((*ptr)[ch] < _max[ch])
						_max[ch] = (*ptr)[ch];
					else if ((*ptr)[ch] > 0.0f)
					{
						if (ch == 1)
						{
							forward = (*ptr)[0] < 0.0f;
						}
						else
						{
							forward = (*ptr)[1] > 0.0f;
						}
						/*
						pk_info pi = {max[i], i};
						pks.push(pi);
						if (-max[i] > rng_max[i])
							rng_max[i] = -max[i];
						if (-max[i] < rng_min[i])
							rng_min[i] = -max[i];
						//pk_out << "{" << max[i] << ", " << i << "} ";
						*/
						_max[ch] = (*ptr)[ch];
					}
				}
			}
		}
	}

	void decode_bits()
	{
		_rng_mid[0] = (_rng_max[0] + _rng_min[0]) * 0.5f;
		_rng_mid[1] = (_rng_max[1] + _rng_min[1]) * 0.5f;
		bit_info b;
		while (_pks.size() > 0)
		{
			pk_info pi = _pks.front();
			_pks.pop();
		//	pk_dec_info pdi = {fabs(pi.val) > rng_mid[pi.ch], pi.ch, pi.forward};
		//	pks_dec.push(pdi);
			bit_info b = {fabs(pi.val) > _rng_mid[pi.ch], pi.forward, pi.smp, pi.chk_ofs};
			_bits.push(b);
		//	pk_out << "{" << pdi.hi << ", " << pdi.ch << "} ";
		}

		//	pk_dec_info last;
		
		// sync
		/*
		while (!sync && pks_dec.size() > 1)
		{
			do
			{
				last = pks_dec.front();
				pks_dec.pop();
			} while (last.hi == pks_dec.front().hi && pks_dec.size() > 1);
			if (pks_dec.size() < 2) break;
			last = pks_dec.front();
			pks_dec.pop();
			if (last.hi == pks_dec.front().hi && last.ch != pks_dec.front().ch)
				sync=true;
		}*/
		/*while (pks_dec.size() > 1)
		{
			last = pks_dec.front();
			pks_dec.pop();
			if (last.ch == 1)
				break;
		}*/
		/*if (last.hi != pks_dec.front().hi)
		{
			last = pks_dec.front();
			pks_dec.pop();
		}*/
		/*
		if (pks_dec.size() > 1)
		{
			last = pks_dec.front();
			pks_dec.pop();
		}
		if (pks_dec.size() > 0 && last.hi != pks_dec.front().hi)
		{
			last = pks_dec.front();
			pks_dec.pop();
		}
		*/
	/*	b.bit = last.hi;
		b.forward = last.forward;
		//b.forward = (last.ch == 1 && pks_dec.front().ch == 0);
		bits.push(b);
		while (pks_dec.size() > 2) // makes duplicate bits
		{
			pks_dec.pop();
			last = pks_dec.front();
			pks_dec.pop();
			if (last.hi == pks_dec.front().hi)
			{
				if (last.ch != pks_dec.front().ch)
				{
					b.bit = last.hi;
					//b.forward = (last.ch == 1 && pks_dec.front().ch == 0);
					b.forward = last.forward;
					bits.push(b);
				}
			}
			else
			{
				last = pks_dec.front();
				pks_dec.pop();
				if (pks_dec.size() > 0 && last.ch != pks_dec.front().ch)
				{
					b.bit = last.hi;
					b.forward = last.forward; //(last.ch == 1 && pks_dec.front().ch == 0);
					bits.push(b);
				}
			}
		}
		*/
	}

	void decode_stream()
	{
		signed int r;
		bool valid = false;
		int tm_to_valid = 0;
		
	//	pk_out << std::hex;
		while (_bits.size() > 0)
		{
		//	for (int i = 0; i < 23; ++i)
			{
				bit_info b = _bits.front();
				_bits.pop();
			//	bits.pop();
			//	pk_out << "bit is " << b.bit << " " << (b.forward ? "f" : "b") << std::endl;
				//bit_reg_l = (bit_reg_l >> 1) | ((unsigned int)b.bit << 22);
				//pk_out << "reg_l is " << bit_reg_l << std::endl;
				if (b.forward) {
					_timecode = fwd(_timecode);
					_bitstream = (_bitstream >> 1) + ((bits_t)b.bit << (23 - 1));
				} else {
					bits_t mask;
					mask = ((1 << 23) - 1);
					_timecode = rev(-_timecode);
					_bitstream = ((_bitstream << 1) & mask) + (bits_t)b.bit;
				}
			//	pk_out << "timecode " << timecode << " bitstream " << bitstream << std::endl;
				if (_timecode == _bitstream)
					_valid_counter++;
				//pk_out << "hello " << timecode << std::endl;
				else {
					_timecode = _bitstream;
					_valid_counter = 0;
				}
				r = lut_lookup(&_lut, _bitstream);

				if (r >= 0) {
				//if (when)
				//    *when = tc->timecode_ticker * tc->dt;
				//return r;
				//	printf("valid %d\n", _valid_counter);
					if (_valid_counter > 23)
					{
					//pk_out << "hello "<< r<<std::endl;
						pos_info p;
						//if (p_begin.mod != 0.0)
						{
							p.tm = r/2110000.0*(17*60);
							p.smp = b.smp;
							p.chk_ofs = b.chk_ofs;
							p.forward = b.forward;
							p.valid = true;
							_pos_stream.push_back(p);
							
						}
						/*
						if (!valid) // only once per frame for now.
						{
							valid = true;
							p_begin = p;
							p_begin.tm -= tm_to_valid / freq;
							pk_out << p_begin.tm << ' ' << p_begin.forward << ' ' << p_begin.freq << ' ' << p_begin.mod << std::endl;
						}*/
					}
				} else {
					//gone bad
					_valid_counter = 0;
				}
			//	if (!valid)
			//		++tm_to_valid;
			}
		//	pk_out << bit_reg_l << ' ';
		}
	}

	Chunk_T *next()
	{
		//absolute values
		_rng_min[0] = 1.0f;
		_rng_min[1] = 1.0f;
		_rng_max[0] = 0.0f;
		_rng_max[1] = 0.0f;
		
		Chunk_T *chk = _src->next();

		SamplePairf *copy_from = chk->_data, *copy_end = copy_from+chunk_size;

		//memcpy(_inBuf, chk->_data, sizeof(SamplePairf) * chunk_size);

		int to_write = fft_size;
		double new_freqs[chunk_size/fft_size];
		for (int f_indx = 0; f_indx < chunk_size/fft_size; ++f_indx) {
			memcpy(_inBuf, copy_from, sizeof(SamplePairf)*to_write);
			copy_from += to_write;
			double f = fft_find_frequency();
			//if (f != 0.0/* && f > 1800.0 && f < 2200.0*/)
				new_freqs[f_indx] = f;
		//	printf("indx[%d] : %f\n", f_indx, freqs[f_indx]);
		}

		_pos_stream.clear();
		
		process_samples(chk);

		decode_bits();
	
		decode_stream();

		int f_indx = 0;
		if (_pos_stream.empty())
		{
			for (int f_indx = 0; f_indx < chunk_size/fft_size; ++f_indx) {
				pos_info p;
				p.forward = true;
				p.sync_time = false;
				p.chk_ofs = f_indx*fft_size;
				if (new_freqs[f_indx] > 0.0)
					p.mod = new_freqs[f_indx]/2000.0;///990.0;///2020.0;
				else
					p.mod = 0.0;
				p.freq = p.mod > 0.0 ? 48000.0 / p.mod : 0.0;
				_pos_stream.push_back(p);
			}
		}
		else 
		{
			for (std::deque<pos_info>::iterator i = _pos_stream.begin(); i != _pos_stream.end(); i++)
			{
				const int indx = i->chk_ofs/fft_size;
				double freq = new_freqs[indx], mod;
				/*if (indx < 0)
				{
					indx = chunk_size/fft_size - 1;
					freq += _freqs[indx--];
					freq += _freqs[indx--];
					freq += _freqs[indx--];
				}
				else
				{
					freq += new_freqs[indx--];
					if (indx < 0)
					{
						indx = chunk_size/fft_size - 1;
						freq += _freqs[indx--];
						freq += _freqs[indx--];
					}
					else
					{
						freq += new_freqs[indx--];
						if (indx < 0)
						{
							indx = chunk_size/fft_size - 1;
							freq += _freqs[indx];
						}
						else
							freq += new_freqs[indx];
					}
				}

				freq /= 4;

				for (int j=0; j<chunk_size/fft_size; ++j)
					_freqs[j] = new_freqs[j];*/

				if (freq > 0.0)
					mod = freq/2000.0;
				else
					mod = 0.0;
				i->freq = mod > 0.0 ? 48000.0 / mod : 0.0;
				i->mod = mod;
			}
		}

		
	//	p_begin.freq = freq;
	//	p_begin.mod = mod;

		return chk;
	}
	SamplePairf _max;
	SamplePairf _rng_min;
	SamplePairf _rng_max;
	SamplePairf _rng_mid;
	std::queue<pk_info> _pks;
	//std::queue<pk_dec_info> pks_dec;
	std::queue<bit_info> _bits;
	std::deque<pos_info> _pos_stream;
	std::ofstream pk_out;
	bits_t _bitstream, _timecode, _valid_counter;
	//bool sync;
	float *_inBuf, *_inEnd, *_inPtr;
	fftwf_complex *_outBuf, *_outEnd;
	fftwf_plan _plan;
	lut_t _lut;
	smp_ofs_t _smp; // handle overflow of this thing

	int _bit_smp_list[24];

	double _freqs[chunk_size/fft_size];
	
	//int _little_fft_size;
	//float *_little_fft_buf;
	//float *_little_fft_end;
	//float *_little_fft_ptr;
};

#endif // !defined(DECODER_H)
