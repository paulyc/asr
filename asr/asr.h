#ifndef _ASR_H
#define _ASR_H

#include <map>
#if MAC
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <queue>
#include <cassert>
#include <exception>
#define _USE_MATH_DEFINES
#include <cmath>
#include <set>
#include <fstream>
#include <iostream>

#include <fftw3.h>
#include <pthread.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "config.h"
#include "malloc.h"

#include "type.h"

#include <semaphore.h>
#include "chunk.h"
#include "malloc.h"
#include "stream.h"
#include "midi.h"

class ASIOProcessor;
class GenericUI;

class ASR
{
public:
	ASR(int argc, char **argv);
	~ASR();

	void execute();

private:
	ASIOProcessor *_asio;
	GenericUI *_ui;
};

#endif // !defined(_ASR_H)
