#ifndef _FFT_H
#define _FFT_H

#include "../type.h"

class FFT
{
public:
	FFT(int N);
	~FFT();
private:
	float *_inBuf;
	float *_outBuf;
};

class FFTPlan
{
public:
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
// out is at least N/2 + 1 ComplexPairfs (2N + 4 floats)
class Time2FrequencyPlan : public FFTPlan
{
public:
	Time2FrequencyPlan(int N, SamplePairf *in, ComplexPairf *out) :
	  FFTPlan(fftwf_plan_dft_r2c_2d(N, 2, (float*)in, (fftwf_complex*)out, FFTW_MEASURE)) {}
};


// overwrites input array
// Frequency2Time(Time2Frequency(x(n))) = N * x(n) for all 0 <= n < N
// out is at least N SamplePairfs (2N floats)
// in is at least N/2 + 1 ComplexPairfs (2N + 4 floats)
class Frequency2TimePlan : public FFTPlan
{
public:
	Frequency2TimePlan(int N, ComplexPairf *in, SamplePairf *out) :
	  FFTPlan(fftwf_plan_dft_c2r_2d(N, 2, (fftwf_complex*)in, (float*)out, FFTW_MEASURE)) {}
};

class FFTWindowFilter
{
public:
};

class STFTBuffer
{
public:
	// N = FFT size
	// R = hop size
	// hops = number of hops >= 0
	// buffer size = N + hops * R
	STFTBuffer(int N, int R, int hops)
	{
		_inBuf = (float*)fftwf_malloc(sizeof(SamplePairf) * (N + hops * R));
		_plans = new FFTPlan*[hops + 1];
	//	for (int i=0; i<hops+1; ++i)
	//	{
	//		_plans[i] = new Time2FrequencyPlan(N, _inBuf, 0);
	//	}
	}
	~STFTBuffer()
	{
		delete [] _plans;
		fftwf_free(_inBuf);
	}
private:
	float *_inBuf;
	FFTPlan **_plans;
};

#endif
