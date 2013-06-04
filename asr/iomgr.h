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

#ifndef _IOMGR_H
#define _IOMGR_H

#include <vector>
#include <string>

#include "AudioDevice.h"

#include "asr.h"
#include "type.h"
#include "asiodrv.h"
#include "util.h"
#include "myasio.h"

struct chunk_buffer : public T_source<chunk_t>
{
	chunk_buffer():_c(0){}
	chunk_t* _c;
	chunk_t* next()
	{
		assert(_c);
		chunk_t *r = _c;
		_c = 0;
		return r;
	}
};

template <typename Chunk_T>
struct chunk_buffer_t : public T_source<Chunk_T>
{
	chunk_buffer_t():_c(0){}
	Chunk_T* _c;
	Chunk_T* next()
	{
		assert(_c);
		Chunk_T *r = _c;
		_c = 0;
		return r;
	}
};

#if WINDOWS

#define CHOOSE_CHANNELS 0
#define RUN 0
#define INPUT_PERIOD (1.0/44100.0)

template <typename Chunk_T>
class IOManager;

struct IASIO;

#define ASIO_ASSERT_NO_ERROR(call) { \
	e = call ; \
	if (e != ASE_OK) { \
		printf("Yo error %s\n", asio_error_str(e)); \
		assert(false); \
	} \
}

struct IODriver
{
	IODriver(std::wstring name) : driverName(name) {}
	virtual ~IODriver() {}

	std::wstring driverName;

	static std::vector<IODriver*> enumerate();
	virtual IOManager<chunk_t>* loadDriver() = 0;
};

struct ASIODriver : public IODriver
{
	ASIODriver(std::wstring name) : IODriver(name) {}
	IOManager<chunk_t>* loadDriver();
};

template <typename Chunk_T>
class IOManager
{
public:
	IOManager() {}
	virtual ~IOManager() {}

	void createInput() {}
	void createOutput() {}
	
protected:
	
};

template <typename Chunk_T>
class ASIOManager : public IOManager<Chunk_T>
{
public:
	typedef int32_t sample_t;

	ASIOManager(IASIO *drv);
	~ASIOManager();

	void enumerateIOs();
	void createBuffers(ASIOProcessor *io);
	void createIOs(chunk_buffer *src, chunk_buffer *src2);

	void setOutputSource(int id);
	void processOutputs();
	void processInputs(long doubleBufferIndex);
	void switchBuffers(long doubleBufferIndex);

	void addInput(AudioInput *input);
	void addOutput(AudioOutput *output);

	sample_t** getBuffers (int index) const;

	int getBufferSize() const { return _bufSize; }

protected:
	IASIO *_drv;
	
	ASIOBufferInfo *_buffer_infos;
	size_t _buffer_infos_len;
	ASIOCallbacks _cb;
	
	long _bufSize;
	long _preferredSize;

	std::list<AudioOutput*> _outputs;
	std::list<AudioInput*> _inputs;
};

#endif // WINDOWS

#endif // !defined(IOMGR_H)
