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
#include "filter.h"
#include "display.h"
#include "asiodrv.cpp"
#include "io.h"
#include "speedparser.h"
#include "ui.h"
#include "tracer.h"

typable(float)
typable(double)

ASR::shared_state_t ASR::shared_state;

void ASR::execute()
{
	begin();

#if WINDOWS
	shared_state.ui->create();
	shared_state.ui->main_loop();
	shared_state.ui->destroy();
#else
#error no ui loop defined
#endif

	end();
}

void ASR::begin()
{
	shared_state.asio = new ASIOProcessor;
#if WINDOWS
	shared_state.ui = new Win32UI(shared_state.asio);
#endif
	shared_state.asio->set_ui(shared_state.ui);
#if RUN
	asio->Start();
	//ASIOStart();
#endif

#if TEST2
	asio->Start();
#endif
}

void ASR::end()
{
#if RUN
	//ASIOStop();
	asio->Stop();
#endif
	shared_state.asio->Finish();
	delete shared_state.ui;
	delete shared_state.asio;

#if DEBUG_MALLOC
	dump_malloc();
#endif
}

int main()
{
#if 0
	ThreadTester<FastUserSyscallLock> *tester = new ThreadTester<FastUserSyscallLock>;
	tester->test();
	delete tester;
	return 0;
#endif
#if 0
#include "filter.h"
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

	ASR *app;
		app = new ASR;
	app->execute();
	delete app;
	

	return 0;
}
