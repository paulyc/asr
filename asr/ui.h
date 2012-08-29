#ifndef _UI_H
#define _UI_H

#include "config.h"
#include "io.h"

class ASIOProcessor;
class GenericUI;

struct UIWavform
{
	UIWavform() :
		playback_pos(0.0),
		px(0),cpx(0), mousedown(false) {}
	double playback_pos;
	int px, cpx;
	RECT r;
	RECT windowr;
	int width(){return r.right - r.left;}
	int height(){return r.bottom - r.top;}

	bool mousedown;
};

struct UIButton
{
	UIButton(GenericUI *ui);
	functor1<GenericUI, int> callback;
};

struct UIText
{
	UIText(GenericUI *ui, int i);
	void set_text(const wchar_t *txt);
	void set_text_pct(double v);
	void set_text_db(double db);
	functor2<GenericUI, int, const wchar_t *> callback;
	int id;
};

struct UITrack
{
	UITrack(GenericUI *ui, int tid, int pitch_id, int gain_id);
	UIWavform wave;
	void set_coarse(double v);
	void set_fine(double v);
	
	
	int id;
	double coarse_val;
	double fine_val;
	UIText pitch;
	UIText gain;
	bool clip;
    bool dirty;
};

struct UISlider
{
	virtual void set_pos(double p) = 0;
};

class MagicController
{
public:
	MagicController(ASIOProcessor *io) : _io(io), _magic_count(-1) {}
	void do_magic()
	{
		if (_magic_count == -1)
			_magic_count = 0;
		else
			_magic_count = -1; // disable if hit button again before finishing
	}
	void next_time(double t, int t_id);
protected:
	ASIOProcessor *_io;
	int _magic_count;
	double _magic_times[3];
	int _first_track;
	int _second_track;
};

class GenericUI
{
public:
	GenericUI(ASIOProcessor *io, UITrack t1, UITrack t2);

	virtual void create() = 0;
	virtual void destroy() = 0;
	virtual void main_loop() = 0;
	virtual bool running() = 0;
	virtual void do_quit() = 0;
    virtual void process_messages() {}
	virtual void render(int) = 0;
    virtual void render_dirty();
	virtual void set_track_filename(int t) = 0;
	virtual void set_position(void *t, double tm, bool invalidate) = 0;
	virtual void set_clip(int) = 0;
	virtual bool want_render() = 0;
	enum MouseButton { Left, Right, Middle };
	virtual void mouse_down(MouseButton b, int x, int y);
	virtual void mouse_up(MouseButton b, int x, int y);
	virtual void mouse_dblclick(MouseButton b, int x, int y);
	virtual void mouse_move(int x, int y);
	virtual void do_paint();
	enum SliderType { PitchCoarse, PitchFine, Gain, XfadeMaster, XfadeCue };
	virtual void slider_move(double pos, SliderType t, int x);
	virtual void set_text_field(int id, const wchar_t *txt) = 0;
	virtual void set_main_mix(double pos){}
    virtual void set_dirty(int track_id)
    {
        if (track_id == 1) 
            _track1.dirty = true; 
        else 
            _track2.dirty = true; 
    }
	virtual void do_magic()
	{
		_magic.do_magic();
	}
//protected:
	ASIOProcessor *_io;
	int _lastx, _lasty;
	UITrack _track1;
	UITrack _track2;
	MagicController _magic;
};

#if WINDOWS
class Win32UI : public GenericUI
{
public:
	Win32UI(ASIOProcessor *io);
	virtual void create();
	virtual void destroy();
	virtual void main_loop();
	virtual void process_messages();
	virtual void render(int);
	virtual void set_track_filename(int t){}
	virtual void set_position(void *t, double tm, bool invalidate);
	virtual void set_clip(int);
	virtual bool want_render(){ bool r = _want_render; _want_render = false; return r; }
	virtual void set_text_field(int id, const wchar_t *txt);
	virtual void do_quit(){_want_quit=true;}
	virtual bool running(){return !_want_quit;}
	virtual void load_track(HWND hwndDlg,WPARAM wParam,LPARAM lParam);

	bool _want_render;
	bool _want_quit;
};

struct Win32UISlider : public UISlider
{
	virtual void set_pos(double p);
};

#include <commctrl.h>
#include "resource.h"
#define SLEEP 0

extern HWND g_dlg;

INT_PTR CALLBACK MyDialogProc(HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
);

#endif // WINDOWS

#endif // !defined(UI_H)
