#include "config.h"
#include "ui.h"

#include <cstdio>

typedef ASIOProcessor ASIOP;
typedef ASIOP::track_t track_t;

#if WINDOWS
#include <commctrl.h>
#include <wingdi.h>
#include "resource.h"
#define SLEEP 0

HWND g_dlg = NULL;
HWND g_newwnd = NULL;

HBRUSH br;
HPEN pen_lt;
HPEN pen_dk;
HPEN pen_yel;
HPEN pen_oran;
double last_time;

INT_PTR CALLBACK MyDialogProc(HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
	ASIOProcessor *asio = ASR::get_io_instance();
	GenericUI *ui = asio->get_ui();

	switch (uMsg)
	{
		case WM_PAINT:
		{
			ui->do_paint();
			return 0L;
		}
		case WM_LBUTTONDOWN:
		{
			ui->mouse_down(GenericUI::Left, LOWORD(lParam), HIWORD(lParam)+45);
			break;
		}
		case WM_LBUTTONDBLCLK:
		{
			ui->mouse_dblclick(GenericUI::Left, LOWORD(lParam), HIWORD(lParam)+45);
			break;
		}
		case WM_LBUTTONUP:
		{
			ui->mouse_up(GenericUI::Left, LOWORD(lParam), HIWORD(lParam)+45);
			break;
		}
		case WM_MOUSEMOVE:
		{
			ui->mouse_move(LOWORD(lParam), HIWORD(lParam)+45);
			break;
		}
		case WM_RBUTTONUP:
		{
			ui->mouse_up(GenericUI::Right, LOWORD(lParam), HIWORD(lParam)+45);
			break;
		}
		case WM_HSCROLL:
			switch (LOWORD(wParam))
			{
				wchar_t buf[64];
				case TB_THUMBPOSITION:
				case TB_THUMBTRACK:
				case TB_ENDTRACK:
					if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER2))
					{
						DWORD dwPos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0); 
						ui->_track1.set_coarse(dwPos/1000.);
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER3))
					{
						DWORD dwPos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0); 
						ui->_track1.set_fine(dwPos/1000.);
					
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER4))
					{
						DWORD dwPos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0); 
						ui->_track2.set_coarse(dwPos/1000.);
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER5))
					{
						DWORD dwPos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0); 
						ui->_track2.set_fine(dwPos/1000.);
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER1))
					{
						DWORD dwPos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0); 
					//	printf("dwPos %d\n", dwPos);
						asio->SetMix(dwPos);
						break; 
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER8))
					{
						DWORD dwPos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0); 
						asio->_cue->set_mix(dwPos);
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER6))
					{
						DWORD dwPos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0); 
						ui->slider_move(dwPos/1000., GenericUI::Gain, 1);
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER7))
					{
						DWORD dwPos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0); 
						ui->slider_move(dwPos/1000., GenericUI::Gain, 2);
					}
					break;
			}
			return TRUE;
		case WM_KEYUP:
			break;
		case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
			/*	case IDC_CHECK1:
					if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED)
						asio->_bus_matrix->map(asio->GetTrack(1), asio->_master_bus);
					else
						asio->_bus_matrix->unmap(asio->GetTrack(1), asio->_master_bus);
					return TRUE;
				case IDC_CHECK2:
					if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED)
						asio->_bus_matrix->map(asio->GetTrack(1), asio->_cue_bus);
					else
						asio->_bus_matrix->unmap(asio->GetTrack(1), asio->_cue_bus);
					return TRUE;
				case IDC_CHECK5:
					if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED)
						asio->_bus_matrix->map(asio->GetTrack(1), asio->_aux_bus);
					else
						asio->_bus_matrix->unmap(asio->GetTrack(1), asio->_aux_bus);
					return TRUE;
				case IDC_CHECK3:
					if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED)
						asio->_bus_matrix->map(asio->GetTrack(2), asio->_master_bus);
					else
						asio->_bus_matrix->unmap(asio->GetTrack(2), asio->_master_bus);
					return TRUE;
				case IDC_CHECK4:
					if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED)
						asio->_bus_matrix->map(asio->GetTrack(2), asio->_cue_bus);
					else
						asio->_bus_matrix->unmap(asio->GetTrack(2), asio->_cue_bus);
					return TRUE;
				case IDC_CHECK6:
					if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED)
						asio->_bus_matrix->map(asio->GetTrack(2), asio->_aux_bus);
					else
						asio->_bus_matrix->unmap(asio->GetTrack(2), asio->_aux_bus);
					return TRUE;*/
					
				case IDOK:
					//kill worker thread
					DestroyWindow(hwndDlg);
					g_dlg = NULL;
					PostQuitMessage(0);
					ui->do_quit();
					return TRUE;
				case IDC_BUTTON1:
					asio->Start();
					return TRUE;
				case IDC_BUTTON2:
					asio->Stop();
					return TRUE;
				case IDC_BUTTON3: // load src
				case IDC_BUTTON6:
				case IDC_BUTTON25:
				{
					dynamic_cast<Win32UI*>(ui)->load_track(hwndDlg, wParam, lParam);
					return TRUE;
				}
				case IDC_BUTTON4: // cue src
				case IDC_BUTTON7:
				{
					asio->GetTrack(LOWORD(wParam)==IDC_BUTTON4 ? 1 : 2)->goto_cuepoint();
					break;
				}
				case IDC_BUTTON5: // play/pause
				case IDC_BUTTON8:
				{
					asio->GetTrack(LOWORD(wParam)==IDC_BUTTON5 ? 1 : 2)->play_pause();
					break;
				}
				case IDC_BUTTON9:
				case IDC_BUTTON11:
				{
					asio->GetTrack(LOWORD(wParam)==IDC_BUTTON9 ? 1 : 2)->set_pitchpoint();
					break;
				}
				case IDC_BUTTON10:
				case IDC_BUTTON12:
				{
					TCHAR buf[64];
					asio->GetTrack(LOWORD(wParam)==IDC_BUTTON10 ? 1 : 2)->goto_pitchpoint();
					swprintf(buf, L"%.02f%%", (48000.0 / asio->GetTrack(LOWORD(wParam)==IDC_BUTTON10 ? 1 : 2)->get_pitchpoint() - 1.0)*100.0);
					SetDlgItemText(g_dlg, LOWORD(wParam)==IDC_BUTTON10 ? IDC_EDIT3 : IDC_EDIT4, 
						buf);
					break;
				}
				case IDC_BUTTON13:
				case IDC_BUTTON14:
				case IDC_BUTTON15:
				case IDC_BUTTON16:
				{
					int t_id = LOWORD(wParam)==IDC_BUTTON13||LOWORD(wParam)==IDC_BUTTON14 ? 1 : 2;
					double dt = LOWORD(wParam)==IDC_BUTTON13 || LOWORD(wParam)==IDC_BUTTON15 ? -0.01 : 0.01;
					asio->GetTrack(t_id)->nudge_time(dt);
					break;
				}
				case IDC_BUTTON17:
				case IDC_BUTTON18:
				case IDC_BUTTON19:
				case IDC_BUTTON20:
				{
					TCHAR buf[64];
					int t_id = LOWORD(wParam)==IDC_BUTTON17||LOWORD(wParam)==IDC_BUTTON18 ? 1 : 2;
					double dp = LOWORD(wParam)==IDC_BUTTON17 || LOWORD(wParam)==IDC_BUTTON19 ? -0.0001 : 0.0001;
					asio->GetTrack(t_id)->nudge_pitch(dp);
					swprintf(buf, L"%.02f%%",  (asio->GetTrack(t_id)->get_pitch() - 1.)*100.);
					SetDlgItemTextW(hwndDlg, t_id==1?IDC_EDIT3:IDC_EDIT4, buf);
					break;
				}
				case IDC_RADIO1:
				case IDC_RADIO2:
				case IDC_RADIO3:
				{
					if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) != BST_CHECKED)
					{
						SendMessage((HWND)lParam, BM_SETCHECK, BST_CHECKED, 0);
						if (LOWORD(wParam)!=IDC_RADIO1)
							SendMessage(GetDlgItem(g_dlg, IDC_RADIO1), BM_SETCHECK, 0, 0);
						else
							asio->set_source(ASIOP::Main, ASIOP::Master);
						if (LOWORD(wParam)!=IDC_RADIO2)
							SendMessage(GetDlgItem(g_dlg, IDC_RADIO2), BM_SETCHECK, 0, 0);
						else
							asio->set_source(ASIOP::Main, ASIOP::Cue);
						if (LOWORD(wParam)!=IDC_RADIO3)
							SendMessage(GetDlgItem(g_dlg, IDC_RADIO3), BM_SETCHECK, 0, 0);
						else
							asio->set_source(ASIOP::Main, ASIOP::Aux);
					}
					break;
				}
				case IDC_RADIO5:
				case IDC_RADIO6:
				case IDC_RADIO7:
				case IDC_RADIO8:
				{
					if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) != BST_CHECKED)
					{
						SendMessage((HWND)lParam, BM_SETCHECK, BST_CHECKED, 0);
						if (LOWORD(wParam)!=IDC_RADIO5)
							SendMessage(GetDlgItem(g_dlg, IDC_RADIO5), BM_SETCHECK, 0, 0);
						else
							asio->set_source(ASIOP::File, ASIOP::Off);

						if (LOWORD(wParam)!=IDC_RADIO6)
							SendMessage(GetDlgItem(g_dlg, IDC_RADIO6), BM_SETCHECK, 0, 0);
						else
							asio->set_source(ASIOP::File, ASIOP::Master);

						if (LOWORD(wParam)!=IDC_RADIO7)
							SendMessage(GetDlgItem(g_dlg, IDC_RADIO7), BM_SETCHECK, 0, 0);
						else
							asio->set_source(ASIOP::File, ASIOP::Cue);

						if (LOWORD(wParam)!=IDC_RADIO8)
							SendMessage(GetDlgItem(g_dlg, IDC_RADIO8), BM_SETCHECK, 0, 0);
						else
							asio->set_source(ASIOP::File, ASIOP::Aux);
					}
					break;
				}
				case IDC_BUTTON26:
					ui->do_magic();
					break;
				case IDC_CHECK9:
					asio->set_sync_cue(SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_CHECK10:
					ui->enable_vinyl_control(1, SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_CHECK11:
					ui->enable_vinyl_control(2, SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_CHECK12:
					ui->set_sync_time(1, SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_CHECK13:
					ui->set_sync_time(2, SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_CHECK14:
					ui->set_add_pitch(1, SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_CHECK15:
					ui->set_add_pitch(2, SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
			}
			return TRUE;
	}
	return FALSE;
}

void Win32UI::load_track(HWND hwndDlg,WPARAM wParam,LPARAM lParam)
{
	TCHAR filepath[512] = {0};
	TCHAR filetitle[512];
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
		LOWORD(wParam)!=IDC_BUTTON25?OFN_FILEMUSTEXIST:NULL, //   DWORD        Flags;
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
		if (LOWORD(wParam)!=IDC_BUTTON25)
		{
			SetDlgItemText(hwndDlg, LOWORD(wParam)==IDC_BUTTON3 ? IDC_EDIT1 : IDC_EDIT2, filepath);
			_io->GetTrack(LOWORD(wParam)==IDC_BUTTON3 ? 1 : 2)->set_source_file(filepath, &_io->_io_lock);
		}
		else
		{
			_io->set_file_output(filepath);
		}
	//	asio->SetSrc(1, filepath);
	}
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
		case WM_CREATE:
			return 0;
			break;
		case WM_PAINT:
			printf("WM_PAINT %d, %d, %d, %d\n", hwnd, uMsg, wParam, lParam);
			hdc = BeginPaint(hwnd, &ps);
		//	wglCreateContext(hdc);
			SelectObject(hdc, br);
			
			GetClientRect(hwnd, &r);
			
			Rectangle(hdc, r.top, r.left, r.right, r.bottom);
			EndPaint(hwnd, &ps);
			return 0;
			break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

Win32UI::Win32UI(ASIOProcessor *io) :
	GenericUI(io, 
		UITrack(this, 1, IDC_EDIT3, IDC_EDIT5), 
		UITrack(this, 2, IDC_EDIT4, IDC_EDIT6)),
	_want_render(false),
	_want_quit(false)
{}

void Win32UI::create()
{
#if 0
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
	ATOM a;
	if (!(a=RegisterClassEx(&wcx)))
	{
		printf("Yo rcx error %d\n", GetLastError());
	}
	g_newwnd = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW|WS_EX_COMPOSITED,
   (LPCWSTR)a,
   L"Hello",
    WS_OVERLAPPEDWINDOW,
    0,
    0,
    600,
    600,
    0,
    0,
    ::GetModuleHandle(NULL),
    0);
	int e = GetLastError();
	ShowWindow(g_newwnd, SW_SHOW);
	UpdateWindow(g_newwnd);
#endif

	HWND button = CreateWindowEx(0, 
		L"Button", 
		L"Text", 
		BS_PUSHBUTTON|BS_TEXT|WS_TABSTOP|WS_CHILD|WS_VISIBLE, 
		100, 100, 100, 100, 
		g_newwnd, 
		0, ::GetModuleHandle(NULL),
		0);
//	ShowWindow(button, SW_SHOW);
	
	g_dlg = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), NULL, MyDialogProc);
	if (!g_dlg) {
		//TCHAR foo[256];
		//FormatMessage(dwFlags, lpSource, dwMessageId, dwLanguageId, foo, sizeof(foo), Arguments);
		//printf("Yo error %s\n", foo);
		printf("Yo error %d\n", GetLastError());
		return;
	}
	SetDlgItemText(g_dlg, IDC_EDIT1, _io->_default_src);
	ShowWindow(g_dlg, SW_SHOW);

	HWND h;
	h = ::GetDlgItem(g_dlg, IDC_STATIC4);
	GetClientRect(h, &_track1.wave.r);
	GetWindowRect(h, &_track1.wave.windowr);
	h = ::GetDlgItem(g_dlg, IDC_STATIC5);
	GetClientRect(h, &_track2.wave.r);
	GetWindowRect(h, &_track2.wave.windowr);

	/*
	HDC hdc = GetDC(GetDlgItem(g_dlg, IDC_STATIC));
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW|PFD_DOUBLEBUFFER,
		24,
		0,0,0,0,0,0,
		0,
		0,
		0,
		0,
		0,0,0,0,
		32,0,0,PFD_MAIN_PLANE,0,0,0,0
	};
	int ipf = ChoosePixelFormat(hdc, &pfd);
	SetPixelFormat(hdc, ipf, &pfd);
	ReleaseDC(GetDlgItem(g_dlg, IDC_STATIC), hdc);
*/
	//printf("static = %d\n", );
	//::InvalidateRect(::GetDlgItem(g_dlg, IDC_STATIC), NULL, FALSE);
	HWND hwndCombo = ::GetDlgItem(g_dlg, IDC_COMBO1);
//	RECT r = {0, 0, 
//	SendMessage(hwndCombo, CB_GETDROPPEDCONTROLRECT, 0, (LPARAM)&rect);
	SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)L"Master");
	SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)L"Cue");
	SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)L"Aux");
	SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
	hwndCombo = ::GetDlgItem(g_dlg, IDC_COMBO2);
	SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)L"");
	SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)L"Master");
	SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)L"Cue");
	SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)L"Aux");
	SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
	SendMessage(GetDlgItem(g_dlg, IDC_CHECK1), BM_SETCHECK, BST_CHECKED, BST_CHECKED);
	SendMessage(GetDlgItem(g_dlg, IDC_CHECK3), BM_SETCHECK, BST_CHECKED, BST_CHECKED);
	HWND hwndSlider = GetDlgItem(g_dlg, IDC_SLIDER1);
	SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0,1000)); 
	//SendMessage(hwndSlider, TBM_SETPOS, TRUE, 500); 
	hwndSlider = GetDlgItem(g_dlg, IDC_SLIDER8);
	SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0,1000)); 
	SendMessage(hwndSlider, TBM_SETPOS, TRUE, 1000); 
	_io->_cue->set_mix(1000);
	hwndSlider = GetDlgItem(g_dlg, IDC_SLIDER2);
	SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0,1000)); 
	SendMessage(hwndSlider, TBM_SETPOS, TRUE, 500); 
	hwndSlider = GetDlgItem(g_dlg, IDC_SLIDER3);
	SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0,1000)); 
	SendMessage(hwndSlider, TBM_SETPOS, TRUE, 500); 
	hwndSlider = GetDlgItem(g_dlg, IDC_SLIDER4);
	SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0,1000)); 
	SendMessage(hwndSlider, TBM_SETPOS, TRUE, 500); 
	hwndSlider = GetDlgItem(g_dlg, IDC_SLIDER5);
	SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0,1000)); 
	SendMessage(hwndSlider, TBM_SETPOS, TRUE, 500); 
	hwndSlider = GetDlgItem(g_dlg, IDC_SLIDER6);
	SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0,1000)); 
	SendMessage(hwndSlider, TBM_SETPOS, TRUE, 800); 
	hwndSlider = GetDlgItem(g_dlg, IDC_SLIDER7);
	SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0,1000)); 
	SendMessage(hwndSlider, TBM_SETPOS, TRUE, 800); 
	SetDlgItemText(g_dlg, IDC_EDIT5, L"0.00 dB");
	SetDlgItemText(g_dlg, IDC_EDIT6, L"0.00 dB");
	SendMessage(GetDlgItem(g_dlg, IDC_RADIO1), BM_SETCHECK, BST_CHECKED, 0);
	SendMessage(GetDlgItem(g_dlg, IDC_RADIO5), BM_SETCHECK, BST_CHECKED, 0);

	//#if USE_NEW_WAVE
	_io->GetTrack(1)->set_display_width(_track1.wave.width());
	_io->GetTrack(2)->set_display_width(_track2.wave.width());
//#endif

	br = CreateSolidBrush(RGB(255,255,255));
	pen_dk = CreatePen(PS_SOLID, 1, RGB(0,0,255));
	pen_lt = CreatePen(PS_SOLID, 1, RGB(0,0,192));
	pen_yel = CreatePen(PS_SOLID, 1, RGB(255,255,0));
	pen_oran = CreatePen(PS_SOLID, 1, RGB(255,128,0));

	//asio->GetTrack(1)->lockedpaint();
	//asio->GetTrack(2)->lockedpaint();

	_accelTable = LoadAccelerators(::GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_ACCELERATOR2));
	_track1.pitch.set_text_pct(0.0);
	_track2.pitch.set_text_pct(0.0);
}

void Win32UI::destroy()
{
	DeleteObject(pen_dk);
	DeleteObject(pen_lt);
	DeleteObject(pen_yel);
	DeleteObject(pen_oran);
	DeleteObject(br);
}

void Win32UI::main_loop()
{
	create();

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
		
		if (!IsWindow(g_dlg) || !IsDialogMessage(g_dlg, &msg)) 
		{ 
			TranslateMessage(&msg); 
			DispatchMessage(&msg); 
		} 
		else
		{
			TranslateAccelerator(g_dlg, _accelTable, &msg);
		}
	} 

	destroy();

#endif // !SLEEP
}

void Win32UI::process_messages()
{
	BOOL bRet;
	MSG msg;

	while ((bRet = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) != 0) 
	{
		if (msg.message == WM_QUIT)
		{

		}
		if (!IsWindow(g_dlg) || !IsDialogMessage(g_dlg, &msg)) 
		{ 
			TranslateMessage(&msg); 
			DispatchMessage(&msg); 
		} 
	} 
}

void Win32UI::set_position(void *t, double p, bool invalidate)
{
	if (_io)
	{
		UITrack *uit = (t == _io->GetTrack(1) ? &_track1 : &_track2);
		//if (p >= 0.0 && p <= 1.0)
		//{
			uit->wave.playback_pos = p;
		//}
		//double len =  asio->_track1->len().time;
		int new_px = p*uit->wave.width();
		//if (asio)repaint();
		if ((new_px != uit->wave.px && new_px >=0 && new_px < uit->wave.width()) || invalidate)
		{
		//InvalidateRect(::GetDlgItem(g_dlg, IDC_STATIC4), NULL, TRUE);
		//	UpdateWindow(::GetDlgItem(g_dlg, IDC_STATIC4));
	//		InvalidateRect(g_dlg, NULL, TRUE);
		//	render(uit-tracks+1);
			_want_render = true;
		}
	}
}

void Win32UI::set_text_field(int id, const wchar_t *txt)
{
	SetDlgItemTextW(g_dlg, id, txt);
}

void Win32UI::set_clip(int t_id)
{
	SendMessage(GetDlgItem(g_dlg, t_id==1?IDC_CHECK7:IDC_CHECK8), BM_SETCHECK, BST_CHECKED, BST_CHECKED);
}

void Win32UI::render(int t_id)
{
	//PAINTSTRUCT ps;
	HDC hdc, hdc_old;
	HWND h;
	track_t *track = _io->GetTrack(t_id);
	UITrack *uit;
	if (t_id==1)
		uit = &_track1;
	else
		uit = &_track2;
	int width, height;
	RECT &r = uit->wave.r;

//	pthread_mutex_lock(&asio->_io_lock);

	if (!track->loaded())
		return;

	h = ::GetDlgItem(g_dlg, t_id == 1 ? IDC_STATIC4 : IDC_STATIC5);
//	printf("WM_PAINT %d, %d, %d, %d\n", hwndDlg, uMsg, wParam, lParam);
	hdc = GetDC(h);

	//GetClientRect(h, &r);

	//GetWindowRect(h, &r2);
	
	width = uit->wave.width();
	height = uit->wave.height();

	HDC memDC = CreateCompatibleDC(hdc);
	HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, width, height);
	HBITMAP hOldBmp = (HBITMAP)SelectObject(memDC, hMemBmp);

	hdc_old = hdc;
	hdc = memDC;

	SelectObject(hdc, br);

	// cant do this here it too slow
	if (width != track->get_display_width())
	{
		track->set_display_width(width);
		track->set_wav_heights();
	}
	Rectangle(hdc, r.top, r.left, r.right, r.bottom);
	//SetBkColor(hdc, RGB(255,255,255));

	for (int p=0; p<width; ++p)
	{
		if (p%(width/10) == 0) 
		{
		//	pthread_mutex_unlock(&asio->_io_lock);
		//	pthread_mutex_lock(&asio->_io_lock);
		}
		const track_t::display_t::wav_height &h =
			track->get_wav_height(p);
		int px_pk = (1.0-h.peak_top) * height / 2;
		int px_avg = (1.0-h.avg_top) * height / 2;
		int px_mn = (1.0+h.peak_bot) * height / 2;
		int line_height = px_avg-px_pk;
		
		if (h.peak_top != 0.0)
		{
			SelectObject(hdc, pen_lt);
			MoveToEx(hdc, r.left+p, r.top+px_pk, NULL);
			LineTo(hdc, r.left+p, r.top+px_avg);
			if (h.peak_bot != 0.0)
			{
				SelectObject(hdc, pen_dk);
				LineTo(hdc, r.left+p, r.top+height-px_avg);
				SelectObject(hdc, pen_lt);
				LineTo(hdc, r.left+p, r.top+height-px_mn);
			}
			else
			{
				SelectObject(hdc, pen_dk);
				LineTo(hdc, r.left+p, r.top+height/2);
			}
		}
		else
		{
			MoveToEx(hdc, r.left+p, r.top+height/2, NULL);
			SelectObject(hdc, pen_dk);
				LineTo(hdc, r.left+p, r.top+height-px_avg);
				SelectObject(hdc, pen_lt);
				LineTo(hdc, r.left+p, r.top+height-px_mn);
		}
		
	}

	double len = track->len().time;
	//px = int(playback_pos / len * double(width));
	if (uit->wave.playback_pos >= 0.0 && uit->wave.playback_pos <= 1.0)
	{
		uit->wave.px = uit->wave.playback_pos * width;
		MoveToEx(hdc, r.left+uit->wave.px, r.top, NULL);
		SelectObject(hdc, pen_yel);
		LineTo(hdc, r.left+uit->wave.px, r.top+height);
	}

	uit->wave.cpx = int(track->get_cuepoint_pos() * double(width));
	MoveToEx(hdc, r.left+uit->wave.cpx, r.top, NULL);
	SelectObject(hdc, pen_oran);
	LineTo(hdc, r.left+uit->wave.cpx, r.top+height);

	BitBlt(hdc_old, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

	SelectObject(memDC, hOldBmp);
	DeleteObject(hMemBmp);
	DeleteDC(memDC);
	
	ReleaseDC(h, hdc_old);

	uit->dirty = false;

//	pthread_mutex_unlock(&asio->_io_lock);
}

#endif // WINDOWS
