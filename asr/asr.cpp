#include "config.h"

#include <asio.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <string>
#include <exception>
#include <iostream>
#include <fstream>
#include <hash_map>
using std::exception;

#include <pthread.h>

#include <fftw3.h>
#include <emmintrin.h>

#include "asr.h"
#include "type.h"
#include "util.h"

#include "fftw.h"

#include "wavfile.h"
#include "dsp/filter.h"
#include "display.h"
#include "asiodrv.cpp"
#include "io.h"
#include "speedparser.h"
#include "ui.h"
#include "tracer.h"

typable(float)
typable(double)

ASR::ASR()
{
	_asio = new ASIOProcessor;
#if WINDOWS
	_ui = new Win32UI(_asio);
#else
#error no ui defined
#endif
}

ASR::~ASR()
{
	_asio->Finish();
	delete _ui;
	delete _asio;
}

void ASR::execute()
{
	_asio->set_ui(_ui);
	_ui->create();
	_ui->main_loop();
	_ui->destroy();
}

#if 1
int main()
{
#if 0
	//ThreadTester<FastUserSyscallLock> *tester = new ThreadTester<FastUserSyscallLock>;
	PreemptableLockTester *tester = new PreemptableLockTester;
	tester->test();
	delete tester;
	return 0;
#endif
#if 0
#include "dsp/filter.h"
#include <iostream>
	using namespace std;
	dumb_resampler<double,double> rs(13);
	double *buf = rs.get_tap_buffer();
	buf[0] = 1.0;
	buf[1] = 1.0;
	buf[2] = 1.0;
	buf[3] = 1.0;
	buf[4] = 1.0;
	buf[5] = 1.0;
	buf[6] = -1.0;
	buf[7] = 1.0;
	buf[8] = 1.0;
	buf[9] = 1.0;
	buf[10] = 1.0;
	buf[11] = 1.0;
	buf[12] = 1.0;

	for (double d = -7; d < -5; d += 0.01){ 
		cout << d << " : " << rs.apply(d) << endl;
	}
	return 0;
#endif
#if TEST
	return test_main();
#endif

	//freopen("stdout.txt", "w", stdout);

	ASR *app = new ASR;
	app->execute();
	delete app;

	T_allocator<chunk_t>::dump_leaks();
#if DEBUG_MALLOC
	dump_malloc();
#endif

	return 0;
}
#endif
