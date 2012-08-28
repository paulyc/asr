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
	shared_state.ui->main_loop();
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
#if TEST
	return test_main();
#endif

	ASR *app = new ASR;
	app->execute();
	delete app;

	return 0;
}
