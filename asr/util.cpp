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

#include "util.h"

template <>
double sinc(double x)
{
	if (x == 0.0) return 1.0;
	double d = M_PI*x;
	return sin(d)/d;
}

template <>
float sinc(float x)
{
	if (x == 0.0f) return 1.0f;
	float d = float(M_PI)*x;
	return sinf(d)/d;
}

//lim a->0
double delta(double x, double a=0.001)
{
	return exp(-(x*x)/(a*a))/(a*0.5*M_2_SQRTPI);
}

template <>
KaiserWindowTable<double> *KaiserWindowTable<double>::_inst = 0;
template <>
KaiserWindowTable<float> *KaiserWindowTable<float>::_inst = 0;

template <>
KaiserWindowTable<double>::sqrt_func KaiserWindowTable<double>::get_sqrt_f()
{
	return sqrt;
}

template <>
KaiserWindowTable<float>::sqrt_func KaiserWindowTable<float>::get_sqrt_f()
{
	return sqrtf;
}

template <>
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

template <>
float I_0(float z)
{
	float sum = 0.0f;
	float k = 0.0f;
	float kfact = 1.0f;
	float quarterzsquared = 0.25f*z*z;
	do
	{
		sum += pow(quarterzsquared, k) / (kfact*kfact);
		k += 1.0f;
		kfact *= k;
	} while (k < 30.0f);
	return sum;
}

template <>
const char *printfbuffer_format<float>()
{
	return "%f ";
}

template <typename T>
void printfbuffer(T *buf, int len, const char *label)
{
	if (label)
		printf("%s\n", label);
	for (int i = 0; i < len; ++i)
	{
		printf(printfbuffer_format<T>(), buf[i]);
		if (i%16 == 0)
			printf("\n");
	}
	printf("\n");
}

template <typename T>
const T Zero<T>::val = T(0);

template <>
const double Zero<double>::val = 0.0;

template <>
const float Zero<float>::val = 0.0;

void copy_chunk_buffer()
{
}

void buf_copy(char *buf_src, char *buf_dst, int num_items, int item_sz, int stride)
{
	char *src_end = buf_src + num_items*(item_sz+stride), *item_end;
	while (buf_src != src_end) {
		item_end = buf_src+item_sz;
		while (buf_src != item_end) {
			*buf_dst++ = *buf_src++;
		}
		buf_src += stride;
	}
}

#if MAC
#include <string>
#include <CoreFoundation/CoreFoundation.h>
std::string CFStringRefToString(CFStringRef ref)
{
	const char *cStr = CFStringGetCStringPtr(ref, kCFStringEncodingUTF8);
	if (!cStr)
	{
		CFIndex len = CFStringGetLength(ref);
		char *buffer = new char[len+1];
		CFStringGetCString(ref, buffer, len+1, kCFStringEncodingUTF8);
		std::string s = std::string(buffer);
		delete [] buffer;
		return s;
	}
	else
	{
		return std::string(cStr);
	}
}
#endif
