#ifndef _UI_H
#define _UI_H

#include "config.h"
#include "io.h"

class ASIOProcessor;
class GenericUI;

struct UIWavform
{
	//UIWavform(int id, int x, int y, int width, int height)
	//{
	//}
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
	functor2<GenericUI, int, const wchar_t *> callback;
	int id;
};

template <typename Chunk_T>
class SeekablePitchableFileSource;

struct UITrack
{
	//UITrack(int x, int y, int width, int height
	UITrack(GenericUI *ui, int tid, int pitch_id);
	//UITrack();
	UIWavform wave;
	void set_coarse(double v);
	void set_fine(double v);
	
	int id;
	double coarse_val;
	double fine_val;
	UIText pitch;
};

struct UISlider
{
	virtual void set_pos(double p) = 0;
};

class GenericUI
{
public:
	GenericUI(ASIOProcessor *io);

	virtual void create() = 0;
	virtual void main_loop() = 0;
	virtual void render(int) = 0;
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
	virtual void slider_move(double pos){}
	virtual void set_text_field(int id, const wchar_t *txt) = 0;
	virtual void set_main_mix(double pos){}
//protected:
	ASIOProcessor *_io;
	int _lastx, _lasty;
	UITrack _track1;
	UITrack _track2;
};

class Win32UI : public GenericUI
{
public:
	Win32UI(ASIOProcessor *io);
	virtual void create();
	virtual void main_loop();
	virtual void render(int);
	virtual void set_track_filename(int t){}
	virtual void set_position(void *t, double tm, bool invalidate);
	virtual void set_clip(int);
	virtual bool want_render(){ bool r = _want_render; _want_render = false; return r; }
	virtual void set_text_field(int id, const wchar_t *txt);

	bool _want_render;
};

struct FinePitchSlider : public UISlider
{
	//FinePitchSlider(track_t *track)
	//void set_pos(double p)

	//functor1
};

#if WINDOWS
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
