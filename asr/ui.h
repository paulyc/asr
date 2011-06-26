#ifndef _UI_H
#define _UI_H

#include "config.h"
#include "io.h"

class GenericUI
{
public:
	virtual void create() = 0;
	virtual void main_loop() = 0;
	virtual void render(int) = 0;
	virtual void set_track_filename(int t) = 0;
	virtual void set_position(void *t, double tm, bool invalidate) = 0;
	virtual void set_clip(int) = 0;
};

template <typename IO_T>
class Win32UI : public GenericUI
{
public:
	Win32UI(IO_T *io):_io(io){}
	virtual void create();
	virtual void main_loop();
	virtual void render(int);
	virtual void set_track_filename(int t){}
	virtual void set_position(void *t, double tm, bool invalidate);
	virtual void set_clip(int);
protected:
	IO_T *_io;
};

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
};

struct UITrack
{
	//UITrack(int x, int y, int width, int height
	UITrack() : coarse_val(48000.0), fine_val(0.0){}
	UIWavform wave;
	double coarse_val;
	double fine_val;
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
