#ifndef _UI_H
#define _UI_H

#include "config.h"
#include "io.h"
#include "future.h"

class ASIOProcessor;
class GenericUI;

struct UIWavform
{
	UIWavform() :
		playback_pos(0.0),
		px(0),cpx(0), mousedown(false), mouseover(false) {}
	double playback_pos;
	int px, cpx;
	RECT r;
	RECT windowr;
	int width(){return r.right - r.left;}
	int height(){return r.bottom - r.top;}

	bool isover(int x, int y) {
		return  y >= windowr.top && y < windowr.bottom &&
				x >= windowr.left && x < windowr.right;
	}

	bool mousedown;
	bool mouseover;
};

struct UIButton
{
	UIButton(GenericUI *ui);
	functor1<GenericUI, int> callback;
};

struct UIText
{
	UIText(GenericUI *ui, int i);
	void set_text(const wchar_t *txt, bool del);
	void set_text_pct(double v);
	void set_text_db(double db);
	GenericUI *_ui;
	int id;
};

struct UITrack
{
	UITrack(GenericUI *ui, int tid, int filename_id, int pitch_id, int gain_id);
	UIWavform wave;
	void set_coarse(double v);
	void set_fine(double v);
	void update_frequency(double f);
	double get_pitch();
	
	GenericUI *_ui;
	int id;
	double coarse_val;
	double fine_val;
	UIText filename;
	UIText pitch;
	UIText gain;
	bool clip;
    bool dirty;
	bool vinyl_control;
	bool sync_time;
	bool add_pitch;
};

struct UISlider
{
	virtual void set_pos(double p) = 0;
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
	virtual void set_text_field(int id, const wchar_t *txt, bool del) = 0;
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
	virtual bool vinyl_control_enabled(int track_id)
	{
		return track_id == 1 ? _track1.vinyl_control : _track2.vinyl_control;
	}
	virtual void enable_vinyl_control(int track_id, bool enabled)
	{
		if (track_id == 1)
			_track1.vinyl_control = enabled;
		else
			_track2.vinyl_control = enabled;
	}
	virtual void set_sync_time(int track_id, bool enabled)
	{
		if (track_id == 1)
			_track1.sync_time = enabled;
		else
			_track2.sync_time = enabled;
	}
	virtual bool get_sync_time(int track_id)
	{
		return track_id == 1 ? _track1.sync_time : _track2.sync_time;
	}
	virtual void set_filters_frequency(void *filt, double freq);
	virtual void set_add_pitch(int track_id, bool enabled)
	{
		get_track(track_id).add_pitch = enabled;
	}
	virtual bool get_add_pitch(int track_id)
	{
		return track_id == 1 ? _track1.add_pitch : _track2.add_pitch;
	}
	virtual void future_task(deferred *d)
	{
		_future.submit(d);
	}
	virtual void drop_file(const wchar_t *filename, bool track1);
	UITrack& get_track(int track_id) { return track_id == 1 ? _track1 : _track2; }
	double get_track_pitch(int track_id) { return get_track(track_id).get_pitch(); }
	//protected:
	ASIOProcessor *_io;
	int _lastx, _lasty;
	UITrack _track1;
	UITrack _track2;
	MagicController _magic;
	FutureExecutor _future;

private:
#if USE_QUEUES
	std::queue<deferred*> _task_q;
#endif
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
	virtual void set_text_field(int id, const wchar_t *txt, bool del);
	virtual void do_quit(){_want_quit=true;}
	virtual bool running(){return !_want_quit;}
	virtual void load_track(HWND hwndDlg,WPARAM wParam,LPARAM lParam);

	static LONG WINAPI top_level_exception_filter(struct _EXCEPTION_POINTERS *ExceptionInfo);

	bool _want_render;
	bool _want_quit;
	HACCEL  _accelTable;

private:
	void set_text_field_impl(int id, const wchar_t *txt, bool del);
	void set_clip_impl(int t_id);
	void render_impl(int t_id);
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
