#include "util.h"

const char* asio_error_str(ASIOError e)
{
	switch (e)
	{
	case 0: return "ASE_OK, This value will be returned whenever the call succeeded";
	case 0x3f4847a0: return "ASE_SUCCESS, unique success return value for ASIOFuture calls";
	case -1000: return "ASE_NotPresent, hardware input or output is not present or available";
	case -999: return "ASE_HWMalfunction, hardware is malfunctioning (can be returned by any ASIO function)";
	case -998: return "ASE_InvalidParameter, input parameter invalid";
	case -997: return "ASE_InvalidMode, hardware is in a bad mode or used in a bad mode";
	case -996: return "ASE_SPNotAdvancing, hardware is not running when sample position is inquired";
	case -995: return "ASE_NoClock, sample clock or rate cannot be determined or is not present";
	case -994: return "ASE_NoMemory, not enough memory for completing the request";
	default: return "I dunno WTF happened but this isn't a valid ASIO error value";
	}
}

const char* asio_sampletype_str(ASIOSampleType t)
{
	switch (t)
	{
	case 16: return "ASIOSTInt16LSB";
	default: return "Unknown";
/*	ASIOSTInt16MSB   = 0,

	ASIOSTInt24MSB   = 1,		// used for 20 bits as well

	ASIOSTInt32MSB   = 2,

	ASIOSTFloat32MSB = 3,		// IEEE 754 32 bit float

	ASIOSTFloat64MSB = 4,		// IEEE 754 64 bit double float



	// these are used for 32 bit data buffer, with different alignment of the data inside

	// 32 bit PCI bus systems can be more easily used with these

	ASIOSTInt32MSB16 = 8,		// 32 bit data with 16 bit alignment

	ASIOSTInt32MSB18 = 9,		// 32 bit data with 18 bit alignment

	ASIOSTInt32MSB20 = 10,		// 32 bit data with 20 bit alignment

	ASIOSTInt32MSB24 = 11,		// 32 bit data with 24 bit alignment

	

	ASIOSTInt16LSB   = 16,

	ASIOSTInt24LSB   = 17,		// used for 20 bits as well

	ASIOSTInt32LSB   = 18,

	ASIOSTFloat32LSB = 19,		// IEEE 754 32 bit float, as found on Intel x86 architecture

	ASIOSTFloat64LSB = 20, 		// IEEE 754 64 bit double float, as found on Intel x86 architecture



	// these are used for 32 bit data buffer, with different alignment of the data inside

	// 32 bit PCI bus systems can more easily used with these

	ASIOSTInt32LSB16 = 24,		// 32 bit data with 18 bit alignment

	ASIOSTInt32LSB18 = 25,		// 32 bit data with 18 bit alignment

	ASIOSTInt32LSB20 = 26,		// 32 bit data with 20 bit alignment

	ASIOSTInt32LSB24 = 27,		// 32 bit data with 24 bit alignment



	//	ASIO DSD format.

	ASIOSTDSDInt8LSB1   = 32,		// DSD 1 bit data, 8 samples per byte. First sample in Least significant bit.

	ASIOSTDSDInt8MSB1   = 33,		// DSD 1 bit data, 8 samples per byte. First sample in Most significant bit.

	ASIOSTDSDInt8NER8	= 40,		// DSD 8 bit data, 1 sample per byte. No Endianness required.
	*/
	}
}

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

KaiserWindowTable<double> *KaiserWindowTable<double>::_inst = 0;
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

template <>
const SamplePairf Zero<SamplePairf>::val = {0.0f, 0.0f};

template <>
const SamplePaird Zero<SamplePaird>::val = {0.0, 0.0};

/* redundant, same as SamplePairf
template <>
const fftwf_complex Zero<fftwf_complex>::val = {0.0f, 0.0f};
*/

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
