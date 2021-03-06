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

#ifndef _FFT_H
#define _FFT_H

#include "../type.h"
#include "../stream.h"
#include "../malloc.h"
#define _USE_MATH_DEFINES
#include <math.h>

double sinc(double x);

double I_0(double z);

// -1.0 <= t <= 1.0
double kaiser(double t, double beta);

// 0 <= n < N
double mlt(int n, int N);

class FFTPlan
{
public:
	FFTPlan() : _plan(NULL) {}
	FFTPlan(fftw_plan p) : _plan(p) {}
	virtual ~FFTPlan()
	{
		fftw_destroy_plan(_plan);
	}
	void execute()
	{
		fftw_execute(_plan);
	}
protected:
	fftw_plan _plan;
};



// in is at least N SamplePairfs (2N floats)
// out is at least ... ComplexPairfs (... floats)
class Time2FrequencyPlan : public FFTPlan
{
public:
	Time2FrequencyPlan(int N, SamplePaird *in, ComplexPaird *out) 
	{
		const int n[] = {N};
		_plan = fftw_plan_many_dft_r2c(1, n, 2, (double*)in, NULL, 2, 1, (fftw_complex*)out, NULL, 2, 1, FFTW_MEASURE);
	}
};


// overwrites input array
// Frequency2Time(Time2Frequency(x(n))) = N * x(n) for all 0 <= n < N
// out is at least N SamplePairfs (2N floats)
// in is at least ... ComplexPairfs (... floats)
class Frequency2TimePlan : public FFTPlan
{
public:
	Frequency2TimePlan(int N, ComplexPaird *in, SamplePaird *out)
	{
		const int n[] = {N};
		_plan = fftw_plan_many_dft_c2r(1, n, 2, (fftw_complex*)in, NULL, 2, 1, (double*)out, NULL, 2, 1, FFTW_MEASURE);
	}
};

class FFT
{
public:
	FFT(int N)
	{
		_timeBuf = (SamplePaird*)fftw_malloc(N * sizeof(SamplePaird));
		_freqBuf = (ComplexPaird*)fftw_malloc((N/2+1) * sizeof(ComplexPaird));

		_forwardPlan = new Time2FrequencyPlan(N, _timeBuf, _freqBuf);
		_inversePlan = new Frequency2TimePlan(N, _freqBuf, _timeBuf);
	}

	virtual ~FFT()
	{
		delete _inversePlan;
		delete _forwardPlan;

		fftw_free(_timeBuf);
		fftw_free(_freqBuf);
	}
private:
	SamplePaird *_timeBuf;
	ComplexPaird *_freqBuf;

	FFTPlan *_forwardPlan;
	FFTPlan *_inversePlan;
};

class FFTWindowFilter
{
public:
	FFTWindowFilter(int N, double beta=8.0) : _N(N), _beta(beta), _initd(false)
	{
		_coeffs = (double*)fftw_malloc(sizeof(double) * _N);
		_fftCoeffs = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (_N/2+1));
	}
	virtual ~FFTWindowFilter()
	{
		fftw_free(_coeffs);
		fftw_free(_fftCoeffs);
	}
	virtual double h_n(int n) = 0;
	void fill()
	{
		double t = -1.0;
		const double dt = 2.0 / _N;
		for (int i=0; i<_N; ++i)
		{
			_coeffs[i] = h_n(i-_N/2) * kaiser(t, _beta);
		//	_coeffs[i] = i == 0?1.0:0.0;
		//	printf("coeff[%d] = %f\n", i, _coeffs[i]);
			t += dt;
		}
	}
	virtual void init()
	{
		fftw_plan p = fftw_plan_dft_r2c_1d(_N, _coeffs, _fftCoeffs, FFTW_MEASURE);
		//fftw_plan p = fftw_plan_dft_1d(_N, 
		fill();
		fftw_execute(p);
		fftw_destroy_plan(p);
		_initd = true;
		
		for (int i=0; i<_N/2+1; ++i)
		{
			double mag = sqrt(_fftCoeffs[i][0] * _fftCoeffs[i][0] + _fftCoeffs[i][1] * _fftCoeffs[i][1]);
		//	double normalize = 1.0/mag;
		//	printf("fftCoeff[%d] = %f + j%f (%f)\n", i, _fftCoeffs[i][0], _fftCoeffs[i][1], mag);
		//	_fftCoeffs[i][0] *= normalize;
		//	_fftCoeffs[i][1] *= normalize;
		}
	}
	fftw_complex *get_fft_coeffs() const
	{
		if (!_initd) throw std::runtime_error("attempt to use filter without init");
		return _fftCoeffs;
	}
	static const double _windowConstant;
protected:
	int _N;
	double _beta;
	double *_coeffs;
	fftw_complex *_fftCoeffs;
	bool _initd;
};

class KaiserWindowFilter : public FFTWindowFilter
{
public:
	KaiserWindowFilter(int N, double beta=8.0) : FFTWindowFilter(N, beta) {}
	double h_n(int n) { return 1.0; }
};

class AllPassFilter : public FFTWindowFilter
{
public:
	AllPassFilter(int N) : FFTWindowFilter(N) {}
	double h_n(int n) { return sinc(n); }
};

// 65536 * 8 = 524kb / filter
// comb filter 130 => 60/130 * 2 * 44100 = 40707
//			   129 =>					   41023
inline double bpm_to_frequency_hz(double bpm)
{
	return bpm / 60.0;
}

inline double bpm_to_period_s(double bpm)
{
	return 60.0 / bpm;
}

inline double bpm_to_interval_samples(double bpm, double sampleRate=44100.0)
{
	return bpm_to_period_s(bpm) * sampleRate;
}

class CombFilter : public FFTWindowFilter
{
public:
	CombFilter(int N, int interval) :
	FFTWindowFilter(N), _interval(interval)
	{
	}
	virtual double h_n(int n)
	{
		if (abs(n) % _interval == 0)
			return 1.0;
		return 0.0;
	}
private:
	int _interval;
};

class LPFilter : public FFTWindowFilter
{
public:
	LPFilter(int N, double sampleRate, double cutoff) : 
	  FFTWindowFilter(N), _sampleRate(sampleRate), _cutoff(cutoff)
	  {
		  _f_c = _cutoff / _sampleRate;
		  _2_f_c = 2.0 * _f_c;
	  }
	virtual double h_n(int n)
	{
		if (n == 0)
			return _2_f_c;
		return _2_f_c*sinc(_2_f_c*n);
	}
	double sinc(double x)
	{
		 if (x == 0.0)
			 return 1.0;
		 else
			 return sin(M_PI*x)/(M_PI*x);
	 }
private:
	double _sampleRate;
	double _cutoff;
	double _f_c;
	double _2_f_c;
};

class BPFilter : public FFTWindowFilter
{
public:
	BPFilter(int N, double sampleRate, double cutoffLow, double cutoffHigh) :
	FFTWindowFilter(N), _sampleRate(sampleRate), _cutoffLow(cutoffLow), _cutoffHigh(cutoffHigh)
	{
		_f_c_low = _cutoffLow / _sampleRate;
		_f_c_high = _cutoffHigh / _sampleRate;
	}
	double h_n(int n) { return 0.0; } // dont use this
	virtual void init()
	{
		fftw_complex *tmpCoeffs = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (_N/2+1));
		fftw_plan p = fftw_plan_dft_r2c_1d(_N, _coeffs, _fftCoeffs, FFTW_MEASURE);
		
		const double dt = 2.0 / _N;
		double t = -1.0;
		for (int i=0; i<_N; ++i)
		{
			_coeffs[i] = h_lowpass(i-_N/2) * kaiser(t, _beta);
			t += dt;
		}
		fftw_execute(p);
		for (int i=0; i<_N/2+1; ++i)
		{
			tmpCoeffs[i][0] = _fftCoeffs[i][0];
			tmpCoeffs[i][1] = _fftCoeffs[i][1];
	   //	  printf("tmpCoeffs[%d] = %f + j%f\n", i, tmpCoeffs[i][0], tmpCoeffs[i][1]);
		}
		t = -1.0;
		for (int i=0; i<_N; ++i)
		{
			_coeffs[i] = h_highpass(i-_N/2) * kaiser(t, _beta);
			t += dt;
		}
		fftw_execute(p);
		for (int i=0; i<_N/2+1; ++i)
		{
		 //	  printf("hpCoeffs[%d] = %f + j%f\n", i, _fftCoeffs[i][0], _fftCoeffs[i][1]);
			// real = ac - bd
			_fftCoeffs[i][0] = _fftCoeffs[i][0] * tmpCoeffs[i][0] - _fftCoeffs[i][1] * tmpCoeffs[i][1];
			// imag = ad + bc
			_fftCoeffs[i][1] = _fftCoeffs[i][0] * tmpCoeffs[i][1] + _fftCoeffs[i][1] * tmpCoeffs[i][0];
		}
		for (int i=0; i<_N/2+1; ++i)
		{
			double mag = sqrt(_fftCoeffs[i][0] * _fftCoeffs[i][0] + _fftCoeffs[i][1] * _fftCoeffs[i][1]);
			//	double normalize = 1.0/mag;
		//	printf("fftCoeff[%d] = %f + j%f (%f)\n", i, _fftCoeffs[i][0], _fftCoeffs[i][1], mag);
			//	_fftCoeffs[i][0] *= normalize;
			//	_fftCoeffs[i][1] *= normalize;
		}
		fftw_destroy_plan(p);
		fftw_free(tmpCoeffs);
		_initd = true;
	}
	double h_lowpass(int n) // lowpass filter at high frequency
	{
		const double _2fc = 2.0 * _f_c_high;
		if (n == 0)
			return _2fc;
		return _2fc*sinc(_2fc*n);
	}
	double h_highpass(int n) // highpass filter at low frequency
	{
		const double _2fc = 2.0 * _f_c_low;
		if (n == 0)
			return 1.0 - _2fc;
		return -_2fc*sinc(_2fc*n);
	}
	double sinc(double x)
	{
		if (x == 0.0)
			return 1.0;
		else
			return sin(M_PI*x)/(M_PI*x);
	}
private:
	double _sampleRate;
	double _cutoffLow;
	double _cutoffHigh;
	double _f_c_low;
	double _f_c_high;
};

class TestSource : public T_source<chunk_t>
{
public:
	TestSource() : _t(0.0) {}
	chunk_t *next()
	{
		const double dt = 1.0/44100.0;
		chunk_t *chk = T_allocator<chunk_t>::alloc();
		for (SamplePairf *p = chk->_data; p < chk->_data + chunk_t::chunk_size; ++p)
		{
			const double val = sin(2*M_PI*1000.0*_t);
			(*p)[0] = (float32_t)val;
			(*p)[1] = (float32_t)val;
			_t += dt;
		}
		return chk;
	}
private:
	double _t;
};

class FilterBank;

class IFilterBankProcessor
{
public:
	virtual void processFFT(ComplexPaird * const fftBuffer) = 0;
};

class FilterBank
{
public:
	FilterBank(int N, int padding=0) : _N(N), _padding(padding)
	{
		_windowBuf = (SamplePaird*)fftw_malloc(sizeof(SamplePaird) * (_N+_padding));
		_outBuf = (ComplexPaird*)fftw_malloc(sizeof(ComplexPaird) * ((_N+_padding)/2+1));
		_fwPlan = new Time2FrequencyPlan(_N, _windowBuf, _outBuf);
	}
	
	virtual ~FilterBank()
	{
		delete _fwPlan;
		fftw_free(_outBuf);
		fftw_free(_windowBuf);
	}
	
	void addProcessor(IFilterBankProcessor *proc)
	{
		_processors.push_back(proc);
	}
	
	void processInput(SamplePaird *inp)
	{
		for (int n=0; n < _N; ++n)
		{
			const double wc = mlt(n, _N);
			_windowBuf[n][0] = inp[n][0] * wc;
			_windowBuf[n][1] = inp[n][1] * wc;
		}
		_fwPlan->execute();
		
		for (std::list<IFilterBankProcessor*>::iterator i = _processors.begin();
			 i != _processors.end();
			 i++)
		{
			(*i)->processFFT(_outBuf);
		}
	}
	
	void processOutput(SamplePaird *out)
	{
		for (int n=0; n < _N; ++n)
		{
			const double wc = mlt(n,_N)/_N;
			out[n][0] *= wc;
			out[n][1] *= wc;
		}
	}

private:
	int _N;
	int _padding;
	FFTPlan *_fwPlan;
	FFTPlan *_inversePlan;
	SamplePaird *_windowBuf;
	ComplexPaird *_outBuf;
	std::list<IFilterBankProcessor*> _processors;
};

class STFTStream : public T_sink_source<chunk_t>, public IFilterBankProcessor
{
private:
	const FFTWindowFilter &_filt;
	FilterBank *_filterBank;
public:
	// N = FFT size
	// R = hop size
	// hops = number of hops >= 0
	// buffer size = N + hops * R
	STFTStream(const FFTWindowFilter &filt, int N, int R, int hops, int padding=0, FilterBank *filterBank=0) :
		T_sink_source<chunk_t>(0), _filt(filt), _N(N), _R(R), _hops(hops), _sourceChk(0), _padding(padding), _filterBank(filterBank)
	{
		_inBuf = (SamplePaird*)fftw_malloc(sizeof(SamplePaird) * (_N + _hops * R));
		_windowBuf = (SamplePaird*)fftw_malloc(sizeof(SamplePaird) * (_N+_padding));
		_synthBuf = (SamplePaird*)fftw_malloc(sizeof(SamplePaird) * (_N + _hops * _R));
		_outBuf = (ComplexPaird*)fftw_malloc(sizeof(ComplexPaird) * ((_N+_padding)/2+1));
		_fwPlan = new Time2FrequencyPlan(N, _windowBuf, _outBuf);
		_inversePlan = new Frequency2TimePlan(N, _outBuf, (SamplePaird*)_outBuf);
		
		if (_filterBank)
			_filterBank->addProcessor(this);
	}

	~STFTStream()
	{
		delete _inversePlan;
		delete _fwPlan;
		fftw_free(_outBuf);
		fftw_free(_synthBuf);
		fftw_free(_windowBuf);
		fftw_free(_inBuf);
		T_allocator<chunk_t>::free(_sourceChk);
	}

	void reset_source(T_source<chunk_t> *src)
	{
		T_allocator<chunk_t>::free(_sourceChk);
		_synthPtr = _synthBuf;
		_p = 0;
		_src = src;
		fill_input();
	}

	void seek_smp(smp_ofs_t smp_ofs)
	{
	//	_src->seek_smp(smp_ofs);
	}
	void fill_input()
	{
		// fill zero for negative time
		_inPtr = _inBuf;
		_readPtr = _synthBuf + _N/2;
		for (int i=0; i<_N/2; ++i)
		{
			(*_inPtr)[0] = 0.0;
			(*_inPtr++)[1] = 0.0;
		}
		_sourceChk = _src->next();
		_sourceBuf = _sourceChk->_data;
		_sourcePtr = _sourceBuf;
		for (int i=0; i < _N + _hops * _R; ++i)
		{
			_synthBuf[i][0] = 0.0;
			_synthBuf[i][1] = 0.0;
		}
	//	for (int i=0; i < 2048; ++i)
	//	{
	//		_windowBuf[i][0] = 0.0;
	//		_windowBuf[i][1] = 0.0;
	//	}
	}
	void ensure_input()
	{
		while (_inPtr < _inBuf + _p*_R + _N)
		{
			if (_sourcePtr >= _sourceBuf + chunk_t::chunk_size)
			{
				T_allocator<chunk_t>::free(_sourceChk);

				_sourceChk = _src->next();
				_sourceBuf = _sourceChk->_data;
				_sourcePtr = _sourceBuf;
			}
			
			while (_inPtr < _inBuf + _p*_R + _N && _sourcePtr < _sourceBuf + chunk_t::chunk_size)
			{
				(*_inPtr)[0] = (*_sourcePtr)[0];
				(*_inPtr++)[1] = (*_sourcePtr++)[1];
			}
		}
	}
	void ensure_hop()
	{
		if (_p >= _hops + 1)
		{
			memmove(_inBuf, _inBuf + (_p-1)*_R, _N/2*sizeof(SamplePaird));
			_inPtr = _inBuf + _N/2;

			assert(_readPtr >= _synthPtr);
			SamplePaird *synthEnd = _synthBuf + _N + _hops * _R;
			long copy = synthEnd - _synthPtr;
			long readOfs = _readPtr - _synthPtr;
			memmove(_synthBuf, _synthPtr, copy*sizeof(SamplePaird));
			_synthPtr = _synthBuf;
			_readPtr = _synthPtr + readOfs;
			for (long i=copy; i <  _N + _hops * _R; ++i)
			{
				_synthBuf[i][0] = 0.0;
				_synthBuf[i][1] = 0.0;
			}
			_p = 0;
		}
	}
	
	virtual void processFFT(ComplexPaird * const fftBuffer)
	{
		fftw_complex *coeffs = _filt.get_fft_coeffs();
		
		for (int n=0; n < _N/2+1; ++n)
		{
			//	const double coeff0 = 1.0;
			//	const double coeff1 = 0.0;
			const double coeff0 = coeffs[n][0];
			const double coeff1 = coeffs[n][1];
			//	printf("L %f + j%f R %f + j%f\n", _outBuf[n][0][0], _outBuf[n][0][1], _outBuf[n][1][0], _outBuf[n][1][1]);
			// real = ac - bd
			double x = _outBuf[n][0][0] * coeff0 - _outBuf[n][0][1] * coeff1;
			// imag = ad + bc
			double y = _outBuf[n][0][0] * coeff1 + _outBuf[n][0][1] * coeff0;
			_outBuf[n][0][0] = x;
			_outBuf[n][0][1] = y;
			
			x = _outBuf[n][1][0] * coeff0 - _outBuf[n][1][1] * coeff1;
			y = _outBuf[n][1][0] * coeff1 + _outBuf[n][1][1] * coeff0;
			_outBuf[n][1][0] = x;
			_outBuf[n][1][1] = y;
		}

		if (_filterBank)
		{
			_inversePlan->execute();
			_filterBank->processOutput((SamplePaird*)_outBuf);
		}
		else
			processOutput();
	} 
	
	void process_input()
	{
		SamplePaird *inp = _inBuf + _p * _R;
		for (int n=0; n < _N; ++n)
		{
			//	const double wc = mlt(n, _N);
			//	const double wc = 1.0;
			const double wc = mlt(n, _N);
			_windowBuf[n][0] = inp[n][0] * wc;
			_windowBuf[n][1] = inp[n][1] * wc;
		}
		_fwPlan->execute();
	}
	
	void processOutput()
	{
		_inversePlan->execute();
		SamplePaird *smp = (SamplePaird*)_outBuf;
		for (int n=0; n < _N; ++n)
		{
			const double wc = mlt(n,_N)/_N;
			_synthPtr[n][0] += smp[n][0] * wc;
			_synthPtr[n][1] += smp[n][1] * wc;
			//	printf("%f %f\n", _synthPtr[n][0], _synthPtr[n][1]);
		}
		_synthPtr += _R;
		++_p;
	}
	
	// do not call this unless everything finished in the buffer is read out
	// ie readPtr == synthPtr
	void synth_step()
	{
		ensure_hop();
		ensure_input();
		process_input();
		processFFT(_outBuf);
	}
	void transform()
	{
		for (int i=0; i < _N + _hops * _R; ++i)
		{
			_synthBuf[i][0] = 0.0;
			_synthBuf[i][1] = 0.0;
		}

		while (_p < _hops + 1)	
		{
			synth_step();
		}

		for (int i=0; i < _N + _hops * _R; ++i)
		{
		//	printf("SynthBuf (%f, %f)\n", _synthBuf[i][0], _synthBuf[i][1]);
		}
	}
	static const double _windowConstant;
	chunk_t *next()
	{
		chunk_t *chk = T_allocator<chunk_t>::alloc();
		SamplePairf *buf = chk->_data;
		SamplePairf *ptr = buf;

		while (ptr < buf + chunk_t::chunk_size)
		{
			while (_readPtr >= _synthPtr)
				synth_step();
			(*ptr)[0] = float((*_readPtr)[0]/* / _windowConstant*/);
			(*ptr++)[1] = float((*_readPtr++)[1]/* / _windowConstant*/);
		}
		

		return chk;
	}
	
private:
	int _N, _R, _hops;
	SamplePaird *_inBuf;
	SamplePaird *_inPtr;
	SamplePaird *_windowBuf;
	ComplexPaird *_outBuf;
	SamplePaird *_synthBuf;
	SamplePaird *_synthPtr;
	SamplePaird *_readPtr;
	FFTPlan *_fwPlan;
//	FFTPlan **_forwardPlans;
	int _p;
	FFTPlan *_inversePlan;

	chunk_t *_sourceChk;
	SamplePairf *_sourceBuf;
	SamplePairf *_sourcePtr;

	int _padding;
};

#endif
