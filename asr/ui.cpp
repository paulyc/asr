#include "ui.h"
#include "io.h"

#include <cstdio>

extern ASIOThinger<SamplePairf, short> * asio;

#if WINDOWS
#include <commctrl.h>
#include "resource.h"
#define SLEEP 0

HWND g_dlg = NULL;

HBRUSH br;
HPEN pen_lt;
HPEN pen_dk;

INT_PTR CALLBACK MyDialogProc(HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
	PAINTSTRUCT ps;
	HDC hdc;
	RECT r;
	HWND h;
	int width, height;

	switch (uMsg)
	{
		case WM_PAINT:
			
			 h = ::GetDlgItem(g_dlg, IDC_STATIC4);
		//	printf("WM_PAINT %d, %d, %d, %d\n", hwndDlg, uMsg, wParam, lParam);
			hdc = BeginPaint(h, &ps);
			 
			  
			SelectObject(hdc, br);
			
			GetClientRect(h, &r);
			width = r.right - r.left;
			height = r.bottom - r.top;
			if (width != asio->_wav_display->get_width())
			{
				asio->_wav_display->set_width(width);
				asio->_wav_display->set_wav_heights();
			}
			Rectangle(hdc, r.top, r.left, r.right, r.bottom);
			//SetBkColor(hdc, RGB(255,255,255));

			for (int p=0; p<width; ++p)
			{
				const ASIOThinger<SamplePairf, short>::display_t::wav_height &h =
					asio->_wav_display->get_wav_height(p);
				int px_pk = (1.0-h.peak_top) * height / 2;
				int px_avg = (1.0-h.avg_top) * height / 2;
				int line_height = px_avg-px_pk;
				
				SelectObject(hdc, pen_lt);
				MoveToEx(hdc, r.left+p, r.top+px_pk, NULL);
				LineTo(hdc, r.left+p, r.top+px_avg);
				SelectObject(hdc, pen_dk);
				LineTo(hdc, r.left+p, r.top+height-px_avg);
				SelectObject(hdc, pen_lt);
				LineTo(hdc, r.left+p, r.top+height-px_pk);
			}
			
			EndPaint(h, &ps);
			return 0L;
		case WM_HSCROLL:
			switch (LOWORD(wParam))
			{
				case SB_THUMBPOSITION:
				case SB_THUMBTRACK:
					double val;
					if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER2))
					{
						printf("%d ", HIWORD(wParam));
						val = 48000.0 / (1.0 + .001 * HIWORD(wParam) -0.05);
						printf("%f\n", val);
						asio->SetResamplerate(val); 
					}
					else
					{
						printf("%d ", HIWORD(wParam));
						val = 10*60*HIWORD(wParam)*0.01;
						printf("%f\n", val);
						asio->SetPos(val);
					}
					break;
			}
			return TRUE;
		case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
				
                case IDOK:
					DestroyWindow(hwndDlg);
					g_dlg = NULL;
					PostMessage(NULL, WM_QUIT, 0, 0);
					return TRUE;
				case IDC_BUTTON1:
					asio->Start();
					return TRUE;
				case IDC_BUTTON2:
					asio->Stop();
					return TRUE;
				case IDC_BUTTON3: // load src 1
				{
					TCHAR filepath[256] = {0};
					TCHAR filetitle[256];
					OPENFILENAME ofn = { //typedef struct tagOFNW {
						sizeof(ofn), // DWORD        lStructSize;
						hwndDlg, //   HWND         hwndOwner;
						NULL, //   HINSTANCE    hInstance;
						TEXT("Sound Files\0*.WAV;*.MP3;*.FLAC;*.AAC;*.MP4;*.AC3\0"
						TEXT("All Files\0*.*")), //   LPCWSTR      lpstrFilter;
						NULL, //LPWSTR       lpstrCustomFilter;
						0, //   DWORD        nMaxCustFilter;
						1, //   DWORD        nFilterIndex;
						filepath, //   LPWSTR       lpstrFile;
						sizeof(filepath), //   DWORD        nMaxFile;
						filetitle, //   LPWSTR       lpstrFileTitle;
						sizeof(filetitle), //   DWORD        nMaxFileTitle;
						NULL, //   LPCWSTR      lpstrInitialDir;
						NULL, //   LPCWSTR      lpstrTitle;
						OFN_FILEMUSTEXIST, //   DWORD        Flags;
						0, //   WORD         nFileOffset;
						0, //   WORD         nFileExtension;
						NULL, //   LPCWSTR      lpstrDefExt;
						NULL, //   LPARAM       lCustData;
						NULL, //   LPOFNHOOKPROC lpfnHook;
						NULL, //   LPCWSTR      lpTemplateName;
						//#ifdef _MAC
						//   LPEDITMENU   lpEditInfo;
						//   LPCSTR       lpstrPrompt;
						//#endif
						//#if (_WIN32_WINNT >= 0x0500)
						NULL, //   void *        pvReserved;
						0, //   DWORD        dwReserved;
						0, //   DWORD        FlagsEx;
						//#endif // (_WIN32_WINNT >= 0x0500)
					};
					//memset(&ofn, 0, sizeof(ofn));
					if (GetOpenFileName(&ofn)) {
						SetDlgItemText(hwndDlg, IDC_EDIT1, filepath);
						asio->SetSrc(1, filepath);
					}
					return TRUE;
				}
			}
	}
	return FALSE;
}

LRESULT CALLBACK CustomWndProc(HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
	switch (uMsg)
	{
		case WM_PAINT:
			printf("WM_PAINT %d, %d, %d, %d\n", hwnd, uMsg, wParam, lParam);
			return TRUE;
			break;
	}
	return FALSE;
}

void CreateUI()
{
	INITCOMMONCONTROLSEX iccx;
	iccx.dwSize = sizeof(iccx);
	iccx.dwICC = ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&iccx);
	WNDCLASSEX wcx = {
		sizeof(WNDCLASSEX),
		CS_HREDRAW|CS_VREDRAW,
		CustomWndProc,
		0,
		0,
		::GetModuleHandle(NULL),
		0,
		0,
		0,
		0,
		L"CustomClass",
		0
	};
	//if (!RegisterClassEx(&wcx))
	{
	//	printf("Yo rcx error %d\n", GetLastError());
	}
	g_dlg = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), NULL, MyDialogProc);
	if (!g_dlg) {
		//TCHAR foo[256];
		//FormatMessage(dwFlags, lpSource, dwMessageId, dwLanguageId, foo, sizeof(foo), Arguments);
		//printf("Yo error %s\n", foo);
		printf("Yo error %d\n", GetLastError());
		return;
	}
	SetDlgItemText(g_dlg, IDC_EDIT1, asio->_default_src);
	ShowWindow(g_dlg, SW_SHOW);

	//printf("static = %d\n", );
	//::InvalidateRect(::GetDlgItem(g_dlg, IDC_STATIC), NULL, FALSE);
}

void MainLoop_Win32()
{
	CreateUI();

	br = CreateSolidBrush(RGB(255,255,255));
	pen_dk = CreatePen(PS_SOLID, 1, RGB(0,0,255));
	pen_lt = CreatePen(PS_SOLID, 1, RGB(0,0,192));

#if SLEEP
	Sleep(10000);
#else
	BOOL bRet;
	MSG msg;

	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) 
	{ 
		if (bRet == -1)
		{
			// Handle the error and possibly exit
			break;
		}
		else if (!IsWindow(g_dlg) || !IsDialogMessage(g_dlg, &msg)) 
		{ 
			TranslateMessage(&msg); 
			DispatchMessage(&msg); 
		} 
	} 

	DeleteObject(pen_dk);
	DeleteObject(pen_lt);
	DeleteObject(br);

#endif // !SLEEP
}
#endif // WINDOWS
