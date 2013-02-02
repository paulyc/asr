#ifndef _MIDI_H
#define _MIDI_H

#include "type.h"

#include <windows.h>

class IMIDIDevice
{
public:
	virtual ~IMIDIDevice() {}

	typedef void (*MIDICallback)(uint32_t msg, uint32_t param, float64_t time, void *cbParam);

	virtual float64_t Start() = 0;
	virtual void Stop() = 0;

	virtual void RegisterCallback(MIDICallback cb, void *param) = 0;
};

class Win32MIDIDevice : IMIDIDevice
{
public:
	Win32MIDIDevice();
	Win32MIDIDevice(HMIDIIN handle);
	~Win32MIDIDevice();

	static void CALLBACK Callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
	
	float64_t Start();
	void Stop();

	void RegisterCallback(MIDICallback cb, void *param) { _cb = cb; _cbParam = param; }

	HMIDIIN* getHandlePtr() { return &_handle; }
private:
	HMIDIIN _handle;
	MIDICallback _cb;
	void *_cbParam;
};

class IMIDIDeviceFactory
{
public:
	virtual ~IMIDIDeviceFactory() {}

	virtual void Enumerate() = 0;
	virtual IMIDIDevice* Instantiate(int id, bool input) = 0;
};

class Win32MIDIDeviceFactory : public IMIDIDeviceFactory
{
public:
	void Enumerate();
	IMIDIDevice* Instantiate(int id, bool input);
};

// time is system time
struct ControlMsg
{
	ControlMsg(int ch, float64_t t) : channel(ch), time(t) {}

	int channel;
	float64_t time;
};

struct PlayPauseMsg : public ControlMsg
{
	PlayPauseMsg(int ch, float64_t t) : ControlMsg(ch, t) {}
};

struct CueMsg : public ControlMsg
{
	CueMsg(int ch, float64_t t) : ControlMsg(ch, t) {}
};

struct SetPitchMsg : public ControlMsg
{
	SetPitchMsg(int ch, float64_t t, double val) : ControlMsg(ch, t), value(val) {}

	double value;
};

struct BendPitchMsg : public ControlMsg
{
	BendPitchMsg(int ch, float64_t t, double val) : ControlMsg(ch, t), value(val) {}

	double value;
};

class IMIDIController
{
public:
	typedef void (*MIDIControlCallback)(ControlMsg *msg, void *cbParam);
	virtual ~IMIDIController() {}

	virtual void RegisterCallback(MIDIControlCallback cb, void *cbParam) = 0;

	virtual void Start() = 0;
	virtual void Stop() = 0;
};

class CDJ350MIDIController : public IMIDIController
{
public:
	CDJ350MIDIController(IMIDIDevice *dev);
	~CDJ350MIDIController();

	void RegisterCallback(MIDIControlCallback cb, void *cbParam);
	static void DeviceCallback(uint32_t msg, uint32_t param, float64_t time, void *cbParam); // time is MIDI time

	void Start();
	void Stop();

	enum MsgType
	{
		PlayPause  = 0x00,
		JogScratch = 0x10,
		JogSpin    = 0x30,
		Tempo      = 0x1d
	};
	enum ButtonPush 
	{ 
		Off = 0x00, 
		On  = 0x7f 
	};

private:
	IMIDIDevice *_dev;
	MIDIControlCallback _cb;
	void *_cbParam;
	float64_t _startTime;
};

#endif // !defined(_MIDI_H)
