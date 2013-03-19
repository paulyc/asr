#ifndef _FFT_H
#define _FFT_H

#include "../type.h"
#define _USE_MATH_DEFINES
#include <math.h>
double I_0(double z)
{
	double sum = 0.0;
	double k = 0.0;
	double kfact = 1.0;
	double quarterzsquared = 0.25*z*z;
	do
	{
		sum += pow(quarterzsquared, k) / (kfact*kfact);
		k += 1.0;
		kfact *= k;
	} while (k < 30.0);
	return sum;
}

// -1.0 <= t <= 1.0

double kaiser(double t, double beta=8.0)
{
	return I_0(beta*sqrt(1.0 - t*t)) / I_0(beta);
}

class FFTPlan
{
public:
	FFTPlan() : _plan(NULL) {}
	FFTPlan(fftwf_plan p) : _plan(p) {}
	virtual ~FFTPlan()
	{
		fftwf_destroy_plan(_plan);
	}
	void execute()
	{
		fftwf_execute(_plan);
	}
protected:
	fftwf_plan _plan;
};



// in is at least N SamplePairfs (2N floats)
// out is at least ... ComplexPairfs (... floats)
class Time2FrequencyPlan : public FFTPlan
{
public:
	Time2FrequencyPlan(int N, SamplePairf *in, ComplexPairf *out) 
	{
		const int n[] = {N};
		_plan = fftwf_plan_many_dft_r2c(1, n, 2, (float*)in, NULL, 2, 1, (fftwf_complex*)out, NULL, 2, 1, FFTW_MEASURE);
	}
};


// overwrites input array
// Frequency2Time(Time2Frequency(x(n))) = N * x(n) for all 0 <= n < N
// out is at least N SamplePairfs (2N floats)
// in is at least ... ComplexPairfs (... floats)
class Frequency2TimePlan : public FFTPlan
{
public:
	Frequency2TimePlan(int N, ComplexPairf *in, SamplePairf *out)
	{
		const int n[] = {N};
		_plan = fftwf_plan_many_dft_c2r(1, n, 2, (fftwf_complex*)in, NULL, 2, 1, (float*)out, NULL, 2, 1, FFTW_MEASURE);
	}
};

class FFT
{
public:
	FFT(int N)
	{
		_timeBuf = (SamplePairf*)fftwf_malloc(N * sizeof(SamplePairf));
		_freqBuf = (ComplexPairf*)fftwf_malloc((N/2+1) * sizeof(ComplexPairf));

		_forwardPlan = new Time2FrequencyPlan(N, _timeBuf, _freqBuf);
		_inversePlan = new Frequency2TimePlan(N, _freqBuf, _timeBuf);
	}

	virtual ~FFT()
	{
		delete _inversePlan;
		delete _forwardPlan;

		fftwf_free(_timeBuf);
		fftwf_free(_freqBuf);
	}
private:
	SamplePairf *_timeBuf;
	ComplexPairf *_freqBuf;

	FFTPlan *_forwardPlan;
	FFTPlan *_inversePlan;
};

class FFTWindowFilter
{
public:
	FFTWindowFilter(int N, double beta=8.0) : _N(N), _beta(beta)
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
			_coeffs[i] = kaiser(t, _beta) * h_n(i - _N/2);
		//	printf("coeff[%d] = %f\n", i, _coeffs[i]);
			t += dt;
		}
	}
	void init()
	{
		fftw_plan p = fftw_plan_dft_r2c_1d(_N, _coeffs, _fftCoeffs, FFTW_MEASURE);
		fill();
		fftw_execute(p);
		fftw_destroy_plan(p);
		for (int i=0; i<_N/2+1; ++i)
		{
		//	printf("fftCoeff[%d] = %f + j%f\n", i, _fftCoeffs[i][0], _fftCoeffs[i][1]);
		}
	}
	fftw_complex *get_fft_coeffs() const
	{
		return _fftCoeffs;
	}
	static const double _windowConstant = 18.5918625;
private:
	int _N;
	double _beta;
	double *_coeffs;
	fftw_complex *_fftCoeffs;
};

class KaiserWindowFilter : public FFTWindowFilter
{
public:
	KaiserWindowFilter(int N, double beta=8.0) : FFTWindowFilter(N, beta) {}
	double h_n(int n) { return 1.0; }
};

class LPFilter : public FFTWindowFilter
{
public:
	LPFilter(int N, double sampleRate, double cutoff) : 
	  FFTWindowFilter(N), _sampleRate(sampleRate), _cutoff(cutoff)
	  {
		  _f_c = _cutoff / _sampleRate;
	  }
	virtual double h_n(int n)
	{
		const double _2_f_c = 2.0 * _f_c;
		if (n == 0)
			return _2_f_c;
		return _2_f_c*sinc(_2_f_c*n);
	}
	static double sinc(double x)
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
};

class STFTBuffer
{
public:
	// N = FFT size
	// R = hop size
	// hops = number of hops >= 0
	// buffer size = N + hops * R
	STFTBuffer(int N, int R, int hops) : _N(N), _R(R), _hops(hops)
	{
		_inBuf = (SamplePairf*)fftwf_malloc(sizeof(SamplePairf) * (N + hops * R));
		_synthBuf = (SamplePairf*)fftwf_malloc(sizeof(SamplePairf) * (N + hops * R));
		_synthPtr = _synthBuf;
		_outBuf = (ComplexPairf*)fftwf_malloc(sizeof(ComplexPairf) * (N/2+1));
		_forwardPlans = new FFTPlan*[hops + 1];
		for (int i=0; i<hops+1; ++i)
		{
			_forwardPlans[i] = new Time2FrequencyPlan(N, _inBuf + i*R, _outBuf);
		}
		_p = 0;
		_inversePlan = new Frequency2TimePlan(N, _outBuf, (SamplePairf*)_outBuf);

		fill_input();
	}
	void fill_input()
	{
		// fill zero for negative time
		_inPtr = _inBuf;
		_readPtr = _synthBuf + _N;
		for (int i=0; i<_N; ++i)
		{
			(*_inPtr)[0] = 0.0f;
			(*_inPtr++)[1] = 0.0f;
		}
	//	for (; input < _inBuf + (_N + _hops * _R); ++input)
	//	{
	//		(*input)[0] = 1.0f;
	//		(*input)[1] = 1.0f;
	//	}
	}
	void ensure_input()
	{
		while (_inPtr < _inBuf + _p*_R + _N)
		{
			if (_sourcePtr >= _sourceBuf + inp_size)
			{
				_sourceBuf = next_input();
				_sourcePtr = inp;
			}
			while (_inPtr < _inBuf + _p*_R + _N && _sourcePtr < _sourceBuf + inp_size)
			{
				(*_inPtr)[0] = (*_p)[0];
				(*_inPtr++)[1] = (*_p++)[1];
			}
		}
	}
	void ensure_hop()
	{
		if (_p >= _hops + 1)
		{
			memcpy(_inBuf, _inBuf + (_p-1)*_R, _N*sizeof(SamplePairf));
			_inPtr = _inBuf + _N;

			// synthptr, readptr ???
		//	memcpy(_synthBuf, _synthPtr - _R, _N*sizeof(SamplePairf));
		//	_synthPtr = _synthBuf + _R;
			memcpy(_synthBuf, _readPtr,
			_synthPtr = _synthBuf + (
			_readPtr = _synthBuf;
		}

		_p = 0;
	}
	void synth_step(const FFTWindowFilter &filt)
	{
		ensure_hop();

		ensure_input();
		
		fftw_complex *coeffs = filt.get_fft_coeffs();
		_forwardPlans[_p]->execute();
		for (int n=0; n < _N/2+1; ++n)
		{
		//	printf("L %f + j%f R %f + j%f\n", _outBuf[n][0][0], _outBuf[n][0][1], _outBuf[n][1][0], _outBuf[n][1][1]);
			// real = ac - bd
			double x = _outBuf[n][0][0] * coeffs[n][0] - _outBuf[n][0][1] * coeffs[n][1];
			// imag = ad + bc
			double y = _outBuf[n][0][0] * coeffs[n][1] + _outBuf[n][0][1] * coeffs[n][0];
			_outBuf[n][0][0] = coeffs[n][0];//x;
			_outBuf[n][0][1] = coeffs[n][1];//y;

			x = _outBuf[n][1][0] * coeffs[n][0] - _outBuf[n][1][1] * coeffs[n][1];
			y = _outBuf[n][1][0] * coeffs[n][1] + _outBuf[n][1][1] * coeffs[n][0];
			_outBuf[n][1][0] = coeffs[n][0];//x;
			_outBuf[n][1][1] = coeffs[n][1];//y;
		}
		_inversePlan->execute();
		SamplePairf *smp = (SamplePairf*)_outBuf;
		for (int n=0; n < _N; ++n)
		{
		//	printf("%f %f\n", smp[n][0], smp[n][1]);
			_synthPtr[n][0] += smp[n][0] / 256.;
			_synthPtr[n][1] += smp[n][1] / 256.;
		}
		_synthPtr += _R;
		++_p;
	}
	void transform(const FFTWindowFilter &filt)
	{
		for (int i=0; i < _N + _hops * _R; ++i)
		{
			_synthBuf[i][0] = 0.0f;
			_synthBuf[i][1] = 0.0f;
		}

		while (_p < _hops + 1)	
		{
			synth_step(filt);
		}

		for (int i=0; i < _N + _hops * _R; ++i)
		{
			printf("SynthBuf (%f, %f)\n", _synthBuf[i][0], _synthBuf[i][1]);
		}
	}
	const double _windowConstant = 18.5918625;
	SamplePairf *next(const FFTWindowFilter &filt, int n)
	{
		SamplePairf *buf = new SamplePairf[n];
		SamplePairf *ptr = buf;

		while (_readPtr + n > _synthPtr)
		{
			synth_step(filt);
		}

		while (ptr < buf + n)
		{
			(*ptr)[0] = (*_readPtr)[0] / _windowConstant;
			(*ptr++)[1] = (*_readPtr++)[1] / _windowConstant;
		}

		return buf;
	}
	~STFTBuffer()
	{
		delete [] _forwardPlans;
		delete _inversePlan;
		fftwf_free(_outBuf);
		fftwf_free(_synthBuf);
		fftwf_free(_inBuf);
	}
private:
	int _N, _R, _hops;
	SamplePairf *_inBuf;
	SamplePairf *_inPtr;
	ComplexPairf *_outBuf;
	SamplePairf *_synthBuf;
	SamplePairf *_synthPtr;
	SamplePairf *_readPtr;
	FFTPlan **_forwardPlans;
	int _p;
	FFTPlan *_inversePlan;

	SamplePairf *_sourceBuf;
	SamplePairf *_sourcePtr;
};

#endif
