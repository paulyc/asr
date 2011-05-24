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
HPEN pen_yel;
double last_time;

double coarse_val=48000.0, fine_val=0.0;

double playback_pos = 0.0;
int px = 0;
RECT r;
int width, height;

void repaint()
{
	PAINTSTRUCT ps;
	HDC hdc;
	
	HWND h;
	
	RECT r2;

	 h = ::GetDlgItem(g_dlg, IDC_STATIC4);
		//	printf("WM_PAINT %d, %d, %d, %d\n", hwndDlg, uMsg, wParam, lParam);
			hdc = BeginPaint(h, &ps);
			 
			  
			SelectObject(hdc, br);
			
			GetClientRect(h, &r);

			//GetWindowRect(h, &r2);
			
			width = r.right - r.left;
			height = r.bottom - r.top;
			if (width != asio->_track1->_display->get_width())
			{
				asio->_track1->_display->set_width(width);
				asio->_track1->_display->set_wav_heights();
			}
			Rectangle(hdc, r.top, r.left, r.right, r.bottom);
			//SetBkColor(hdc, RGB(255,255,255));

			for (int p=0; p<width; ++p)
			{
				const ASIOThinger<SamplePairf, short>::track_t::display_t::wav_height &h =
					asio->_track1->_display->get_wav_height(p);
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

			double len =  asio->_track1->len().time;
			px = int(playback_pos / len * double(width));
			MoveToEx(hdc, r.left+px, r.top, NULL);
			SelectObject(hdc, pen_yel);
			LineTo(hdc, r.left+px, r.top+height);
			
			EndPaint(h, &ps);
}

void set_position(double tm)
{
	if (asio)
	{
		playback_pos = tm;
		double len =  asio->_track1->len().time;
		int new_px = int(playback_pos / len * double(r.right - r.left));
		//if (asio)repaint();
		if (new_px != px)
		{
		InvalidateRect(::GetDlgItem(g_dlg, IDC_STATIC4), &r, TRUE);
		repaint();
		}
	}
}

INT_PTR CALLBACK MyDialogProc(HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
	switch (uMsg)
	{
		case WM_PAINT:
		{
			repaint();
			return 0L;
		}
		case WM_LBUTTONUP:
		{
			HWND h = GetDlgItem(g_dlg, IDC_STATIC4);
			POINT p;
			WINDOWINFO wi;
		//	GetCurrentPositionEx(h, &p);
		//	::GetWindowInfo(h, &wi);
		//	35,131,438,52
			//216,56
		//	if (HIWORD(lParam) >= 35 || LOWORD(lParam) >= 131)
			{
				int y = LOWORD(lParam) - 56;
				if (HIWORD(lParam) > 216 && HIWORD(lParam) < 216+height)
				{
					double f = double(y)/width;
					if (f > 0.0)
					{
						printf("%f\n", f);
						asio->_track1->seek_f(f);
					}
				}
			//	printf("%d %d %d\n", HIWORD(lParam), LOWORD(lParam), y);
			}
			break;
		}
		case WM_RBUTTONUP:
		{
		//	HWND h = GetDlgItem(g_dlg, IDC_STATIC4);
		//	POINT p;
		//	WINDOWINFO wi;
		//	GetCurrentPositionEx(h, &p);
		//	::GetWindowInfo(h, &wi);
		//	35,131,438,52
			//216,56
		//	if (HIWORD(lParam) >= 35 || LOWORD(lParam) >= 131)
			{
				int y = LOWORD(lParam) - 56;
				if (HIWORD(lParam) > 216 && HIWORD(lParam) < 216+height)
				{
					double f = double(y)/width;
					if (f > 0.0)
					{
						printf("cue %f\n", f);
						asio->_track1->set_cuepoint(f);
					}
				}
			//	printf("%d %d %d\n", HIWORD(lParam), LOWORD(lParam), y);
			}
			break;
		}
		case WM_PARENTNOTIFY:
		{
			HWND h = GetDlgItem(g_dlg, IDC_STATIC4);
			int x = IDC_STATIC4;
			break;
		}
		case WM_HSCROLL:
			switch (LOWORD(wParam))
			{
				case SB_THUMBPOSITION:
				case SB_THUMBTRACK:
					double val;
					if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER2))
					{
						printf("coarse %d ", HIWORD(wParam));
						coarse_val = 48000.0 / (1.0 + .01 * HIWORD(wParam) -0.5);
						printf("coarse_val %f\n", coarse_val);
						asio->_track1->set_output_sampling_frequency(coarse_val+fine_val); 
					}
					else
					{
						printf("fine %d ", HIWORD(wParam));
						//val = 10*60*HIWORD(wParam)*0.01;
						fine_val = 1000.0 -  20*HIWORD(wParam);
						printf("fine_val %f\n", fine_val);
						asio->_track1->set_output_sampling_frequency(coarse_val+fine_val); 
					/*	if (val != last_time) // debounce
						{
							asio->_track1->seek_time(val);
							last_time = val;
						}*/
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
						asio->_track1->set_source_file(filepath);
					//	asio->SetSrc(1, filepath);
					}
					return TRUE;
				}
				case IDC_BUTTON4: // cue src 1
				{
					asio->_track1->goto_cuepoint();
					break;
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
	RECT r;
	HDC hdc;
	PAINTSTRUCT ps;
	switch (uMsg)
	{
		case WM_PAINT:
			printf("WM_PAINT %d, %d, %d, %d\n", hwnd, uMsg, wParam, lParam);
			hdc = BeginPaint(hwnd, &ps);
			SelectObject(hdc, br);
			
			GetClientRect(hwnd, &r);
			
			Rectangle(hdc, r.top, r.left, r.right, r.bottom);
			EndPaint(hwnd, &ps);
			return 0;
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
	if (!RegisterClassEx(&wcx))
	{
		printf("Yo rcx error %d\n", GetLastError());
	}
	HWND newwnd = CreateWindowEx(0,
   L"CustomClass",
   L"Hello",
    0,
    0,
    0,
    600,
    100,
    0,
    0,
    ::GetModuleHandle(NULL),
    0);
	int e = GetLastError();
	ShowWindow(newwnd, SW_SHOW);
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
	pen_yel = CreatePen(PS_SOLID, 1, RGB(255,255,0));

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
	DeleteObject(pen_yel);
	DeleteObject(br);

#endif // !SLEEP
}
#endif // WINDOWS
