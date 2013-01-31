#include "midi.h"

Win32MIDIDevice::Win32MIDIDevice() :
	_handle(NULL)
{
}

Win32MIDIDevice::Win32MIDIDevice(HMIDIIN handle) :
	_handle(handle)
{
}

Win32MIDIDevice::~Win32MIDIDevice()
{
	midiInClose(_handle);
}

void Win32MIDIDevice::Callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	Win32MIDIDevice *dev = (Win32MIDIDevice*)dwInstance;
	dev->Process(wMsg, dwParam1, dwParam2/1000.);
}

void Win32MIDIDevice::Start()
{
	midiInStart(_handle);
}

void Win32MIDIDevice::Stop()
{
	midiInStop(_handle);
}

void Win32MIDIDevice::Process(UINT wMsg, DWORD_PTR dwParam1, double timeS)
{
	int ch = dwParam1 & 0xF;
	MsgType msgType = (MsgType)((dwParam1 & 0xFF00) >> 8);
	int code = (dwParam1 & 0xFF0000) >> 16;
	switch (msgType)
	{
	case JogScratch: // jog scratch
	case JogSpin: // jog spin
		printf("jog\n");
		break;
	case Tempo: // tempo
		printf("tempo\n");
		break;
	case PlayPause: // play/pause
		printf("playpause\n");
		break;
	default:
		printf("%x %x %f\n", wMsg, dwParam1, timeS);
	}
	
}

void Win32MIDIDeviceFactory::Enumerate()
{
	UINT nInputs = midiInGetNumDevs();
	UINT nOutputs = midiOutGetNumDevs();
	MIDIINCAPS in;
	MIDIOUTCAPS out;

	for (int i=0; i < nInputs; ++i)
	{
		::midiInGetDevCaps(i, &in, sizeof(in));
		wprintf(L"Input %d %s\n", i, in.szPname);
	}
	for (int i=0; i < nOutputs; ++i)
	{
		::midiOutGetDevCaps(i, &out, sizeof(in));
		wprintf(L"Output %d %s\n", i, out.szPname);
	}
}

IMIDIDevice* Win32MIDIDeviceFactory::Instantiate(int id, bool input)
{
	if (input)
	{
		Win32MIDIDevice *dev = new Win32MIDIDevice;
		midiInOpen(dev->getHandlePtr(), id, (DWORD_PTR)Win32MIDIDevice::Callback, (DWORD_PTR)dev, CALLBACK_FUNCTION);
		return (IMIDIDevice*)dev;
	}
	return 0;
}

#if 0
int main()
{
	Win32MIDIDeviceFactory fac;
	fac.Enumerate();
	IMIDIDevice *dev = fac.Instantiate(0, true);
	dev->Start();
	Sleep(10000);
	dev->Stop();
	delete dev;
	return 0;
}
#endif
