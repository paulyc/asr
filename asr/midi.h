#ifndef _MIDI_H
#define _MIDI_H

#include "type.h"

#include <vector>
#include <string>

class IMIDIDevice;

class IMIDIListener
{
public:
};

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
    
    virtual const IMIDIDevice *GetDevice() const = 0;
};

class IMIDIDevice;

class IMIDIDeviceDescriptor
{
public:
    virtual ~IMIDIDeviceDescriptor() {}
    
    virtual std::string GetDeviceName() const = 0;
    
    virtual IMIDIDevice* Instantiate() const = 0;
};

class IMIDIDeviceFactory
{
public:
	virtual ~IMIDIDeviceFactory() {}

	virtual std::vector<const IMIDIDeviceDescriptor*> Enumerate() const = 0;
};

class IMIDIDevice
{
public:
	virtual ~IMIDIDevice() {}
    
    virtual std::vector<const IMIDIEntityDescriptor*> GetEntities() const = 0;
    virtual std::vector<const IMIDIEndpointDescriptor*> GetEndpoints() const = 0;
    
    virtual const IMIDIDeviceDescriptor *GetDesc() const = 0;
    
	typedef void (*MIDICallback)(uint32_t msg, uint32_t param, float64_t time, void *cbParam);
    
	virtual float64_t Start() = 0;
	virtual void Stop() = 0;
    
	virtual void RegisterCallback(MIDICallback cb, void *param) = 0;
};

#if MAC

#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>

class CoreMIDIDeviceDescriptor;
class CoreMIDIEndpointDescriptor;
class CoreMIDIEntityDescriptor;
class CoreMIDIDevice;
class CoreMIDIClient;

class CoreMIDIDeviceDescriptor : public IMIDIDeviceDescriptor
{
public:
    CoreMIDIDeviceDescriptor(MIDIDeviceRef dev, CoreMIDIClient &client);
    
    virtual std::string GetDeviceName() const { return _name; }
    
    virtual IMIDIDevice* Instantiate() const;
    
    virtual MIDIClientRef GetClientRef() const;
    virtual MIDIDeviceRef GetDeviceRef() const { return _devRef; }
private:
    MIDIDeviceRef _devRef;
    CoreMIDIClient &_client;
    std::string _name;
};

class CoreMIDIEntityDescriptor : public IMIDIEntityDescriptor
{
public:
    CoreMIDIEntityDescriptor(MIDIEntityRef entRef, CoreMIDIDevice &device);
    
    std::string GetEntityName() const { return _name; }
    virtual std::vector<const IMIDIEndpointDescriptor*> GetEndpoints() const;
    
    virtual MIDIClientRef GetClientRef() const;
    
    virtual const IMIDIDevice *GetDevice() const;
private:
    MIDIEntityRef _entRef;
    CoreMIDIDevice &_device;
    std::vector<CoreMIDIEndpointDescriptor> _endpoints;
    std::string _name;
};

class CoreMIDIEndpointDescriptor : public IMIDIEndpointDescriptor
{
public:
    CoreMIDIEndpointDescriptor(MIDIEndpointRef endRef, DeviceType type, CoreMIDIDevice &device);
    
    virtual DeviceType GetEndpointType() const { return _type; }
    virtual std::string GetEndpointName() const { return _name; }
    
    virtual IMIDIEndpoint* GetEndpoint() const;
    
    MIDIClientRef GetClientRef() const;
private:
    MIDIEndpointRef _endRef;
    DeviceType _type;
    CoreMIDIDevice &_device;
    std::string _name;
};

class CoreMIDIEndpoint : public IMIDIEndpoint
{
public:
    CoreMIDIEndpoint(const CoreMIDIEndpointDescriptor &desc) : _desc(desc), _listener(0) {}
    
    virtual void SetListener(IMIDIListener *listener);
private:
    static void _readProc(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon);
    
    CoreMIDIEndpointDescriptor _desc;
    IMIDIListener *_listener;
};

class CoreMIDIClient : public IMIDIDeviceFactory
{
public:
    CoreMIDIClient(CFStringRef name);
    ~CoreMIDIClient();
    
    virtual std::vector<const IMIDIDeviceDescriptor*> Enumerate() const;
    
    MIDIClientRef GetRef() const { return _ref; }
private:
    static void _notifyProc(const MIDINotification *message, void *refCon);
    
    MIDIClientRef _ref;
    std::vector<CoreMIDIDeviceDescriptor> _devices;
};

class CoreMIDIDevice : public IMIDIDevice
{
public:
    CoreMIDIDevice(const CoreMIDIDeviceDescriptor &desc);
    
    virtual std::vector<const IMIDIEntityDescriptor*> GetEntities() const;
    virtual std::vector<const IMIDIEndpointDescriptor*> GetEndpoints() const;
    
    virtual const IMIDIDeviceDescriptor *GetDesc() const { return &_desc; }
    
    virtual MIDIClientRef GetClientRef() const { return _desc.GetClientRef(); }
    
private:
    CoreMIDIDeviceDescriptor _desc;
    std::vector<CoreMIDIEntityDescriptor> _entities;
    std::vector<CoreMIDIEndpointDescriptor> _endpoints;
};

#elif WINDOWS

class Win32MIDIDeviceFactory : public IMIDIDeviceFactory
{
public:
	virtual std::vector<const IMIDIDeviceDescriptor*> Enumerate() const;
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
