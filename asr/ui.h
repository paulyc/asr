#ifndef _UI_H
#define _UI_H

#include "config.h"

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
