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

#include "fft.h"

const double FFTWindowFilter::_windowConstant = 18.5918625;
const double STFTStream::_windowConstant = 1.0;//18.5918625;

double sinc(double x)
{
	if (x == 0.0)
		return 1.0;
	else
		return sin(M_PI*x)/(M_PI*x);
}

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

// 0 <= n < N
double mlt(int n, int N)
{
	return sin((n + 0.5)*M_PI/(N));
}

#if 0
int main()
{
	//LPFilter filter(1024, 44100.0, 100.0);
	//filter.init();
	freopen("output.txt", "w", stdout);
	TestSource src;
	//KaiserWindowFilter window(512, 8.0);
	//window.init();
	//AllPassFilter ap(512);
	//ap.init();
	LPFilter lp(1024, 44100.0, 100.0);
	lp.init();
	STFTStream buffer(&src, lp, 1024, 512, 40);
	//buffer.transform();
	chunk_t *chk = buffer.next();
	for (SamplePairf *p = chk->_data; p < chk->_data + chunk_t::chunk_size; ++p)
	{
		printf("(%f,%f)\n", (*p)[0], (*p)[1]);
	}
/*	chk = buffer.next();
	for (SamplePairf *p = chk->_data; p < chk->_data + chunk_t::chunk_size; ++p)
	{
		printf("(%f,%f) ", (*p)[0], (*p)[1]);
	}
	chk = buffer.next();
	for (SamplePairf *p = chk->_data; p < chk->_data + chunk_t::chunk_size; ++p)
	{
		printf("(%f,%f) ", (*p)[0], (*p)[1]);
	}
*/	return 0;
}
#endif