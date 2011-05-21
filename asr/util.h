#ifndef _UTIL_H
#define _UTIL_H

#define _USE_MATH_DEFINES
#include <cmath>

#include <asio.h>
#include <fftw3.h>

#include "asr.h"

const char* asio_error_str(ASIOError e);
const char* asio_sampletype_str(ASIOSampleType t);

template <typename T>
class Zero
{
public:
	const static T val;
	void set(T &t)
	{
		t = val;
	}
};

template <typename T>
class BinaryOperator
{
public:
	static void calc(T &result, const T &lhs, const T &rhs);
};

template <typename T>
class Sum : BinaryOperator<T>
{
public:
	static void calc(T &result, const T &lhs, const T &rhs)
	{
		result = lhs + rhs;
	}
};

template <>
class Sum<SamplePairf> : public BinaryOperator<SamplePairf>
{
	static void calc(SamplePairf &result, const SamplePairf &lhs, const SamplePairf &rhs)
	{
		result[0] = lhs[0]+rhs[0];
		result[1] = lhs[1]+rhs[1];
	}
};

template <typename T>
class Division : BinaryOperator<T>
{
public:
	static void calc(T &result, const T &lhs, const T &rhs)
	{
		result = lhs / rhs;
	}
};

template <>
class Division<SamplePairf> : public BinaryOperator<SamplePairf>
{
	static void calc(SamplePairf &result, const SamplePairf &lhs, const SamplePairf &rhs)
	{
		result[0] = lhs[0]/rhs[0];
		result[1] = lhs[1]/rhs[1];
	}
	static void calc(SamplePairf &result, const SamplePairf &lhs, float scalar)
	{
		result[0] = lhs[0]/scalar;
		result[1] = lhs[1]/scalar;
	}
};

template <typename T>
class Product : BinaryOperator<T>
{
public:
	static void calc(T &result, const T &lhs, const T &rhs)
	{
		result = lhs * rhs;
	}
};

template <>
class Product<SamplePairf> : public BinaryOperator<SamplePairf>
{
	static void calc(SamplePairf &result, const SamplePairf &lhs, const SamplePairf &rhs)
	{
		result[0] = lhs[0]*rhs[0];
		result[1] = lhs[1]*rhs[1];
	}
	static void calc(SamplePairf &result, const SamplePairf &lhs, float scalar)
	{
		result[0] = lhs[0]*scalar;
		result[1] = lhs[1]*scalar;
	}
};

template <typename T>
class UnaryOperator
{
public:
	static void calc(T &result, const T &lhs);
};

template <typename T>
class Log10 : UnaryOperator<T>
{
public:
	static void calc(T &result, const T &lhs)
	{
		result = log10(lhs);
	}
};

template <>
class Log10<SamplePairf> : UnaryOperator<SamplePairf>
{
public:
	static void calc(SamplePairf &result, const SamplePairf &lhs)
	{
		result[0] = log10f(lhs[0]);
		result[1] = log10f(lhs[1]);
	}
};

template <typename T>
T sinc(T x);

double delta(double x, double a);

template <typename T>
T I_0(T z);

template <typename T>
class KaiserWindowBase
{
public:
	virtual T get(T x) = 0;
};

template <typename T, int TblSz=10000>
class KaiserWindowTable : public KaiserWindowBase<T>
{
protected:
	typedef T(*sqrt_func)(T);
	KaiserWindowTable()
	{
		T time;
		m_sqrt = get_sqrt_f();
		m_alpha = T(2.0);
		m_pitimesalpha = T(M_PI)*m_alpha;
		m_d = I_0<T>(m_pitimesalpha);
		m_inversed = T(1.0) / m_d;
		for (int i = 0; i < TblSz; ++i)
		{
			time = i / T(TblSz);
			m_kaiserTable[i] = I_0<T>(m_pitimesalpha*m_sqrt(1-time*time))*m_inversed;
		}
	}
	sqrt_func get_sqrt_f();
	static KaiserWindowTable *_inst;
	T m_kaiserTable[TblSz];
	T m_alpha;
	T m_pitimesalpha;
	T m_d;
	T m_inversed;
	sqrt_func m_sqrt;
public:
	static KaiserWindowTable* get()
	{
		if (!_inst)
			_inst = new KaiserWindowTable;
		return _inst;
	}

	T get(T x)
	{
		int indx = int((x < T(0.0)? -x : x)*T(TblSz));
		if (indx >= TblSz)
			indx = TblSz - 1;
		return m_kaiserTable[indx];
	}
	T get_i(int i)
	{
		return m_kaiserTable[i];
	}
};

template <typename T>
class KaiserWindowCalc : public KaiserWindowBase<T>
{
public:
	static T get(T x)
	{
	}
};

class OperationBase
{
public:
	virtual void operator()() = 0;
};

template <typename T>
const char *printfbuffer_format();

template <typename T>
void printfbuffer(T *buf, int len, const char *label=0);

void copy_chunk_buffer();
void buf_copy(char *buf_src, char *buf_dst, int num_items, int item_sz=1, int stride=1);

#endif
