#include "ui.h"

#include <cstdio>

extern ASIOProcessor<SamplePairf, short> * asio;
typedef ASIOProcessor<SamplePairf, short>::track_t track_t;

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

UITrack tracks[2];

void repaint(int t_id=1)
{
	//PAINTSTRUCT ps;
	HDC hdc, hdc_old;
	HWND h;
	track_t *track = asio->GetTrack(t_id);
	UITrack *uit = tracks+t_id-1;
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
		const ASIOProcessor<SamplePairf, short>::track_t::display_t::wav_height &h =
			track->_display->get_wav_height(p);
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

//	pthread_mutex_unlock(&asio->_io_lock);
}

void set_position(void *t, double p, bool invalidate)
{
	if (asio)
	{
		UITrack *uit = (t == asio->GetTrack(1) ? tracks : tracks+1);
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
			repaint(uit-tracks+1);
		}
	}
}

int buttonx,buttony,lastx,lasty;

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
			asio->GetTrack(1)->lockedcall(repaint);
			asio->GetTrack(2)->lockedcall(repaint);
			return 0L;
		}
		case WM_LBUTTONDOWN:
		{
			buttonx = LOWORD(lParam);
			buttony = HIWORD(lParam)+45;
			
			tracks[0].wave.mousedown = buttony >= tracks[0].wave.windowr.top && buttony < tracks[0].wave.windowr.bottom &&
				buttonx >= tracks[0].wave.windowr.left && buttonx < tracks[0].wave.windowr.right;
			if (tracks[0].wave.mousedown)
			{
				lastx=buttonx;
				lasty=buttony;
				asio->GetTrack(1)->lock_pos(lastx-tracks[0].wave.windowr.left);
			}

			tracks[1].wave.mousedown = buttony >= tracks[1].wave.windowr.top && buttony < tracks[1].wave.windowr.bottom &&
				buttonx >= tracks[1].wave.windowr.left && buttonx < tracks[1].wave.windowr.right;
			if (tracks[1].wave.mousedown)
			{
				lastx=buttonx;
				lasty=buttony;
				asio->GetTrack(2)->lock_pos(lastx-tracks[1].wave.windowr.left);
			}
			break;
		}
		case WM_LBUTTONDBLCLK:
		{
			buttonx = LOWORD(lParam);
			buttony = HIWORD(lParam)+45;
			if (buttony >= tracks[0].wave.windowr.top && buttony < tracks[0].wave.windowr.bottom)
			{
				double f = double(buttonx - tracks[0].wave.windowr.left)/tracks[0].wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("%f\n", f);
					asio->GetTrack(1)->seek_time(asio->GetTrack(1)->get_display_time(f));
				}
			}
			else if (buttony >= tracks[1].wave.windowr.top && buttony < tracks[1].wave.windowr.bottom)
			{
				double f = double(buttonx - tracks[1].wave.windowr.left)/tracks[1].wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("%f\n", f);
					asio->GetTrack(2)->seek_time(asio->GetTrack(2)->get_display_time(f));
				}
			}
			break;
		}
		case WM_LBUTTONUP:
		{
			tracks[0].wave.mousedown = false;
			tracks[1].wave.mousedown = false;
			asio->GetTrack(1)->unlock_pos();
			asio->GetTrack(2)->unlock_pos();
			break;
		}
		case WM_MOUSEMOVE:
		{
			buttonx = LOWORD(lParam);
			buttony = HIWORD(lParam)+45;
		//	printf("WM_MOUSEMOVE %d %d %x %x\n", hwndDlg,uMsg,wParam,lParam);
			int dx = buttonx-lastx;
			int dy = buttony-lasty;

			if (tracks[0].wave.mousedown)
			{
				if (dy)
				{
					asio->GetTrack(1)->zoom_px(dy);
				}
				if (dx)
				{
					asio->GetTrack(1)->move_px(dx);
					asio->GetTrack(1)->lock_pos(buttonx-tracks[0].wave.windowr.left);
				}
				lastx =buttonx;
				lasty =buttony;
			}
			else if (tracks[1].wave.mousedown)
			{
				if (dy)
				{
					asio->GetTrack(2)->zoom_px(dy);
				}
				if (dx)
				{
					asio->GetTrack(2)->move_px(dx);
					asio->GetTrack(2)->lock_pos(buttonx-tracks[1].wave.windowr.left);
				}
				lastx =buttonx;
				lasty =buttony;
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
			buttonx = LOWORD(lParam);
			buttony = HIWORD(lParam)+45;
			{
				if (buttony > tracks[0].wave.windowr.top && buttony < tracks[0].wave.windowr.bottom)
				{
					double f = double(buttonx - tracks[0].wave.windowr.left)/tracks[0].wave.width();
					if (f >= 0.0 && f <= 1.0)
					{
						printf("cue %f\n", f);
						asio->GetTrack(1)->set_cuepoint(asio->GetTrack(1)->get_display_time(f));
					}
				}
				else if (buttony > tracks[1].wave.windowr.top && buttony < tracks[1].wave.windowr.bottom)
				{
					double f = double(buttonx - tracks[1].wave.windowr.left)/tracks[1].wave.width();
					if (f >= 0.0 && f <= 1.0)
					{
						printf("cue %f\n", f);
						asio->GetTrack(2)->set_cuepoint(asio->GetTrack(2)->get_display_time(f));
					}
				}
			//	printf("%d %d %d\n", HIWORD(lParam), LOWORD(lParam), y);
			}
			break;
		}
		case WM_HSCROLL:
			switch (LOWORD(wParam))
			{
				wchar_t buf[64];
				case SB_THUMBPOSITION:
				case SB_THUMBTRACK:
					if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER2))
					{
					//	printf("coarse %d ", HIWORD(wParam));
						tracks[0].coarse_val = 48000.0 / (1.0 + .01 * HIWORD(wParam) -0.5);
					//	printf("coarse_val %f\n", tracks[0].coarse_val);
						double val = 48000.0 / (tracks[0].coarse_val+tracks[0].fine_val) - 1.0;
						printf("track 1: %f\n", val*100.0);
						swprintf(buf, L"%.02f%%", val*100.0);
						SetDlgItemTextW(hwndDlg, IDC_EDIT3, buf);
						asio->GetTrack(1)->set_output_sampling_frequency(tracks[0].coarse_val+tracks[0].fine_val); 
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER3))
					{
					//	printf("fine %d ", HIWORD(wParam));
						//val = 10*60*HIWORD(wParam)*0.01;
						tracks[0].fine_val = 1000.0 -  20*HIWORD(wParam);
					//	printf("fine_val %f\n", tracks[0].fine_val);
						double val = 48000.0 / (tracks[0].coarse_val+tracks[0].fine_val) - 1.0;
						printf("track 1: %f\n", val*100.0);
						swprintf(buf, L"%.02f%%", val*100.0);
						SetDlgItemTextW(hwndDlg, IDC_EDIT3, buf);
						asio->GetTrack(1)->set_output_sampling_frequency(tracks[0].coarse_val+tracks[0].fine_val); 
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER4))
					{
					//	printf("coarse %d ", HIWORD(wParam));
						tracks[1].coarse_val = 48000.0 / (1.0 + .01 * HIWORD(wParam) -0.5);
					//	printf("coarse_val %f\n", tracks[1].coarse_val);
						double val = 48000.0 / (tracks[1].coarse_val+tracks[1].fine_val) - 1.0;
						printf("track 2: %f\n", val*100.0);
						swprintf(buf, L"%.02f%%", val*100.0);
						SetDlgItemTextW(hwndDlg, IDC_EDIT4, buf);
						asio->GetTrack(2)->set_output_sampling_frequency(tracks[1].coarse_val+tracks[1].fine_val); 
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER5))
					{
					//	printf("fine %d ", HIWORD(wParam));
						//val = 10*60*HIWORD(wParam)*0.01;
						tracks[1].fine_val = 1000.0 -  20*HIWORD(wParam);
					//	printf("fine_val %f\n", tracks[1].fine_val);
						double val = 48000.0 / (tracks[1].coarse_val+tracks[1].fine_val) - 1.0;
						printf("track 2: %f\n", val*100.0);
						swprintf(buf, L"%.02f%%", val*100.0);
						SetDlgItemTextW(hwndDlg, IDC_EDIT4, buf);
						asio->GetTrack(2)->set_output_sampling_frequency(tracks[1].coarse_val+tracks[1].fine_val); 
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER1))
					{
						asio->SetMix(HIWORD(wParam));
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER8))
					{
						asio->_cue->set_mix(HIWORD(wParam));
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER6))
					{
						asio->_master_xfader->set_gain(1, HIWORD(wParam)/100.);
						asio->_cue->set_gain(1, HIWORD(wParam)/100.);
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER7))
					{
						asio->_master_xfader->set_gain(2, HIWORD(wParam)/100.);
						asio->_cue->set_gain(2, HIWORD(wParam)/100.);
					}
					break;
			}
			return TRUE;
		case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
				case IDC_CHECK1:
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
					return TRUE;
					
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
				case IDC_BUTTON3: // load src
				case IDC_BUTTON6:
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
						SetDlgItemText(hwndDlg, LOWORD(wParam)==IDC_BUTTON3 ? IDC_EDIT1 : IDC_EDIT2, filepath);
						asio->GetTrack(LOWORD(wParam)==IDC_BUTTON3 ? 1 : 2)->set_source_file(filepath, &asio->_io_lock);
					//	asio->SetSrc(1, filepath);
					}
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
				case IDC_COMBO1:
				case IDC_COMBO2:
				{
					switch(HIWORD(wParam))
					{
						case CBN_CLOSEUP:
						{
						//	T_sink_sourceable<chunk_t> *output;
						//	if (LOWORD(wParam)==IDC_COMBO1)
						//		output = asio->_main_out;
						//	else
						//		output = asio->_file_out;
							LRESULT foo = SendMessage(GetDlgItem(g_dlg, LOWORD(wParam)), CB_GETCURSEL, 0, 0);
							switch (foo)
							{
							case 0:
								//output->set_source(asio->_master_bus);
								if (LOWORD(wParam)==IDC_COMBO1)
									asio->_main_src = asio->_master_xfader;
								else
									asio->_file_src = 0;
								break;
							case 1:
								if (LOWORD(wParam)==IDC_COMBO1)
									asio->_main_src = asio->_cue;
								else
									asio->_file_src = asio->_master_xfader;
								break;
							case 2:
								if (LOWORD(wParam)==IDC_COMBO1)
									asio->_main_src = asio->_aux;
								else
									asio->_file_src = asio->_cue;
								break;
							case 3:
								asio->_file_src = asio->_aux;
								break;
							}
							break;
						}
					}
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

void CreateUI()
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

	HWND button = CreateWindowEx(0, L"Button", L"Text", BS_PUSHBUTTON|BS_TEXT|WS_TABSTOP|WS_CHILD|WS_VISIBLE, 100, 100, 100, 100, g_newwnd, 0, ::GetModuleHandle(NULL),
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
	SetDlgItemText(g_dlg, IDC_EDIT1, asio->_default_src);
	ShowWindow(g_dlg, SW_SHOW);

	HWND h;
	for (int t=0; t < 2; ++t)
	{
		h = ::GetDlgItem(g_dlg, t == 0 ? IDC_STATIC4 : IDC_STATIC5);
		GetClientRect(h, &tracks[t].wave.r);
		GetWindowRect(h, &tracks[t].wave.windowr);
	}

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
}

void MainLoop_Win32()
{
	CreateUI();

#if USE_NEW_WAVE
	asio->GetTrack(1)->set_display_width(tracks[0].wave.width());
	asio->GetTrack(2)->set_display_width(tracks[1].wave.width());
#endif

	br = CreateSolidBrush(RGB(255,255,255));
	pen_dk = CreatePen(PS_SOLID, 1, RGB(0,0,255));
	pen_lt = CreatePen(PS_SOLID, 1, RGB(0,0,192));
	pen_yel = CreatePen(PS_SOLID, 1, RGB(255,255,0));
	pen_oran = CreatePen(PS_SOLID, 1, RGB(255,128,0));

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
	DeleteObject(pen_oran);
	DeleteObject(br);

#endif // !SLEEP
}
#endif // WINDOWS
