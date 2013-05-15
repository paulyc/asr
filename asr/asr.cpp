#include "config.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <string>
#include <exception>
#include <iostream>
#include <fstream>
using std::exception;

#include <pthread.h>

#include <fftw3.h>
#include <emmintrin.h>

#include "io.h"

#include "asr.h"
#include "type.h"
#include "util.h"

#include "dsp/filter.h"
#include "display.h"

#if WINDOWS
#include "asiodrv.cpp"
#endif

#include "speedparser.h"
#include "ui.h"
#include "tracer.h"

typable(float)
typable(double)

ASR::ASR(int argc, char **argv)
{
	_asio = new ASIOProcessor;
#if WINDOWS
	_ui = new Win32UI(_asio);
#elif OPENGL_ENABLED
	_ui = new OpenGLUI(_asio, argc, argv);
#elif MAC
    //_ui = new CocoaUI(_asio);
#else
    _ui = new CommandlineUI(_asio);
#endif
}

ASR::ASR(GenericUI *ui) : _ui(ui)
{
    _asio = new ASIOProcessor;
    _ui->_io = _asio;
}

ASR::~ASR()
{
	_asio->Finish();
	delete _ui;
	delete _asio;
}

void ASR::init()
{
    _asio->set_ui(_ui);
	_ui->create();
}

void ASR::execute()
{
	init();
	_ui->main_loop();
	finish();
}

void ASR::finish()
{
    _ui->destroy();
}

#if !TEST_BEATS && !MAC
int main(int argc, char **argv)
{
#if 0
	//ThreadTester<FastUserSyscallLock> *tester = new ThreadTester<FastUserSyscallLock>;
	PreemptableLockTester *tester = new PreemptableLockTester;
	tester->test();
	delete tester;
	return 0;
#endif

	//freopen("stdout.txt", "w", stdout);

	ASR *app = new ASR(argc, argv);
	app->execute();
	delete app;

	T_allocator<chunk_t>::dump_leaks();
#if DEBUG_MALLOC
	dump_malloc();
#endif

	return 0;
}
#endif
