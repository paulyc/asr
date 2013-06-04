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

#ifndef _UTIL_H
#define _UTIL_H

#define _USE_MATH_DEFINES
#include <cmath>
#include <climits>
#include <cstdlib>

#include <fftw3.h>

#include "type.h"
#include "lock.h"

#if WINDOWS
#include <asio.h>

const char* asio_error_str(ASIOError e);
const char* asio_sampletype_str(ASIOSampleType t);
#endif // WINDOWS

template <typename T>
class Zero
{
public:
	const static T val;
	static void set(T &t)
	{
		t = val;
	}
};

template <>
class Zero<SamplePairf>
{
public:
	static void set(SamplePairf &t)
	{
		t[0] = 0.0f;
		t[1] = 0.0f;
	}
};

template <typename T>
class MinVal
{
public:
	const static T val;
};

template <>
class MinVal<int16_t>
{
public:
	const static int16_t val = (-32768);
};

template <>
class MinVal<int32_t>
{
public:
	const static int32_t val = 0x80000000;
};

template <typename T>
class MaxVal
{
public:
	const static T val;
};

template <>
class MaxVal<int16_t>
{
public:
	const static int16_t val = 0x7FFF;
};

template <>
class MaxVal<int32_t>
{
public:
	const static int32_t val = 0x7FFFFFFF;
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
public:
	static void calc(SamplePairf &result, const SamplePairf &lhs, const SamplePairf &rhs)
	{
		result[0] = lhs[0]+rhs[0];
		result[1] = lhs[1]+rhs[1];
	}
};

template <typename T>
class Quotient : BinaryOperator<T>
{
public:
	static void calc(T &result, const T &lhs, const T &rhs)
	{
		result = lhs / rhs;
	}
};

template <>
class Quotient<SamplePairf> : public BinaryOperator<SamplePairf>
{
public:
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
class SetMax : UnaryOperator<T>
{
public:
	static void calc(T &result, const T &lhs)
	{
		if (lhs > result)
			result = lhs;
	}
};

template <>
class SetMax<SamplePairf> : UnaryOperator<SamplePairf>
{
public:
	static void calc(SamplePairf &result, const SamplePairf &lhs)
	{
		if (lhs[0] > result[0])
			result[0] = lhs[0];
		if (lhs[1] > result[1])
			result[1] = lhs[1];
	}
};

template <typename T>
class SetMin : UnaryOperator<T>
{
public:
	static void calc(T &result, const T &lhs)
	{
		if (lhs < result)
			result = lhs;
	}
};

template <>
class SetMin<SamplePairf> : UnaryOperator<SamplePairf>
{
public:
	static void calc(SamplePairf &result, const SamplePairf &lhs)
	{
		if (lhs[0] < result[0])
			result[0] = lhs[0];
		if (lhs[1] < result[1])
			result[1] = lhs[1];
	}
};

template <typename T>
class Abs : UnaryOperator<T>
{
public:
	static void calc(T &result, const T &lhs)
	{
		result = abs(lhs);
	}
};

template <>
class Abs<SamplePairf> : UnaryOperator<SamplePairf>
{
public:
	static void calc(SamplePairf &result, const SamplePairf &lhs)
	{
		result[0] = fabs(lhs[0]);
		result[1] = fabs(lhs[1]);
	}
};

template <typename T>
class dBm : UnaryOperator<T>
{
public:
	static void calc(T &result, const T &lhs)
	{
		result = 20*log10(lhs);
	}
};

template <>
class dBm<SamplePairf> : UnaryOperator<SamplePairf>
{
public:
	static void calc(SamplePairf &result, const SamplePairf &lhs)
	{
		result[0] = 20.0f*log10f(lhs[0]);
		result[1] = 20.0f*log10f(lhs[1]);
	}
};

inline void PairFromT(SamplePairf &p, short s1, short s2)
{
	if (s1 >= 0)
	{
		p[0] = s1 * (1.0f/float(SHRT_MAX));
	}
	else
	{
		p[0] = s1 * (-1.0f/float(SHRT_MIN));
	}
	if (s2 >= 0)
	{
		p[1] = s2 * (1.0f/float(SHRT_MAX));
	}
	else
	{
		p[1] = s2 * (-1.0f/float(SHRT_MIN));
	}
}

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
	
	sqrt_func get_sqrt_f();
	static KaiserWindowTable *_inst;
	T m_kaiserTable[TblSz];
	T m_alpha;
	T m_beta                                                                                                                                                                                         ;
	T m_d;
	T m_inversed;
	sqrt_func m_sqrt;
public:
	KaiserWindowTable(T alpha=2.0)
	{
		T time;
	//	m_sqrt = get_sqrt_f();
		m_sqrt = sqrt;	//hack
		m_alpha = alpha;
		m_beta = T(M_PI)*m_alpha; // beta
		m_d = I_0<T>(m_beta);
		m_inversed = T(1.0) / m_d;
		for (int i = 0; i < TblSz; ++i)
		{
			time = i / T(TblSz);
			m_kaiserTable[i] = I_0<T>(m_beta*m_sqrt(1-time*time))*m_inversed;
		}
	}

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

template <typename T>
class functor0
{
public:
	functor0(T *obj, void(T::*f)()):_obj(obj),_f(f){}
	void operator()() {(_obj->*_f)();}
private:
	T *_obj;
	void(T::*_f)();
};

class deferred
{
public:
    virtual ~deferred() {}
	virtual void operator()() = 0;
};

template <typename T>
class deferred0 : public functor0<T>, public deferred
{
public:
	deferred0(T *obj, void(T::*f)()) : functor0<T>(obj, f) {}
};

template <typename T, typename Ret>
class functor0_r
{
public:
	functor0_r(T *obj, Ret(T::*f)()):_obj(obj),_f(f){}
	Ret operator()() {return (_obj->*_f)();}
private:
	T *_obj;
	Ret(T::*_f)();
};

template <typename T, typename Param1>
class functor1
{
public:
	functor1(T *obj, void(T::*f)(Param1)):_obj(obj),_f(f){}
	void operator()(Param1 p1) {(_obj->*_f)(p1);}
private:
	T *_obj;
	void(T::*_f)(Param1);
};

template <typename T, typename Param1>
class deferred1 : public functor1<T, Param1>, public deferred
{
public:
	deferred1(T *obj, void(T::*f)(Param1), Param1 p1) : functor1<T, Param1>(obj, f), _p1(p1) {}
	void operator()()
	{
		functor1<T, Param1>::operator ()(_p1);
	}
private:
	Param1 _p1;
};

template <typename T, typename Ret, typename Param1>
class functor1_r
{
public:
	functor1_r(T *obj, Ret(T::*f)(Param1)):_obj(obj),_f(f){}
	Ret operator()(Param1 p1) {return (_obj->*_f)(p1);}
private:
	T *_obj;
	Ret(T::*_f)(Param1);
};

template <typename T, typename Param1, typename Param2>
class functor2
{
public:
	functor2(T *obj, void(T::*f)(Param1,Param2)):_obj(obj),_f(f){}
	void operator()(Param1 p1, Param2 p2) {(_obj->*_f)(p1, p2);}
private:
	T *_obj;
	void(T::*_f)(Param1,Param2);
};

template <typename T, typename Param1, typename Param2>
class deferred2 : public functor2<T, Param1, Param2>, public deferred
{
public:
	deferred2(T *obj, void(T::*f)(Param1, Param2), Param1 p1, Param2 p2) : functor2<T, Param1, Param2>(obj, f), _p1(p1), _p2(p2) {}
	void operator()()
	{
		functor2<T, Param1, Param2>::operator ()(_p1, _p2);
	}
private:
	Param1 _p1;
	Param2 _p2;
};

template <typename T, typename Param1, typename Param2, typename Param3>
class functor3
{
public:
	functor3(T *obj, void(T::*f)(Param1,Param2,Param3)):_obj(obj),_f(f){}
	void operator()(Param1 p1, Param2 p2, Param3 p3) {(_obj->*_f)(p1, p2, p3);}
private:
	T *_obj;
	void(T::*_f)(Param1,Param2,Param3);
};

template <typename T, typename Param1, typename Param2, typename Param3>
class deferred3 : public functor3<T, Param1, Param2, Param3>, public deferred
{
public:
	deferred3(T *obj, void(T::*f)(Param1,Param2,Param3), Param1 p1, Param2 p2, Param3 p3) : 
	  functor3<T, Param1, Param2, Param3>(obj, f), _p1(p1), _p2(p2), _p3(p3) {}
	void operator()()
	{
		functor3<T, Param1, Param2, Param3>::operator ()(_p1, _p2, _p3);
	}
private:
	Param1 _p1;
	Param2 _p2;
	Param3 _p3;
};

template <typename T>
class fast_queue
{
public:
	fast_queue():_count(0),_head(0),_tail(0){}
	void push(T item)
	{
		++_count;
		node *t = _tail;
		_tail = new node(item, 0);
		if (t) t->_next = _tail;
		else _head = _tail;
	}
	T pop()
	{
		node *h = _head;
		T it = h->_item;
		--_count;
		_head = h->_next;
		delete h;
		if (!_head) _tail = 0;
		return it;
	}
	unsigned count1()
	{
		node *n = _head;
		unsigned c;
		if (!n) return 0;
		for (c=1;n!=_tail;++c)n=n->_next;
		return c;
	}
	unsigned count2()
	{
		return _count;
	}
private:
	struct node 
	{
		node(T item, node *next):_item(item),_next(next){}
		T _item;
		node *_next;
	};
	unsigned _count;
	node *_head;
	node *_tail;
};

template <typename T>
class ts_queue : public fast_queue<T>
{
public:
	ts_queue():
		fast_queue<T>(){}
	void push (T item)
	{
		_lock.acquire();
		fast_queue<T>::push(item);
		_cond.signal();
		_lock.release();
	}
	T pop()
	{
		_lock.acquire();
		T it = fast_queue<T>::pop();
		_lock.release();
		return it;
	}
	T pop_wait()
	{
		_lock.acquire();
		while (!fast_queue<T>::count1())
			_cond.signal();
		T it = fast_queue<T>::pop();
		_lock.release();
		return it;
	}
	unsigned count()
	{
		_lock.acquire();
		unsigned c = fast_queue<T>::count1();
		_lock.release();
		return c;
	}
private:
	Lock_T _lock;
	Condition_T _cond;
};

#if ONE_CPU
#define LOCK_IF_SMP(lock) lock->acquire();
#define UNLOCK_IF_SMP(lock) lock->release();
#else
#define LOCK_IF_SMP(lock)
#define UNLOCK_IF_SMP(lock)
#endif

template <int N, typename T=double>
class moving_average
{
public:
	moving_average() : _next(_points) {}
	void set(T point)
	{
		*_next++ = point;
		if (_next == N)
			_next = _points;
	}

	T get()
	{
		T sum = Zero<T>::val;
		for (int i=0; i<N; ++i)
			sum += _points[i];
		return sum / N;
	}
protected:
	T _points[N];
	T *_next;
};

#if !ONE_CPU
#define CRITICAL_SECTION_GUARD(lock)
#else
#define CRITICAL_SECTION_GUARD(lock) if (lock) { \
lock->acquire(); \
lock->release(); \
sched_yield(); }
#endif


#if 0
#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))
#endif

#if 0
#if WINDOWS
typedef std::exception string_exception;
#else
class string_exception : public std::exception
{
public:
	string_exception(const char *p) : std::exception(), _p(p) {}
	const char *what() { return _p; }
private:
	const char *_p;
};
#endif
#endif // 0

#if MAC
#include <string>
#include <CoreFoundation/CoreFoundation.h>

std::string CFStringRefToString(CFStringRef ref);
#endif

class Differentiator
{
public:
	Differentiator(int size=3)
	{
		_size = size;
		_buffer = new double[size];
		_ptr = _buffer;
		for (int i=0; i<size; ++i)
			_buffer[i] = 0.0;
	}
	~Differentiator()
	{
		delete [] _buffer;
	}
	double dx()
	{
		if (_ptr == _buffer)
			return *_ptr - *(_ptr + _size -1);
		else
			return *_ptr - *(_ptr-1);
	}
	void next(double x)
	{
		++_ptr;
		if (_ptr >= _buffer + _size)
			_ptr = _buffer;
		*_ptr = x;
	}
private:
	int _size;
	double *_buffer;
	double *_ptr;
};

#endif // !defined(_UTIL_H)
