#ifndef _IOMGR_H
#define _IOMGR_H

#include <asio.h>

#include <vector>
#include <string>

#include "asr.h"
#include "type.h"
#include "asiodrv.h"
#include "util.h"
#include "myasio.h"

#define CHOOSE_CHANNELS 0
#define RUN 0
#define INPUT_PERIOD (1.0/44100.0)

template <typename Chunk_T>
class IOManager;

interface IASIO;

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

struct chk_mgr : public T_source<chunk_t>
	{
		chk_mgr():_c(0){}
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
class ASIOManager : public IOManager<Chunk_T>
{
public:
	ASIOManager(IASIO *drv);
	~ASIOManager();

	void enumerateIOs();
	void createBuffers();
	void createIOs(chk_mgr *src);

	void setOutputSource(int id);

	void switchBuffers(long doubleBufferIndex);

//protected:
	IASIO *_drv;

	ASIODriverInfo _drv_info;
	ASIOBufferInfo *_buffer_infos;
	size_t _buffer_infos_len;
	ASIOCallbacks _cb;
	long _numInputChannels;
	long _numOutputChannels;
	ASIOChannelInfo *_input_channel_infos;
	ASIOChannelInfo *_output_channel_infos;
	long _bufSize;
	long _preferredSize;

	struct channel
	{
	};
	
	struct input : public channel
	{
	};

	struct output : public channel
	{
	};

	std::vector<T_source<Chunk_T>*> _inputs;
	std::vector<T_sink<Chunk_T>*> _outputs;

	asio_sink<chunk_t, chk_mgr, short> *_main_out;
	asio_sink<chunk_t, chk_mgr, short> *_out_2;
};

#endif // !defined(IOMGR_H)
