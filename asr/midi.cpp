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

#include "config.h"
#include "util.h"
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
    ItemCount nDevices = MIDIGetNumberOfDevices();
    for (ItemCount devIndx = 0; devIndx < nDevices; ++devIndx)
    {
        _devices.push_back(CoreMIDIDeviceDescriptor(MIDIGetDevice(devIndx), *this));
    }
}

CoreMIDIClient::~CoreMIDIClient()
{
    MIDIClientDispose(_ref);
}

std::vector<const IMIDIDeviceDescriptor*> CoreMIDIClient::Enumerate() const
{
    std::vector<const IMIDIDeviceDescriptor*> devices;
    for (int i=0; i<_devices.size(); ++i)
    {
        devices.push_back(&_devices[i]);
    }
    return devices;
}
    
CoreMIDIEntityDescriptor::CoreMIDIEntityDescriptor(MIDIEntityRef entRef, CoreMIDIDevice &device) :
    _entRef(entRef),
    _device(device)
{
    ItemCount nSources = MIDIEntityGetNumberOfSources(_entRef);
    for (ItemCount srcIndx = 0; srcIndx < nSources; ++srcIndx)
    {
        _endpoints.push_back(CoreMIDIEndpointDescriptor(MIDIEntityGetSource(entRef, srcIndx), IMIDIEndpointDescriptor::Input, _device));
    }
    
    ItemCount nDests = MIDIEntityGetNumberOfDestinations(_entRef);
    for (ItemCount dstIndx = 0; dstIndx < nDests; ++dstIndx)
    {
        _endpoints.push_back(CoreMIDIEndpointDescriptor(MIDIEntityGetDestination(entRef, dstIndx), IMIDIEndpointDescriptor::Output, _device));
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

MIDIClientRef CoreMIDIEntityDescriptor::GetClientRef() const
{
    return _device.GetClientRef();
}

const IMIDIDevice *CoreMIDIEntityDescriptor::GetDevice() const
{
    return &_device;
}

CoreMIDIEndpointDescriptor::CoreMIDIEndpointDescriptor(MIDIEndpointRef endRef, DeviceType type, CoreMIDIDevice &device) :
    _endRef(endRef),
    _type(type),
    _device(device)
{
    CFStringRef name;
    MIDIObjectGetStringProperty(_endRef, kMIDIPropertyName, &name);
    _name = CFStringRefToString(name);
}

MIDIClientRef CoreMIDIEndpointDescriptor::GetClientRef() const
{
    return _device.GetClientRef();
}

IMIDIEndpoint* CoreMIDIEndpointDescriptor::GetEndpoint() const
{
    return new CoreMIDIEndpoint(*this);
}

CoreMIDIEndpoint::CoreMIDIEndpoint(const CoreMIDIEndpointDescriptor &desc) : _desc(desc), _listener(0), _started(false)
{
    _desc.GetDevice().RegisterEndpoint(this);
    OSStatus stat = MIDIInputPortCreate(_desc.GetClientRef(), CFSTR("MIDI read port"), _readProc, this, &_port);
}

CoreMIDIEndpoint::~CoreMIDIEndpoint()
{
    Stop();
    MIDIPortDispose(_port);
}

void CoreMIDIEndpoint::SetListener(IMIDIListener *listener)
{
    _listener = listener;
}

void CoreMIDIEndpoint::_readProc(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon)
{
    const MIDIPacket *pkt = &pktlist->packet[0];
    for (UInt32 i = 0; i < pktlist->numPackets; ++i)
    {
        for (int byte = 0; byte < pkt->length;)
        {
            IMIDIListener *listener = (IMIDIListener*)srcConnRefCon;
            listener->ProcessMsg(pkt->data[byte++],
                                 pkt->data[byte++],
                                 pkt->data[byte++]);
        }
        pkt = MIDIPacketNext(pkt);
    }
}

void CoreMIDIEndpoint::Start()
{
    if (!_started)
    {
        _started = true;
        MIDIPortConnectSource(_port, _desc.GetEndpointRef(), _listener);
    }
}

void CoreMIDIEndpoint::Stop()
{
    if (_started)
    {
        _started = false;
        MIDIPortDisconnectSource(_port, _desc.GetEndpointRef());
    }
}

CoreMIDIDeviceDescriptor::CoreMIDIDeviceDescriptor(MIDIDeviceRef devRef, CoreMIDIClient &client) : _devRef(devRef), _client(client)
{
    CFStringRef name;
    MIDIObjectGetStringProperty(_devRef, kMIDIPropertyName, &name);
    _name = CFStringRefToString(name);
}

CoreMIDIDevice::CoreMIDIDevice(const CoreMIDIDeviceDescriptor &desc) : _desc(desc)
{
    ItemCount nEntities = MIDIDeviceGetNumberOfEntities(desc.GetDeviceRef());
    for (ItemCount entIndx = 0; entIndx < nEntities; ++entIndx)
    {
        _entities.push_back(CoreMIDIEntityDescriptor(MIDIDeviceGetEntity(desc.GetDeviceRef(), entIndx), *this));
    }
    
    ItemCount nSources = MIDIGetNumberOfSources();
    for (ItemCount srcIndx = 0; srcIndx < nSources; ++srcIndx)
    {
        _endpoints.push_back(CoreMIDIEndpointDescriptor(MIDIGetSource(srcIndx), IMIDIEndpointDescriptor::Input, *this));
    }
    
    ItemCount nDests = MIDIGetNumberOfDestinations();
    for (ItemCount dstIndx = 0; dstIndx < nDests; ++dstIndx)
    {
        _endpoints.push_back(CoreMIDIEndpointDescriptor(MIDIGetDestination(dstIndx), IMIDIEndpointDescriptor::Output, *this));
    }
}

std::vector<const IMIDIEntityDescriptor*> CoreMIDIDevice::GetEntities() const
{
    std::vector<const IMIDIEntityDescriptor*> entities;
    for (int i=0; i<_entities.size(); ++i)
    {
        entities.push_back(&_entities[i]);
    }
    return entities;
}

std::vector<const IMIDIEndpointDescriptor*> CoreMIDIDevice::GetEndpoints() const
{
    std::vector<const IMIDIEndpointDescriptor*> endpoints;
    for (int i=0; i<_endpoints.size(); ++i)
    {
        endpoints.push_back(&_endpoints[i]);
    }
    return endpoints;
}

IMIDIDevice* CoreMIDIDeviceDescriptor::Instantiate() const
{
    return new CoreMIDIDevice(*this);
}

MIDIClientRef CoreMIDIDeviceDescriptor::GetClientRef() const
{
    return _client.GetRef();
}

float64_t CoreMIDIDevice::Start()
{
    for (int i=0; i < _endpointRegister.size(); ++i)
        _endpointRegister[i]->Start();
    return 0.0;
}

void CoreMIDIDevice::Stop()
{
    for (int i=0; i < _endpointRegister.size(); ++i)
        _endpointRegister[i]->Stop();
}

#endif // MAC

CDJ350MIDIController::CDJ350MIDIController(IMIDIDevice *dev) :
	_dev(dev),
    _endp(0),
	_listener(0),
	_track(0)
{
    std::vector<const IMIDIEndpointDescriptor*> endpoints = dev->GetEndpoints();
    for (int i=0; i<endpoints.size(); ++i)
    {
        if (endpoints[i]->GetEndpointType() == IMIDIEndpointDescriptor::Input &&
            endpoints[i]->GetEndpointName() == std::string("PIONEER CDJ-350"))
        {
            _endp = endpoints[i]->GetEndpoint();
            _endp->SetListener(this);
            break;
        }
    }
}

CDJ350MIDIController::~CDJ350MIDIController() 
{
    Stop();
    delete _endp;
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

void CDJ350MIDIController::ProcessMsg(uint8_t status, uint8_t data1, uint8_t data2)
{
#if WINDOWS
	float64_t time = timeGetTime() / 1000.0;
#else
    float64_t time = 0.0;
#endif
	int ch = status & 0xF;
	MsgType msgType = (MsgType)(data1);
	int code = data2;
	switch (msgType)
	{
        case JogScratch: // jog scratch
        case JogSpin: // jog spin
            //	printf("jog\n");
            _listener->HandleBendPitch(_track, time/* + control->_startTime*/, GetBend(code));
            break;
        case Tempo: // tempo
            //	printf("tempo\n");
            _listener->HandleSetPitch(_track, time/* + control->_startTime*/, GetTempo(code));
            break;
        case PlayPause: // play/pause
            if (code)
            {
                //		printf("playpause\n");
                _listener->HandlePlayPause(_track, time/* + control->_startTime*/);
            }
            break;
        case Cue: // play/pause
            if (code)
            {
                //		printf("cue\n");
                _listener->HandleCue(_track, time/* + control->_startTime*/);
            }
            break;
        case SelectPush:
            if (code)
            {
                _track ^= 1;
                _listener->HandleSwitchTrack(_track);
            }
            break;
        case SeekTrackBack:
            if (code)
            {
                _listener->HandleSeekTrack(_track, time, true);
            }
            break;
        case SeekTrackForward:
            if (code)
            {
                _listener->HandleSeekTrack(_track, time, false);
            }
            break;
        default:
            printf("%x %x %x %f\n", status, data1, data2, time+_startTime);
	}
}

void CDJ350MIDIController::DeviceCallback(uint32_t msg, uint32_t param, float64_t time, void *cbParam)
{
	CDJ350MIDIController *control = static_cast<CDJ350MIDIController*>(cbParam);
    control->ProcessMsg(msg & 0xFF, (msg & 0xFF00) >> 8, (msg & 0xFF0000) >> 16);
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
