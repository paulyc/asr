#include "midi.h"

Win32MIDIDevice::Win32MIDIDevice() :
	_handle(NULL),
	_cb(0)
{
}

Win32MIDIDevice::Win32MIDIDevice(HMIDIIN handle) :
	_handle(handle),
	_cb(0)
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
	if (dev->_cb)
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

CDJ350MIDIController::CDJ350MIDIController(IMIDIDevice *dev, IControlListener *listener) : 
	_dev(dev),
	_listener(listener)
{
	_dev->RegisterCallback(DeviceCallback, this);
}

CDJ350MIDIController::~CDJ350MIDIController() 
{
	delete _dev;
}

void CDJ350MIDIController::Start()
{
	_startTime = _dev->Start();
}

void CDJ350MIDIController::Stop()
{
	_dev->Stop();
}

static double GetTempo(int code)
{
	double t = 1.0;
	if (code != 0x40)
	{
		double increment = 0.12 / 0x7F;
		t = 0.94 + code * increment;
	}
//	printf("tempo %f\n", t);
	return t;
}

void CDJ350MIDIController::DeviceCallback(uint32_t msg, uint32_t param, float64_t time, void *cbParam)
{
	CDJ350MIDIController *control = static_cast<CDJ350MIDIController*>(cbParam);
	time = timeGetTime() / 1000.0;
	int ch = param & 0xF;
	MsgType msgType = (MsgType)((param & 0xFF00) >> 8);
	int code = (param & 0xFF0000) >> 16;
	switch (msgType)
	{
	case JogScratch: // jog scratch
	case JogSpin: // jog spin
	//	printf("jog\n");
		control->_listener->HandleBendPitch(ch, time/* + control->_startTime*/, 0);
		break;
	case Tempo: // tempo
	//	printf("tempo\n");
		control->_listener->HandleSetPitch(ch, time/* + control->_startTime*/, GetTempo(code));
		break;
	case PlayPause: // play/pause
		if (code)
		{
	//		printf("playpause\n");
			control->_listener->HandlePlayPause(ch, time/* + control->_startTime*/);
		}
		break;
	case Cue: // play/pause
		if (code)
		{
	//		printf("cue\n");
			control->_listener->HandleCue(ch, time/* + control->_startTime*/);
		}
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
