#include "midi.h"

#if WINDOWS

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

#elif MAC

CoreMIDIClient::CoreMIDIClient(CFStringRef name)
{
    MIDIClientCreate(name, NULL, this, &_ref);
}

CoreMIDIClient::~CoreMIDIClient()
{
    MIDIClientDispose(&_ref);
}

CoreMIDIDeviceFactory::CoreMIDIDeviceFactory()
{
    ItemCount nDevices = MIDIGetNumberOfDevices();
    for (ItemCount devIndx = 0; devIndx < nDevices; ++devIndx)
    {
        _devices.push_back(new CoreMIDIDeviceDescriptor(MIDIGetDevice(devIndx)));
    }
}

CoreMIDIDeviceFactory::~CoreMIDIDeviceFactory()
{
    for (size_t i = 0; i < _devices.size(); ++i)
        delete _devices[i];
}
                            
CoreMIDIEntityDescriptor::CoreMIDIEntityDescriptor(MIDIEntityRef entRef) : _entRef(entRef)
{
    ItemCount nSources = MIDIEntityGetNumberOfSources(_entRef);
    for (ItemCount srcIndx = 0; srcIndx < nSources; ++srcIndx)
    {
        _endpoints.push_back(CoreMIDIEndpointDescriptor(MIDIEntityGetSource(entRef, srcIndx), IMIDIEndpointDescriptor::Input));
    }
    
    ItemCount nDests = MIDIEntityGetNumberOfDestinations(_entRef);
    for (ItemCount dstIndx = 0; dstIndx < nDests; ++dstIndx)
    {
        _endpoints.push_back(CoreMIDIEndpointDescriptor(MIDIEntityGetDestination(entRef, dstIndx), IMIDIEndpointDescriptor::Output));
    }
    
    CFStringRef name;
    MIDIObjectGetStringProperty(_entRef, kMIDIPropertyName, &name);
    _name = CFStringRefToString(name);
}

std::vector<const IMIDIEndpointDescriptor*> CoreMIDIEntityDescriptor::GetEndpoints() const
{
    std::vector<const IMIDIEndpointDescriptor*> endpoints;
    for (size_t i=0; i<_endpoints.size(); ++i)
    {
        endpoints.push_back(&_endpoints[i]);
    }
    return endpoints;
}

CoreMIDIEndpointDescriptor::CoreMIDIEndpointDescriptor(MIDIEndpointRef endRef, DeviceType type) :
    _endRef(endRef),
    _type(type),
    _listener(0)
{
    CFStringRef name;
    MIDIObjectGetStringProperty(_endRef, kMIDIPropertyName, &name);
    _name = CFStringRefToString(name);
}

IMIDIEndpoint* CoreMIDIEndpointDescriptor::GetEndpoint() const
{
    return new CoreMIDIEndpoint(*this);
}

CoreMIDIDeviceDescriptor::CoreMIDIDeviceDescriptor(MIDIDeviceRef devRef) : _devRef(devRef)
{
    ItemCount nEntities = MIDIDeviceGetNumberOfEntities(_devRef);
    for (ItemCount entIndx = 0; entIndx < nEntities; ++entIndx)
    {
        _entities.push_back(CoreMIDIEntityDescriptor(MIDIDeviceGetEntity(_devRef, entIndx)));
    }
    
    CFStringRef name;
    MIDIObjectGetStringProperty(_devRef, kMIDIPropertyName, &name);
    _name = CFStringRefToString(name);
}

std::vector<const IMIDIEntityDescriptor*> CoreMIDIDeviceDescriptor::GetEntities() const
{
    
}

std::vector<const IMIDIEndpointDescriptor*> CoreMIDIDeviceDescriptor::GetEndpoints() const
{
    
}

virtual IMIDIDevice* CoreMIDIDeviceDescriptor::Instantiate() const
{
    
}



#endif // MAC

CDJ350MIDIController::CDJ350MIDIController(IMIDIDevice *dev, IControlListener **listener) :
	_dev(dev),
	_listener(listener),
	_track(0)
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

static double GetBend(int code)
{
	double dt = 0.0;
	if (code != 0x40)
	{
		double increment = 0.8 / 0x7F;
		dt = -0.4 + code * increment;
	}
	return dt;
}

void CDJ350MIDIController::DeviceCallback(uint32_t msg, uint32_t param, float64_t time, void *cbParam)
{
	CDJ350MIDIController *control = static_cast<CDJ350MIDIController*>(cbParam);
#if WINDOWS
	time = timeGetTime() / 1000.0;
#else
    time = 0.0;
#endif
	int ch = param & 0xF;
	MsgType msgType = (MsgType)((param & 0xFF00) >> 8);
	int code = (param & 0xFF0000) >> 16;
	switch (msgType)
	{
	case JogScratch: // jog scratch
	case JogSpin: // jog spin
	//	printf("jog\n");
		(*control->_listener)->HandleBendPitch(control->_track, time/* + control->_startTime*/, GetBend(code));
		break;
	case Tempo: // tempo
	//	printf("tempo\n");
		(*control->_listener)->HandleSetPitch(control->_track, time/* + control->_startTime*/, GetTempo(code));
		break;
	case PlayPause: // play/pause
		if (code)
		{
	//		printf("playpause\n");
			(*control->_listener)->HandlePlayPause(control->_track, time/* + control->_startTime*/);
		}
		break;
	case Cue: // play/pause
		if (code)
		{
	//		printf("cue\n");
			(*control->_listener)->HandleCue(control->_track, time/* + control->_startTime*/);
		}
		break;
	case SelectPush:
		if (code)
		{
			control->_track ^= 1;
		}
		break;
	case SeekTrackBack:
		if (code)
		{
			(*control->_listener)->HandleSeekTrack(control->_track, time, true);
		}
		break;
	case SeekTrackForward:
		if (code)
		{
			(*control->_listener)->HandleSeekTrack(control->_track, time, false);
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
