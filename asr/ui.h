#ifndef _UI_H
#define _UI_H

#include "config.h"

class GenericUI
{
public:
	virtual void create() = 0;
	virtual void main_loop() = 0;
	virtual void render() = 0;
	virtual void set_track_filename(int t) = 0;
	virtual void set_position(int t, double tm, bool invalidate) = 0;
};

template <typename IO_T>
class Win32UI : public GenericUI
{
public:
	Win32UI(IO_T *io):_io(io){}
	virtual void create(){}
	virtual void main_loop(){}
	virtual void render(){}
	virtual void set_track_filename(int t){}
	virtual void set_position(int t, double tm, bool invalidate){}
protected:
	IO_T *_io;
};

void set_position(double tm, bool invalidate);

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

void CreateUI();

void MainLoop_Win32();

#endif // WINDOWS

#endif // !defined(UI_H)
