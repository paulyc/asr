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

template <int chunk_size>
class SpeedParser2
{
public:
	SpeedParser2()
	{
		_inBuf = _inEnd = (float*)fftwf_malloc(sizeof(float) * chunk_size * 2);
		_outBuf = _outEnd = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (chunk_size*4));
		_plan = fftwf_plan_dft_r2c_2d(chunk_size, 2, _inBuf, _outBuf, FFTW_MEASURE);
	}
	void LoadData(float* data=NULL, int smp=0)
	{
		float *endData = data+2*smp;
		float *endIn = _inBuf + 2*chunk_size;
		size_t empty_buffer = endIn - _inEnd, data_to_copy = endData - data;
		if (empty_buffer <= 0)
		{
		}
		if (empty_buffer < data_to_copy)
		{
			memcpy(_inEnd, data, sizeof(float)*empty_buffer);
		}
		else
		{
			memcpy(_inEnd, data, sizeof(float)*data_to_copy);
			_inEnd += 2*smp;
		}
		while (data < endData && _inEnd < endIn)
		{
		}
	}
	float phase(float x, float y)
	{
		if (x > 0.0f)
			return atanf(y/x);
		else if (x < 0.0f)
		{
			if (y >= 0.0f)
				return atanf(y/x) + float(M_PI);
			else
				return atanf(y/x) - float(M_PI);
		}
		else
		{
			if (y > 0.0f)
				return float(M_PI_2);
			else if (y < 0.0f)
				return -float(M_PI_2);
			else
				return NAN;
		}
	}
	double Execute(/*float* data=NULL, int smp*/)
	{
		float x, y, y1, y2, y3, mag, maxL=0.0f, maxR=0.0f, dbin, fc, fcR, fcL, phL, phR;
		/*if (data) memcpy(_inBuf, data, sizeof(float) * chunk_size * 2);*/
		fftwf_execute(_plan);
		int i, maxLpos, maxRpos;
		for (i=0;i<(chunk_size/2+1);++i)
		{
			x = _outBuf[i*2][0];
			y = _outBuf[i*2][1];
			mag = sqrtf(x*x+y*y);
			if (mag > maxL)
			{
				maxL = mag;
				phL = phase(x,y);
				maxLpos = i;
			}
			x = _outBuf[i*2+1][0];
			y = _outBuf[i*2+1][1];
			mag = sqrtf(x*x+y*y);
			if (mag > maxR)
			{
				maxR = mag;
				phR = phase(x,y);
				maxRpos = i;
			}
		}
		x = _outBuf[(maxLpos-1)*2][0];
		y = _outBuf[(maxLpos-1)*2][1];
		y1 = sqrtf(x*x+y*y);
		y2 = maxL;
		x = _outBuf[(maxLpos+1)*2][0];
		y = _outBuf[(maxLpos+1)*2][1];
		y3 = sqrtf(x*x+y*y);
		dbin = ((y3-y1)/(y1+y2+y3));
		fcL = (float(maxLpos)+dbin)*(SAMPLERATE/chunk_size);
		x = _outBuf[(maxRpos-1)*2+1][0];
		y = _outBuf[(maxRpos-1)*2+1][1];
		y1 = sqrtf(x*x+y*y);
		y2 = maxR;
		x = _outBuf[(maxRpos+1)*2+1][0];
		y = _outBuf[(maxRpos+1)*2+1][1];
		y3 = sqrtf(x*x+y*y);
		dbin = ((y3-y1)/(y1+y2+y3));
		fcR = (float(maxRpos)+dbin)*(SAMPLERATE/chunk_size);
		fc = (fcL+fcR)*0.5f;

		//printf("_speed %f fc %f magL %f magR %f phaseL - phaseR %f\n", 1000.0/fc, fc, maxL, maxR, phL- phR);
		if (maxL < 1.0f)
			return 0.0;
		return 2000.0/fc;
	}
	~SpeedParser2()
	{
		fftwf_destroy_plan(_plan);
		fftwf_free(_inBuf);
		fftwf_free(_outBuf);
	}
//protected:
	float *_inBuf, *_inEnd;
	fftwf_complex *_outBuf, *_outEnd;
	fftwf_plan _plan;
};
