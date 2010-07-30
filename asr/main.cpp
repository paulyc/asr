#include <cstdio>
#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <queue>
#include <iostream>
#include <fstream>
using namespace std;

#include "asr.h"

template <int data_size_n>
struct chunk_float32_T_size : public chunk_T_T_size<float, data_size_n>
{
};

typedef chunk_float32_T_size<512> chunk_float32_2048B;
typable(chunk_float32_2048B)
//define_template_type_n(chunk_float32_T_size,512,chunk_float32_2048B)

template <int data_size_n>
struct chunk_int16_T_size : public chunk_T_T_size<unsigned short, data_size_n>
{
};

typedef chunk_int16_T_size<512> chunk_int16_1024B;

template <int data_size_n>
struct chunk_byte_T_size : public chunk_T_T_size<char, data_size_n>
{
};

typedef chunk_byte_T_size<1024> chunk_byte_1024B;

typedef T_source<chunk_float32_2048B> default_float32_source;
typedef T_sink<chunk_float32_2048B> default_float32_sink;

class default_float32_sin_source : public default_float32_source
{
	float _sample_rate;
	float _frequency;
	float _period;
	float _2_pi_f;
	float _next_time;
public:
	default_float32_sin_source(float sample_rate, float frequency) :
		_sample_rate(sample_rate),
		_frequency(frequency),
		_period(1.0f/frequency),
		_2_pi_f(float(M_2_PI)*frequency),
		_next_time(0.0f)
	{
	}

	virtual chunk_float32_2048B* next()
	{
		chunk_float32_2048B * chunk = new chunk_float32_2048B;
		size_t data_size = sizeof(chunk->_data)/sizeof(float);
		for (size_t i=0;i<data_size;++i)
		{
			chunk->_data[i] = sinf(_2_pi_f*_next_time);
			_next_time += _period;
		}
		return chunk;
	}
};

template <typename T>
class T_stream_base
{
public:
	virtual void process() = 0;
};

template <typename T>
class T_stream : public T_stream_base<T>
{
	T_source<T> * _source;
	T_sink<T> * _sink;
public:
	T_stream<T>()
	{
		_source = new T_source<T>;
		_sink = new T_sink<T>;
	}

	T_stream<T>(T_source<T> * source, T_sink<T> * sink) :
		_source(source),
		_sink(sink)
	{
	}

	virtual void process()
	{
		_sink->next(_source->next());
	}
};

typedef T_source<chunk_null> null_source;
typedef T_sink<chunk_null> null_sink;
typedef T_stream<chunk_null> null_stream;

template <int size_bytes>
struct chunk_zero_T_size
{
	char _data[size_bytes];

	chunk_zero_T_size<size_bytes>()
	{
		memset(_data, 0, size_bytes);
	}
};

typedef chunk_zero_T_size<512> chunk_zero_512B;

typedef T_source<chunk_zero_512B> zero_source;
typedef T_sink<chunk_zero_512B> zero_sink;
typedef T_stream<chunk_zero_512B> zero_stream;

template <typename T>
class T_stream_connector : public T_source<T>, public T_sink<T>
{
	T_stream_base<T> * _stream_source;
	T * _temp_T;
public:
	T_stream_connector<T>() :
		_stream_source(0),
		_temp_T(0)
	{
	}

	T_stream_connector<T>(T_stream_base<T> * stream_source) :
		_stream_source(stream_source),
		_temp_T(0)
	{
	}

	virtual void set_stream_source(T_stream_base<T> * stream_source)
	{
		_stream_source = stream_source;
	}

	virtual T* next()
	{
		_stream_source->process();
		T * ret_T = _temp_T;
		_temp_T = 0;
		return ret_T;
	}

	virtual void next(T* t)
	{
		_temp_T = t;
	}
};

template <typename T>
class T_stream_sum : public T_stream_base<T>
{
	T_source<T> * _source1;
	T_source<T> * _source2;
	T_sink<T> * _sink;
	T_sink<T> * _default_sink;
public:
	T_stream_sum<T>(T_source<T> * source1, T_source<T> * source2, T_sink<T> * sink) :
		_source1(source1),
		_source2(source2),
		_sink(sink),
		_default_sink(new T_sink<T>)
	{
	}

	~T_stream_sum<T>()
	{
		delete _default_sink;
	}

	virtual void process()
	{
		T * chunk1 = _source1->next();
		T * chunk2 = _source2->next();
		// sum into chunk1 and pass it on
		size_t i;
		for (i=0;i<sizeof(chunk1->_data)/sizeof(float) && i<sizeof(chunk2->_data)/sizeof(float);++i)
		{
			chunk1->_data[i] += chunk2->_data[i];
		}
		for (;i<sizeof(chunk2->_data)/sizeof(float);++i)
		{
			chunk1->_data[i] = chunk2->_data[i];
		}
		_default_sink->next(chunk2);
		_sink->next(chunk1);
	}
};

template <typename T, int N, typename precision_type=float>
class T_stream_DFT : public T_stream_base<T>
{
	T_source<T> * _source;
	T_sink<T> * _sink;
	T_sink<T> * _default_sink;
	
	struct complex
	{
		precision_type real;
		precision_type imag;
	};

	// [n][k][2] NxN/2x2
	static complex _coeff_tab[N][N];
	static bool _coeff_tab_init;
public:
	T_stream_DFT<T,N,precision_type>(T_source<T> * source, T_sink<T> * sink) :
		_source(source),
		_sink(sink),
		_default_sink(new T_sink<T>)
	{
		if (!_coeff_tab_init)
		{
			fill_coeff_tab();
		}
	}

	~T_stream_DFT<T,N,precision_type>()
	{
		delete _default_sink;
	}

	static void fill_coeff_tab()
	{
		precision_type root = -precision_type(M_2_PI)/precision_type(N), param;
		for (size_t k=0;k<N;++k)
		{
			for (size_t n=0;n<N;++n)
			{
				param = root*k*n;
				_coeff_tab[n][k].real = cosf(param);
				_coeff_tab[n][k].imag = sinf(param);
			}
		}
		_coeff_tab_init = true;
	}

	static void generate_coeff_tab()
	{
		ofstream out("c:\\test.txt");
		out.setf(ios::showpoint);
		if (!_coeff_tab_init)
		{
			fill_coeff_tab();
		}
		out << "template <>\n"<<gettype<precision_type>()<<" T_stream_DFT<"<<gettype<T>()<<","<<N<<","<<gettype<precision_type>()<<">::_coeff_tab["<<N<<"]["<<N/2<<"][2] = {\n";
		for (size_t n=0;n<N;++n)
		{
			out << "{";
			for (size_t k=0;k<N/2;++k)
			{
				out << "{"<<_coeff_tab[n][k][0]<<"f,"<<_coeff_tab[n][k][1]<<"f},";
			}
			out << "},\n";
		}
		out << "};" << endl;
	}

	virtual void process()
	{
		T * chunk = _source->next();
		T * chk_out = T_allocator<T>::alloc();
		size_t k,n;
		_complex coeff;
		
		for (k=0;k<N/2-1;++k)
		{
			chk_out->_data[2*k] = precision_type(0.0);
			chk_out->_data[2*k+1] = precision_type(0.0);
			for (n=0;n<N;++n)
			{
				chk_out->_data[2*k] += chunk->_data[n]*_coeff_tab[n][k].real;
				chk_out->_data[2*k+1] += chunk->_data[n]*_coeff_tab[n][k].imag;
			}
		}

		_default_sink->next(chunk);
		_sink->next(chk_out);
	}
};

template <typename T, int N, typename precision_type>
T_stream_DFT<T,N,precision_type>::complex T_stream_DFT<T,N,precision_type>::_coeff_tab[N][N];

template <>
T_stream_DFT<chunk_float32_2048B,512,float>::complex T_stream_DFT<chunk_float32_2048B,512,float>::_coeff_tab[512][512] = {1.0,1.0};

template <typename T, int N, typename precision_type>
bool T_stream_DFT<T,N,precision_type>::_coeff_tab_init = false;

template <typename T, int N, typename precision_type=float>
class T_stream_iDFT : public T_stream_base<T>
{
	T_source<T> * _source;
	T_sink<T> * _sink;
	T_sink<T> _default_sink;
	
	struct complex
	{
		precision_type real;
		precision_type imag;
	};

	// [n][k][2] NxNx2
	static complex _coeff_tab[N][N];
	static bool _coeff_tab_init;
public:
	T_stream_iDFT<T,N,precision_type>(T_source<T> * source, T_sink<T> * sink) :
		_source(source),
		_sink(sink)
	{
		if (!_coeff_tab_init)
		{
			fill_coeff_tab();
		}
	}

	// same as DFT version, should subclass..
	static void fill_coeff_tab()
	{
		precision_type root = -precision_type(M_2_PI)/precision_type(N), param;
		for (size_t k=0;k<N;++k)
		{
			for (size_t n=0;n<N;++n)
			{
				param = root*k*n;
				_coeff_tab[n][k].real = cosf(param);
				_coeff_tab[n][k].imag = sinf(param);
			}
		}
		_coeff_tab_init = true;
	}

	virtual void do_dft()
	{
	}

	virtual void process()
	{
		T * chunk = _source->next();
		T * chk_out = T_allocator<T>::alloc();
		size_t k,n;
		for (n=0;n<N;++n)
		{
			chk_out->_data[n] = chunk->_data[0]*_coeff_tab[n][0][0];
			for (k=1;k<N/2-1;++k)
			{
				chk_out->_data[n] += chunk->_data[2*k-1]*_coeff_tab[n][k][0] -
									chunk->_data[2*k]*_coeff_tab[n][k][1];
			}
			chk_out->_data[n] -= chunk->_data[N-1]*_coeff_tab[n][0][1];
			for (k=N/2+1;k<N;++k)
			{
				chk_out->_data[n] += chunk->_data[2*(N-k)-1]*_coeff_tab[n][k][0] -
									chunk->_data[2*(N-k)]*_coeff_tab[n][k][1];
			}
		}

		_default_sink.next(chunk);
		_sink->next(chk_out);
	}
};

template <typename T, int N, typename precision_type>
T_stream_iDFT<T,N,precision_type>::complex T_stream_iDFT<T,N,precision_type>::_coeff_tab[N][N];

template <>
T_stream_iDFT<chunk_float32_2048B,512,float>::complex T_stream_iDFT<chunk_float32_2048B,512,float>::_coeff_tab[512][512] = {1.0, 1.0};

template <typename T, int N, typename precision_type>
bool T_stream_iDFT<T,N,precision_type>::_coeff_tab_init = false;

class custom_source : public default_float32_source
{
public:
	virtual chunk_float32_2048B* next()
	{
		chunk_float32_2048B* chk = T_allocator<chunk_float32_2048B>::alloc();
		for (size_t i=0;i<512;++i)
			chk->_data[i] = float(i);
		return chk;
	}
};

class fspace_print_sink : public default_float32_sink
{
public:
	virtual void next(chunk_float32_2048B * chk)
	{
		//print
		size_t n=0;
		cout << "N[" << n << "] = " << chk->_data[1] << " + j0" << endl;
		for (;n<256;++n)
			cout << "N[" << n << "] = " << chk->_data[2*n] << " + j" << chk->_data[2*n+1] << endl;
		cout << "N[" << n << "] = " << chk->_data[1] << " + j0" << endl;
		T_allocator<chunk_float32_2048B>::free(chk);
	}
};

class tspace_print_sink : public default_float32_sink
{
public:
	virtual void next(chunk_float32_2048B * chk)
	{
		//print
		size_t n=0;
		for (;n<10;++n)
			cout << "N[" << n << "] = " << chk->_data[n] << endl;
		T_allocator<chunk_float32_2048B>::free(chk);
	}
};

int main()
{
	T_stream<chunk_null> null_stream;

	// dial tone generator: sum of a 350 Hz and 440 Hz sine-wave
	T_sink<chunk_float32_2048B> my_sink;
	default_float32_sin_source source_350(44100.0f, 350.0f), source_440(44100.0f, 440.0f);
	T_stream_sum<chunk_float32_2048B> my_stream(&source_350, &source_440, &my_sink);
	my_stream.process();

	tspace_print_sink my_dft_sink;
	custom_source my_dft_source;

	T_stream_connector<chunk_float32_2048B> conn;

	T_stream_DFT<chunk_float32_2048B,512> dft_stream(&my_dft_source, &conn);
	conn.set_stream_source(&dft_stream);
	//dft_stream.process();
	T_stream_iDFT<chunk_float32_2048B,512> idft_stream(&conn, &my_dft_sink);
	
	idft_stream.process();

	int i = 0, sum=0;
	for (; i<512;++i)
	{
		sum +=i;
	}
	cout << "sum is " << sum << endl;
}