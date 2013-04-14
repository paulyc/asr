#if 0
#include "fft.h"

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
	STFTBuffer buffer(&src, lp, 1024, 512, 40);
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