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
	if (_handle != NULL)
		midiInClose(_handle);
}

void Win32MIDIDevice::Callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	Win32MIDIDevice *dev = (Win32MIDIDevice*)dwInstance;
	dev->_cb(wMsg, dwParam1, dwParam2/1000., dev->_cbParam);
}

float64_t Win32MIDIDevice::Start()
{
	midiInStart(_handle);
	return timeGetTime() / 1000.0;
}

void Win32MIDIDevice::Stop()
{
	midiInStop(_handle);
}

void Win32MIDIDeviceFactory::Enumerate()
{
	UINT nInputs = midiInGetNumDevs();
	UINT nOutputs = midiOutGetNumDevs();
	MIDIINCAPS in;
	MIDIOUTCAPS out;

	for (UINT i=0; i < nInputs; ++i)
	{
		::midiInGetDevCaps(i, &in, sizeof(in));
		wprintf(L"Input %d %s\n", i, in.szPname);
	}
	for (UINT i=0; i < nOutputs; ++i)
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
		if (midiInOpen(dev->getHandlePtr(), 
			id, 
			(DWORD_PTR)Win32MIDIDevice::Callback, 
			(DWORD_PTR)dev, 
			CALLBACK_FUNCTION) == MMSYSERR_NOERROR)
		{
			return (IMIDIDevice*)dev;
		}
		delete dev;
	}
	return 0;
}

CDJ350MIDIController::CDJ350MIDIController(IMIDIDevice *dev) : 
	_dev(dev),
	_cb(0)
{
	_dev->RegisterCallback(DeviceCallback, this);
}

CDJ350MIDIController::~CDJ350MIDIController() 
{
	delete _dev;
}

void CDJ350MIDIController::RegisterCallback(MIDIControlCallback cb, void *param)
{
	_cb = cb;
	_cbParam = param;
}

void CDJ350MIDIController::Start()
{
	_startTime = _dev->Start();
}

void CDJ350MIDIController::Stop()
{
	_dev->Stop();
}

void CDJ350MIDIController::DeviceCallback(uint32_t msg, uint32_t param, float64_t time, void *cbParam)
{
	CDJ350MIDIController *control = static_cast<CDJ350MIDIController*>(cbParam);
	int ch = param & 0xF;
	MsgType msgType = (MsgType)((param & 0xFF00) >> 8);
	int code = (param & 0xFF0000) >> 16;
	switch (msgType)
	{
	case JogScratch: // jog scratch
	case JogSpin: // jog spin
		printf("jog\n");
		control->_cb(new BendPitchMsg(ch, time+control->_startTime, 0), control->_cbParam);
		break;
	case Tempo: // tempo
		printf("tempo\n");
		control->_cb(new SetPitchMsg(ch, time+control->_startTime, 0), control->_cbParam);
		break;
	case PlayPause: // play/pause
		printf("playpause\n");
		control->_cb(new PlayPauseMsg(ch, time+control->_startTime), control->_cbParam);
		break;
	default:
		printf("%x %x %f\n", msg, param, time+control->_startTime);
	}
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
