#ifndef _ASR_H
#define _ASR_H

#include <map>
#include <hash_map>
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

/*
SamplePairf& operator=(SamplePairf &lhs, float rhs)
{
	lhs[0] = rhs;
	lhs[1] = rhs;
	return lhs;
}

SamplePairf operator*(SamplePairf &lhs, float rhs)
{
	SamplePairf val;
	val[0] = lhs[0] * rhs;
	val[1] = lhs[1] * rhs;
	return val;
}

SamplePairf& operator*=(SamplePairf &lhs, float rhs)
{
	lhs[0] *= rhs;
	lhs[1] *= rhs;
	return lhs;
}
*/

#include "chunk.h"
#include "malloc.h"
#include "stream.h"
#include "midi.h"

class ASIOProcessor;
class GenericUI;

class ASR
{
public:
	ASR() {}
	~ASR() {}

	void execute();

	static ASIOProcessor* get_io_instance()
	{
		return shared_state.asio;
	}

	static GenericUI* get_ui_instance()
	{
		return shared_state.ui;
	}

protected:
	void begin();
	void end();

	static struct shared_state_t {
		shared_state_t() : asio(0), ui(0), running(true) {}
		ASIOProcessor * asio;
		GenericUI *ui;
		bool running;
	} shared_state;
};

#endif // !defined(_ASR_H)
