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

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#define _USE_MATH_DEFINES
#include <cmath>
#include <fftw3.h>

#define INPUT_FILE_SAMPLING_FREQUENCY 44100.0

//#define min(x,y) ((x) < (y) ? (x) : (y))

// kaiser windowing constants
double kaiserTable[10000];
double alpha;
double pitimesalpha;
double d;
double inversed;

// EvalSignal constants
double inputSamplingFrequency;
double inputSamplingPeriod; // input sampling period in seconds

double outputSamplingFrequency;
double outputSamplingPeriod;

double impulseResponseFrequency;
double impulseResponsePeriod;
double rho;
double impulseResponseScale; // "maintains unity gain in the passband"

#include "util.h"

const int N = 2048;

void BuildKaiserTable()
{
	double time;
	for (int i = 0; i < 10000; ++i)
	{
		time = i / 10000.0;
		kaiserTable[i] = I_0(pitimesalpha*sqrt(1-time*time))*inversed;
	}
}

double GetKaiser(double relativeFraction)
{
	return kaiserTable[int(relativeFraction*10000.0)];
}

void SetOutputSamplingFrequency(double f)
{
	outputSamplingFrequency = f;
	outputSamplingPeriod = 1.0 / outputSamplingFrequency;

	impulseResponseFrequency = fmin(inputSamplingFrequency, outputSamplingFrequency);
	impulseResponsePeriod = 1.0 / impulseResponseFrequency;
	rho = outputSamplingFrequency / inputSamplingFrequency;
	impulseResponseScale = fmin(1.0, rho);
}

void init()
{
//	sampleP = 0;
	alpha = 2.0;
	pitimesalpha = M_PI*alpha;
	d = I_0<double>(pitimesalpha);
	inversed = 1.0 / d;

	inputSamplingFrequency = INPUT_FILE_SAMPLING_FREQUENCY;
	inputSamplingPeriod = 1.0 / inputSamplingFrequency;

	SetOutputSamplingFrequency(inputSamplingFrequency);

	BuildKaiserTable();
}

bool CanEval(double t_out, double t_in, size_t nInputSamples)
{
	return false;
}

void EvalSignal(double t_out, float * outputSamples, double t_in, float * inputSamples, size_t nInputSamples)
{
	int nextSample = (int)ceil((t_out - t_in) / inputSamplingPeriod); // first sample after the time we're evaluating
	double nextSampleTime = t_in + nextSample * inputSamplingPeriod; // time of that sample

	double outputAccumulatorL = 0.0;
	double outputAccumulatorR = 0.0;

	int sampleIndex = 2*nextSample; // index into the array since there are two channels interleaved
	double endTime = 13*impulseResponsePeriod; // for all time after endTime, we take impulse reponse to be 0
//	double invEndTime = 1.0 / endTime;
	double invEndTime = 10000.0 / endTime;
	assert(endTime < nInputSamples * inputSamplingPeriod);
	double sampleScale, relativeTime;

	// go through samples following t_out
	for (relativeTime = nextSampleTime - t_out; relativeTime < endTime; relativeTime += inputSamplingPeriod)
	{
	//	pseudocode for scale
	//	sampleScale = h_s(sampleTime) * kaiser(sampleTime);
		sampleScale = (impulseResponseScale * sinc(impulseResponseFrequency*relativeTime)) *
		//	(GetKaiser(relativeTime * invEndTime));
			(kaiserTable[int(relativeTime * invEndTime)]);

		outputAccumulatorL += sampleScale * inputSamples[sampleIndex++];
		outputAccumulatorR += sampleScale * inputSamples[sampleIndex++];
	}

	sampleIndex = 2*(nextSample-1);
	endTime = -13*impulseResponsePeriod;  // for all time before endTime, we take impulse reponse to be 0
//	invEndTime = 1.0 / endTime;
	invEndTime = 10000.0 / endTime;
	assert(endTime+t_out > t_in);

	// go through samples preceding t_out
	for (relativeTime = nextSampleTime - t_out - inputSamplingPeriod; relativeTime > endTime; relativeTime -= inputSamplingPeriod)
	{
		sampleScale = impulseResponseScale * sinc(impulseResponseFrequency*relativeTime) *
		//	(GetKaiser(relativeTime * invEndTime));
			(kaiserTable[int(relativeTime * invEndTime)]);

		outputAccumulatorL += sampleScale * inputSamples[sampleIndex];
		outputAccumulatorR += sampleScale * inputSamples[sampleIndex+1];
		sampleIndex -= 2;
	}

	outputSamples[0] = float(outputAccumulatorL);
	outputSamples[1] = float(outputAccumulatorR);
}

template <int M>
void EvalSignalN(double t_out, float * outputSamples, double t_in, float * inputSamples, size_t nInputSamples)
{
}

//template <int M>
//void EvalSignalN_ftoi(double t_out, int * outputSamples, double t_in, float * inputSamples, size_t nInputSamples)
//{
//}

void myassert(bool cond)
{
	if (!cond)
	{

		fprintf(stderr, "assertion failed\n");
		__asm {
			int 3
		}
	}
}

template <>
void EvalSignalN<1>(double t_out, float * outputSamples, double t_in, float * inputSamples, size_t nInputSamples)
{
	int nextSample = (int)ceil((t_out - t_in) / inputSamplingPeriod); // first sample after the time we're evaluating
	double nextSampleTime = t_in + nextSample * inputSamplingPeriod; // time of that sample

	double acc = 0.0;

	int sampleIndex = nextSample; // index into the array
	double endTime = 13*impulseResponsePeriod; // for all time after endTime, we take impulse reponse to be 0
//	double invEndTime = 1.0 / endTime;
	double invEndTime = 10000.0 / endTime;
/*#define assert(x) { \
	if(!x){ \
		__asm{int 3} \
	}\
}*/
	assert(sampleIndex < nInputSamples);
	assert(endTime < nInputSamples * inputSamplingPeriod);
	double sampleScale, relativeTime;

	// go through samples following t_out
	for (relativeTime = nextSampleTime - t_out; relativeTime < endTime; relativeTime += inputSamplingPeriod)
	{
	//	pseudocode for scale
	//	sampleScale = h_s(sampleTime) * kaiser(sampleTime);
		sampleScale = (impulseResponseScale * sinc(impulseResponseFrequency*relativeTime)) *
		//	(GetKaiser(relativeTime * invEndTime));
			(kaiserTable[int(relativeTime * invEndTime)]);

		acc += sampleScale * inputSamples[sampleIndex++];
	}

	sampleIndex = nextSample-1;
	endTime = -13*impulseResponsePeriod;  // for all time before endTime, we take impulse reponse to be 0
//	invEndTime = 1.0 / endTime;
	invEndTime = 10000.0 / endTime;
	//assert(endTime+t_out >= t_in);

	// go through samples preceding t_out
	for (relativeTime = nextSampleTime - t_out - inputSamplingPeriod; relativeTime > endTime; relativeTime -= inputSamplingPeriod)
	{
		sampleScale = impulseResponseScale * sinc(impulseResponseFrequency*relativeTime) *
		//	(GetKaiser(relativeTime * invEndTime));
			(kaiserTable[int(relativeTime * invEndTime)]);

		acc += sampleScale * inputSamples[sampleIndex];
		--sampleIndex;
	}

	*outputSamples = float(acc);
}

template <>
void EvalSignalN<4>(double t_out, float * outputSamples, double t_in, float * inputSamples, size_t nInputSamples)
{
	int nextSample = (int)ceil((t_out - t_in) / inputSamplingPeriod); // first sample after the time we're evaluating
	double nextSampleTime = t_in + nextSample * inputSamplingPeriod; // time of that sample

	double acc = 0.0;

	int sampleIndex = nextSample; // index into the array
	double endTime = 13*impulseResponsePeriod; // for all time after endTime, we take impulse reponse to be 0
//	double invEndTime = 1.0 / endTime;
	double invEndTime = 10000.0 / endTime;
/*#define assert(x) { \
	if(!x){ \
		__asm{int 3} \
	}\
}*/
//#define assert(x) myassert(x)
	assert(sampleIndex < nInputSamples);
	assert(endTime < nInputSamples * inputSamplingPeriod);
	double sampleScale, relativeTime;

	// go through samples following t_out
	for (relativeTime = nextSampleTime - t_out; relativeTime < endTime; relativeTime += inputSamplingPeriod)
	{
	//	pseudocode for scale
	//	sampleScale = h_s(sampleTime) * kaiser(sampleTime);
		sampleScale = (impulseResponseScale * sinc(impulseResponseFrequency*relativeTime)) *
		//	(GetKaiser(relativeTime * invEndTime));
			(kaiserTable[int(relativeTime * invEndTime)]);

		acc += sampleScale * inputSamples[sampleIndex++];
	}

	sampleIndex = nextSample-1;
	endTime = -13*impulseResponsePeriod;  // for all time before endTime, we take impulse reponse to be 0
//	invEndTime = 1.0 / endTime;
	invEndTime = 10000.0 / endTime;
	assert(endTime+t_out >= t_in);

	// go through samples preceding t_out
	for (relativeTime = nextSampleTime - t_out - inputSamplingPeriod; relativeTime > endTime; relativeTime -= inputSamplingPeriod)
	{
		sampleScale = impulseResponseScale * sinc(impulseResponseFrequency*relativeTime) *
		//	(GetKaiser(relativeTime * invEndTime));
			(kaiserTable[int(relativeTime * invEndTime)]);

		acc += sampleScale * inputSamples[sampleIndex];
		--sampleIndex;
	}

	*outputSamples = float(acc);
}

typedef float aud_smp[2];

#define frame_time (double(N)/96000.0)
#define sample_time (1.0/96000.0)

struct control_processor
{
};

struct aud_processor
{
	float *in;
	fftwf_complex *out;
	fftwf_plan p;
	FILE *ctrl_in, *ctrl_out;
	FILE *aud_in, *aud_out;
	bool in_eof;
	double t_out, t_in_buf;
	aud_smp *in_buf, *out_buf, *end_in;
	size_t sampleCount;
};

void aud_processor_init(struct aud_processor * proc)
{
	int i;
	size_t sz;
	aud_smp * in_ptr;

	proc->sampleCount = 0;
	proc->ctrl_in = fopen("G:\\My Music\\projects\\serato2.raw", "rb");
	proc->ctrl_out = fopen("output.txt", "w");
	proc->in = (float*) fftwf_malloc(sizeof(float) * N*4);
	proc->out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (N*4));
	proc->aud_in = fopen("source.raw", "rb");
	proc->aud_out = fopen("sourceout.raw", "wb");
	proc->in_buf = (aud_smp*) fftwf_malloc(sizeof(aud_smp) * N*2);
	proc->out_buf = (aud_smp*) fftwf_malloc(sizeof(aud_smp) * N);
	proc->t_out = 0.0;

	//proc->p = fftw_plan_dft_1d(N, proc->in, proc->out, FFTW_FORWARD, FFTW_ESTIMATE);
	proc->p = fftwf_plan_dft_r2c_2d(N, 2, proc->in, proc->out, FFTW_MEASURE);

	/* init input buffer */
	in_ptr = proc->in_buf;
	for (i=0; i<15; ++i)
	{
		*in_ptr[0] = 0.0f;
		*(in_ptr++)[1] = 0.0f;
	}
	proc->t_in_buf = -15 * sample_time;
/*	t_inbuf_end = proc->t_in_buf + 1024*sample_time; */
	sz = fread(in_ptr, sizeof(aud_smp), 1024-15, proc->aud_in);
	proc->in_eof = (sz < 1024-15);
	proc->end_in = in_ptr + sz;
}

void aud_processor_destroy(struct aud_processor * proc)
{
	fftwf_destroy_plan(proc->p);
	fftwf_free(proc->in); fftwf_free(proc->out);
	fclose(proc->ctrl_in);
	fclose(proc->ctrl_out);
	fftwf_free(proc->in_buf); fftwf_free(proc->out_buf);
	fclose(proc->aud_in);
	fclose(proc->aud_out);
}

double load_input(struct aud_processor * proc)
{
	size_t rd;
	memmove(proc->in_buf, proc->in_buf + 1024-32, 32*sizeof(aud_smp));
	rd = fread(proc->in_buf+32, sizeof(aud_smp), 1024-32, proc->aud_in);
	if (rd < 1024-32)
	{
		proc->in_eof = true;
		proc->end_in = proc->in_buf+32+rd;
	}
	proc->t_in_buf = proc->t_in_buf + (1024-32)*sample_time;
	return proc->t_in_buf + 1024*sample_time;
}

void store_output(struct aud_processor * proc)
{
	fwrite(proc->out_buf, sizeof(aud_smp), 1024, proc->aud_out);
}

void process_audio(struct aud_processor * proc, double speed_factor)
{
	aud_smp * out_ptr = proc->out_buf;
	aud_smp * end_out = out_ptr + 1024;

	/* check input buffer */
	/* calculate end of this output */
/*	double t_end_out = t_out + frame_time; */
	/* calculate eqvivalent input time */
/*	double t_end_in = proc->t_in_buf + frame_time/speed_factor; */
	double t_end_in = proc->t_in_buf + (proc->end_in - proc->in_buf)*sample_time;
/*	double t_out_delta = sample_time;
	double t_in_delta = sample_time / speed_factor;
*/	double t_smp_p = sample_time / speed_factor;

	for (;out_ptr<end_out;++out_ptr)
	{
		if (proc->t_out + 15*impulseResponsePeriod > t_end_in)
		{
			if (proc->in_eof)
			{
				
			}
			else
			{
				t_end_in = load_input(proc);
			}
		}
		EvalSignal(proc->t_out, (float*)(&(out_ptr[0])), proc->t_in_buf, (float*)(&(proc->in_buf[0])), 1024);
		proc->t_out += t_smp_p;
	}
	store_output(proc);
}

void fftloop(struct aud_processor * proc)
{
	float x,y,dbin,fc;
	float maxL=0.0f, maxR=0.0f;
	fftwf_complex *out=proc->out;
	float *in=proc->in, mag,y1,y2,y3,fcL,fcR;
	double fmod;
	size_t maxLpos=0, maxRpos=0, sz, i;
	sz = fread(proc->in, sizeof(float), N*2, proc->ctrl_in);
//	fprintf(fout, "sz %d sampleCount %d\n", sz, sampleCount);
	fprintf(proc->ctrl_out, "%f\t", proc->sampleCount/96000.0f);
	//while (sz < N*2) in[sz++] = 0.0f;
	fftwf_execute(proc->p); /* repeat as needed */
	for (i=0;i<(N/2+1);++i)
	{
		x = out[i*2][0];
		y = out[i*2][1];
		mag = sqrtf(x*x+y*y);
		if (mag > maxL)
		{
			maxL = mag;
			maxLpos = i;
		}
	//	fprintf(proc->ctrl_out, "%d\t", i);
	//	fprintf(proc->ctrl_out, "%d\tmagL %f\t", i, mag);
		x = out[i*2+1][0];
		y = out[i*2+1][1];
		mag = sqrtf(x*x+y*y);
		if (mag > maxR)
		{
			maxR = mag;
			maxRpos = i;
		}
	//	fprintf(proc->ctrl_out, "magR %f\n", mag);
	}
	x = out[(maxLpos-1)*2][0];
	y = out[(maxLpos-1)*2][1];
	y1 = sqrtf(x*x+y*y);
	y2 = maxL;
	x = out[(maxLpos+1)*2][0];
	y = out[(maxLpos+1)*2][1];
	y3 = sqrtf(x*x+y*y);
	dbin = ((y3-y1)/(y1+y2+y3));
	fcL = (float(maxLpos)+dbin)*(96000.0f/N);
	x = out[(maxRpos-1)*2+1][0];
	y = out[(maxRpos-1)*2+1][1];
	y1 = sqrtf(x*x+y*y);
	y2 = maxR;
	x = out[(maxRpos+1)*2+1][0];
	y = out[(maxRpos+1)*2+1][1];
	y3 = sqrtf(x*x+y*y);
	dbin = ((y3-y1)/(y1+y2+y3));
	fcR = (float(maxRpos)+dbin)*(96000.0f/N);
	fc = (fcL+fcR)*0.5f;
	fprintf(proc->ctrl_out, "fcL = %f fcR = %f avg = %f\n", fcL, fcR, fc);
//	fprintf(proc->ctrl_out, "\n");
/*	fprintf(proc->ctrl_out, "%f\t%d\t%f\t%d\t%f\n", 
		proc->sampleCount/96000.0f,
		maxLpos, maxL,
		maxRpos, maxR); */

	fmod = (1000.0/fc);

	process_audio(proc, fmod);	

	// done
	proc->sampleCount += N;
}

void do_fftw()
{
	struct aud_processor proc;

	aud_processor_init(&proc);

	fprintf(proc.ctrl_out, "t0\tmaxLpos\tmaxLmag\tmaxRpos\tmaxRmag\n");
	while (!feof(proc.ctrl_in))
	{
		fftloop(&proc);
	}
	aud_processor_destroy(&proc);
}

/*
int main()
{
	init();
	do_fftw();
	return 0;
}
*/
