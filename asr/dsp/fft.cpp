#if 0
#include "fft.h"

int main()
{
	//LPFilter filter(1024, 44100.0, 100.0);
	//filter.init();
	KaiserWindowFilter window(256, 8.0);
	window.init();
	STFTBuffer buffer(256, 6, 6000);
	buffer.transform(window);
	return 0;
}
#endif