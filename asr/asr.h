// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef _ASR_H
#define _ASR_H

#include "config.h"

#include <map>
#if MAC
#include <unordered_map>
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
    ASR(GenericUI *ui);
	~ASR();

    void init();
	void execute();
    void finish();

private:
	ASIOProcessor *_asio;
	GenericUI *_ui;
};

#endif // !defined(_ASR_H)
