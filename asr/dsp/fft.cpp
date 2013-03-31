#if 1
#include "fft.h"

int main()
{
	//LPFilter filter(1024, 44100.0, 100.0);
	//filter.init();
	TestSource src;
	KaiserWindowFilter window(256, 8.0);
	window.init();
	STFTBuffer buffer(&src, window, 256, 6, 6000);
	buffer.transform();
	chunk_t *chk = buffer.next();
	for (SamplePairf *p = chk->_data; p < chk->_data + chunk_t::chunk_size; ++p)
	{
		printf("%f %f\n", (*p)[0], (*p)[1]);
	}
	return 0;
}
#endif