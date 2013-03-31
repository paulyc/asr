#ifndef _FFT_H
#define _FFT_H

#include "../type.h"
#include "../stream.h"
#include "../malloc.h"
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
			printf("coeff[%d] = %f\n", i, _coeffs[i]);
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
	static const double _windowConstant;
private:
	int _N;
	double _beta;
	double *_coeffs;
	fftw_complex *_fftCoeffs;
};

const double FFTWindowFilter::_windowConstant = 18.5918625;

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

class TestSource : public T_source<chunk_t>
{
public:
	chunk_t *next()
	{
		chunk_t *chk = T_allocator<chunk_t>::alloc();
		for (SamplePairf *p = chk->_data; p < chk->_data + chunk_t::chunk_size; ++p)
		{
			(*p)[0] = 1.0f;
			(*p)[1] = -1.0f;
		}
		return chk;
	}
};

class STFTBuffer : public T_sink_source<chunk_t>
{
public:
	const FFTWindowFilter &_filt;
	// N = FFT size
	// R = hop size
	// hops = number of hops >= 0
	// buffer size = N + hops * R
	STFTBuffer(T_source<chunk_t> *src, const FFTWindowFilter &filt, int N, int R, int hops) : 
		T_sink_source(src), _filt(filt), _N(N), _R(R), _hops(hops)
	{
		_inBuf = (SamplePaird*)fftw_malloc(sizeof(SamplePaird) * (N + hops * R));
		_synthBuf = (SamplePaird*)fftw_malloc(sizeof(SamplePaird) * (N + hops * R));
		_synthPtr = _synthBuf;
		_outBuf = (ComplexPaird*)fftw_malloc(sizeof(ComplexPaird) * (N/2+1));
		_forwardPlans = new FFTPlan*[hops + 1];
		for (int i=0; i<hops+1; ++i)
		{
			_forwardPlans[i] = new Time2FrequencyPlan(N, _inBuf + i*R, _outBuf);
		}
		_p = 0;
		_inversePlan = new Frequency2TimePlan(N, _outBuf, (SamplePaird*)_outBuf);

		fill_input();
	}
	void fill_input()
	{
		// fill zero for negative time
		_inPtr = _inBuf;
		_readPtr = _synthBuf + _N;
		for (int i=0; i<_N; ++i)
		{
			(*_inPtr)[0] = 0.0;
			(*_inPtr++)[1] = 0.0;
		}
		_sourceChk = _src->next();
		_sourceBuf = _sourceChk->_data;
		_sourcePtr = _sourceBuf;
	//	for (; input < _inBuf + (_N + _hops * _R); ++input)
	//	{
	//		(*input)[0] = 1.0f;
	//		(*input)[1] = 1.0f;
	//	}
		for (int i=0; i < _N + _hops * _R; ++i)
		{
			_synthBuf[i][0] = 0.0;
			_synthBuf[i][1] = 0.0;
		}
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
			memcpy(_inBuf, _inBuf + (_p-1)*_R, _N*sizeof(SamplePaird));
			_inPtr = _inBuf + _N;

			// synthptr, readptr ???
		//	memcpy(_synthBuf, _synthPtr - _R, _N*sizeof(SamplePairf));
		//	_synthPtr = _synthBuf + _R;
			SamplePaird *synthEnd = _synthBuf + _N + _hops * _R;
			int copy = synthEnd - _readPtr;
			int synthOfs = synthEnd - _synthPtr;
			memcpy(_synthBuf, _readPtr, copy*sizeof(SamplePaird));
			_synthPtr = _synthBuf + synthOfs;
			_readPtr = _synthBuf;
			_p = 0;
		}
	}
	void synth_step()
	{
		ensure_hop();

		ensure_input();
		
		fftw_complex *coeffs = _filt.get_fft_coeffs();
		_forwardPlans[_p]->execute();
		for (int n=0; n < _N/2+1; ++n)
		{
		//	printf("L %f + j%f R %f + j%f\n", _outBuf[n][0][0], _outBuf[n][0][1], _outBuf[n][1][0], _outBuf[n][1][1]);
			// real = ac - bd
			double x = _outBuf[n][0][0] * coeffs[n][0] - _outBuf[n][0][1] * coeffs[n][1];
			// imag = ad + bc
			double y = _outBuf[n][0][0] * coeffs[n][1] + _outBuf[n][0][1] * coeffs[n][0];
			_outBuf[n][0][0] = x;
			_outBuf[n][0][1] = y;

			x = _outBuf[n][1][0] * coeffs[n][0] - _outBuf[n][1][1] * coeffs[n][1];
			y = _outBuf[n][1][0] * coeffs[n][1] + _outBuf[n][1][1] * coeffs[n][0];
			_outBuf[n][1][0] = x;
			_outBuf[n][1][1] = y;
		}
		_inversePlan->execute();
		SamplePaird *smp = (SamplePaird*)_outBuf;
		for (int n=0; n < _N; ++n)
		{
		//	printf("%f %f\n", smp[n][0], smp[n][1]);
			_synthPtr[n][0] += smp[n][0] / (_N*_N);
			_synthPtr[n][1] += smp[n][1] / (_N*_N);
		//	printf("%f %f\n", _synthPtr[n][0], _synthPtr[n][1]);
		}
		_synthPtr += _R;
		++_p;
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
			printf("SynthBuf (%f, %f)\n", _synthBuf[i][0], _synthBuf[i][1]);
		}
	}
	static const double _windowConstant;
	chunk_t *next()
	{
		chunk_t *chk = T_allocator<chunk_t>::alloc();
		SamplePairf *buf = chk->_data;
		SamplePairf *ptr = buf;

//		while (_readPtr + n > _synthPtr)
//		{
//			synth_step();
//		}

		while (ptr < buf + chunk_t::chunk_size)
		{
			while (_readPtr >= _synthPtr)
				synth_step();
			(*ptr)[0] = float((*_readPtr)[0] / _windowConstant);
			(*ptr++)[1] = float((*_readPtr++)[1] / _windowConstant);
		}

		return chk;
	}
	~STFTBuffer()
	{
		delete [] _forwardPlans;
		delete _inversePlan;
		fftw_free(_outBuf);
		fftw_free(_synthBuf);
		fftw_free(_inBuf);
	}
private:
	int _N, _R, _hops;
	SamplePaird *_inBuf;
	SamplePaird *_inPtr;
	ComplexPaird *_outBuf;
	SamplePaird *_synthBuf;
	SamplePaird *_synthPtr;
	SamplePaird *_readPtr;
	FFTPlan **_forwardPlans;
	int _p;
	FFTPlan *_inversePlan;

	chunk_t *_sourceChk;
	SamplePairf *_sourceBuf;
	SamplePairf *_sourcePtr;
};

const double STFTBuffer::_windowConstant = 1.0;//18.5918625;

#endif
