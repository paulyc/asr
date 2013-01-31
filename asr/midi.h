#ifndef _MIDI_H
#define _MIDI_H

#include "type.h"

#include <windows.h>

class IMIDIDevice
{
public:
	virtual ~IMIDIDevice() {}

	virtual void Start() = 0;
	virtual void Stop() = 0;
};

class Win32MIDIDevice : IMIDIDevice
{
public:
	Win32MIDIDevice();
	Win32MIDIDevice(HMIDIIN handle);
	~Win32MIDIDevice();

	static void CALLBACK Callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
	
	void Start();
	void Stop();

	void Process(UINT wMsg, DWORD_PTR dwParam1, double timeS);

	HMIDIIN* getHandlePtr() { return &_handle; }

	enum MsgType
	{
		PlayPause = 0x00,
		JogScratch = 0x10,
		JogSpin = 0x30,
		Tempo = 0x1d
	};
	enum ButtonPush { on, off };
private:
	HMIDIIN _handle;
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

#endif // !defined(_MIDI_H)
