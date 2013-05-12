#ifndef _MIDI_H
#define _MIDI_H

#include "type.h"

#include <vector>
#include <string>

class IMIDIHandler
{
public:
};

class IMIDIDevice
{
public:
	virtual ~IMIDIDevice() {}

	typedef void (*MIDICallback)(uint32_t msg, uint32_t param, float64_t time, void *cbParam);

	virtual float64_t Start() = 0;
	virtual void Stop() = 0;

	virtual void RegisterCallback(MIDICallback cb, void *param) = 0;
};

class IMIDIListener
{
public:
};

#if WINDOWS
#include <windows.h>

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
#endif

class IMIDIEndpoint
{
public:
    virtual ~IMIDIEndpoint() {}
    
    virtual void SetListener(IMIDIListener *listener) = 0;
};

class IMIDIEndpointDescriptor
{
public:
    virtual ~IMIDIEndpointDescriptor() {}
    
    enum DeviceType { Input, Output };
    
    virtual DeviceType GetEndpointType() const = 0;
    virtual std::string GetEndpointName() const = 0;
    
    virtual IMIDIEndpoint* GetEndpoint() const = 0;
};

class IMIDIEntityDescriptor
{
public:
    virtual ~IMIDIEntityDescriptor() {}
    
    virtual std::string GetEntityName() const = 0;
    virtual std::vector<const IMIDIEndpointDescriptor*> GetEndpoints() const = 0;
};

class IMIDIDeviceDescriptor
{
public:
    virtual ~IMIDIDeviceDescriptor() {}
    
    virtual std::string GetDeviceName() const = 0;
    
    virtual std::vector<const IMIDIEntityDescriptor*> GetEntities() const = 0;
    virtual std::vector<const IMIDIEndpointDescriptor*> GetEndpoints() const = 0;
    
    virtual IMIDIDevice* Instantiate() const = 0;
};

class IMIDIDeviceFactory
{
public:
	virtual ~IMIDIDeviceFactory() {}

	virtual std::vector<const IMIDIDeviceDescriptor*> Enumerate() const = 0;
};

#if WINDOWS

class Win32MIDIDeviceFactory : public IMIDIDeviceFactory
{
public:
	virtual std::vector<const IMIDIDeviceDescriptor*> Enumerate() const;
};

#elif MAC

#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>

class CoreMIDIEndpointDescriptor;

class CoreMIDIEntityDescriptor : public IMIDIEntityDescriptor
{
public:
    CoreMIDIEntityDescriptor(MIDIEntityRef entRef);
    
    std::string GetEntityName() const { return _name; }
    virtual std::vector<const IMIDIEndpointDescriptor*> GetEndpoints() const;
private:
    MIDIEntityRef _entRef;
    std::vector<CoreMIDIEndpointDescriptor> _endpoints;
    std::string _name;
};

class CoreMIDIEndpointDescriptor : public IMIDIEndpointDescriptor
{
public:
    CoreMIDIEndpointDescriptor(MIDIEndpointRef endRef, DeviceType type);
    
    virtual DeviceType GetEndpointType() const { return _type; }
    virtual std::string GetEndpointName() const { return _name; }
    
    virtual IMIDIEndpoint* GetEndpoint() const;
private:
    MIDIEndpointRef _endRef;
    DeviceType _type;
    std::string _name;
};

class CoreMIDIEndpoint : public IMIDIEndpoint
{
public:
    CoreMIDIEndpoint(const CoreMIDIEndpointDescriptor &desc) : _desc(desc) {}
    
    virtual void SetListener(IMIDIListener *listener);
private:
    CoreMIDIEndpointDescriptor _desc;
    IMIDIListener *_listener;
};

class CoreMIDIDeviceDescriptor : public IMIDIDeviceDescriptor
{
public:
    CoreMIDIDeviceDescriptor(MIDIDeviceRef dev);
    
    virtual std::string GetDeviceName() const { return _name; }
    
    virtual std::vector<const IMIDIEntityDescriptor*> GetEntities() const;
    virtual std::vector<const IMIDIEndpointDescriptor*> GetEndpoints() const;
    
    virtual IMIDIDevice* Instantiate() const;
private:
    MIDIDeviceRef _devRef;
    std::vector<CoreMIDIEntityDescriptor> _entities;
    std::string _name;
};

class CoreMIDIDeviceFactory : public IMIDIDeviceFactory
{
public:
    CoreMIDIDeviceFactory();
    virtual ~CoreMIDIDeviceFactory();
    
    virtual std::vector<const IMIDIDeviceDescriptor*> Enumerate() const;
private:
    std::vector<CoreMIDIDeviceDescriptor> _devices;
};

class CoreMIDIClient
{
public:
    CoreMIDIClient(CFStringRef name);
    ~CoreMIDIClient();
    
    MIDIClientRef getRef() const { return _ref; }
private:
    MIDIClientRef _ref;
};

class CoreMIDIDevice : public IMIDIDevice
{
public:
    CoreMIDIDevice(const CoreMIDIDeviceDescriptor &desc) : _desc(desc), _midiClient(CFSTR("CoreMIDIDevice Client")) {}
private:
    CoreMIDIDeviceDescriptor _desc;
    CoreMIDIClient _midiClient;
};

#endif

class DummyMIDIDeviceFactory : public IMIDIDeviceFactory
{
public:
    virtual std::vector<const IMIDIDeviceDescriptor*> Enumerate() { return std::vector<const IMIDIDeviceDescriptor*>(); }
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

class IControlListener
{
public:
	virtual void HandlePlayPause(int channel, float64_t time) {}
	virtual void HandleCue(int channel, float64_t time) {}
	virtual void HandleSetPitch(int channel, float64_t time, float64_t pitch) {}
	virtual void HandleBendPitch(int channel, float64_t time, float64_t dpitch) {}
	virtual void HandleSeekTrack(int channel, float64_t time, bool backward) {}
};

class IMIDIController
{
public:
	virtual ~IMIDIController() {}

	enum Event
	{
		PlayPauseEvent,
		CueEvent,
		SetPitchEvent,
		BendPitchEvent,
		NUM_EVENTS
	};

	virtual void RegisterControlListener(IControlListener **listener) = 0;

	virtual void Start() = 0;
	virtual void Stop() = 0;
};

class CDJ350MIDIController : public IMIDIController, public IMIDIListener
{
public:
	CDJ350MIDIController(IMIDIDevice *dev, IControlListener **listener);
	~CDJ350MIDIController();

	void RegisterControlListener(IControlListener **listener) { _listener = listener; }

	static void DeviceCallback(uint32_t msg, uint32_t param, float64_t time, void *cbParam); // time is MIDI time

	void Start();
	void Stop();

	enum MsgType
	{
		PlayPause        = 0x00,
		Cue              = 0x01,
		JogScratch       = 0x10,
		JogSpin          = 0x30,
		Tempo            = 0x1d,
		SelectPush       = 0x33,
		SeekTrackBack    = 0x05,
		SeekTrackForward = 0x04
	};
	enum ButtonPush 
	{ 
		Off = 0x00, 
		On  = 0x7f 
	};

private:
	IMIDIDevice *_dev;
	float64_t _startTime;

	IControlListener **_listener;

	int _track;
};

#endif // !defined(_MIDI_H)
