#ifndef _UTIL_H
#define _UTIL_H

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

#endif