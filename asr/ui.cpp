#include "config.h"
#include "ui.h"

#include <cstdio>

typedef ASIOProcessor ASIOP;
extern ASIOP * asio;
typedef ASIOP::track_t track_t;
extern GenericUI *ui;

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

void GenericUI::do_paint()
{
	_io->GetTrack(1)->lockedpaint();
	_io->GetTrack(2)->lockedpaint();
}

void GenericUI::mouse_down(MouseButton b, int x, int y)
{
	switch (b)
	{
		case Left:
		{
			_track1.wave.mousedown = y >= _track1.wave.windowr.top && y < _track1.wave.windowr.bottom &&
				x >= _track1.wave.windowr.left && x < _track1.wave.windowr.right;
			if (_track1.wave.mousedown)
			{
				_lastx=x;
				_lasty=y;
				_io->GetTrack(1)->lock_pos(x-_track1.wave.windowr.left);
			}

			_track2.wave.mousedown = y >= _track2.wave.windowr.top && y < _track2.wave.windowr.bottom &&
				x >= _track2.wave.windowr.left && x < _track2.wave.windowr.right;
			if (_track2.wave.mousedown)
			{
				_lastx=x;
				_lasty=y;
				_io->GetTrack(2)->lock_pos(x-_track2.wave.windowr.left);
			}
			break;
		}
	case Right:
		break;
	case Middle:
		break;
	}
}

void GenericUI::mouse_up(MouseButton b, int x, int y)
{
	switch (b)
	{
		case Left:
		{
			_track1.wave.mousedown = false;
			_track2.wave.mousedown = false;
			_io->GetTrack(1)->unlock_pos();
			_io->GetTrack(2)->unlock_pos();
			break;
		}
		case Right:
		{
			if (y > _track1.wave.windowr.top && y < _track1.wave.windowr.bottom)
			{
				double f = double(x - _track1.wave.windowr.left)/_track1.wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("cue %f\n", f);
					_io->GetTrack(1)->set_cuepoint(_io->GetTrack(1)->get_display_time(f));
				}
			}
			else if (y > _track2.wave.windowr.top && y < _track2.wave.windowr.bottom)
			{
				double f = double(x - _track2.wave.windowr.left)/_track2.wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("cue %f\n", f);
					_io->GetTrack(2)->set_cuepoint(_io->GetTrack(2)->get_display_time(f));
				}
			}
			break;
		}
	case Middle:
		break;
	}
}

void GenericUI::mouse_dblclick(MouseButton b, int x, int y)
{
	switch (b)
	{
		case Left:
		{
			if (y >= _track1.wave.windowr.top && y < _track1.wave.windowr.bottom)
			{
				double f = double(x - _track1.wave.windowr.left)/_track1.wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("%f\n", f);
					_io->GetTrack(1)->seek_time(_io->GetTrack(1)->get_display_time(f));
				}
			}
			else if (y >= _track2.wave.windowr.top && y < _track2.wave.windowr.bottom)
			{
				double f = double(x - _track2.wave.windowr.left)/_track2.wave.width();
				if (f >= 0.0 && f <= 1.0)
				{
					printf("%f\n", f);
					_io->GetTrack(2)->seek_time(_io->GetTrack(2)->get_display_time(f));
				}
			}
			break;
		}
	case Right:
		break;
	case Middle:
		break;
	}
}

void GenericUI::mouse_move(int x, int y)
{
	int dx = x-_lastx;
	int dy = y-_lasty;

	if (_track1.wave.mousedown)
	{
		if (dy)
		{
			_io->GetTrack(1)->zoom_px(dy);
		}
		if (dx)
		{
			_io->GetTrack(1)->move_px(dx);
			_io->GetTrack(1)->lock_pos(x-_track1.wave.windowr.left);
		}
		_lastx = x;
		_lasty = y;
	}
	else if (_track2.wave.mousedown)
	{
		if (dy)
		{
			_io->GetTrack(2)->zoom_px(dy);
		}
		if (dx)
		{
			_io->GetTrack(2)->move_px(dx);
			_io->GetTrack(2)->lock_pos(x-_track2.wave.windowr.left);
		}
		_lastx = x;
		_lasty = y;
	}
}

void UITrack::set_coarse(double v)
{
	coarse_val = 48000.0 / (1.0 + .4 * v -0.2);
//	printf("coarse_val %f\n", _track2.coarse_val);
	double val = 48000.0 / (coarse_val+fine_val) - 1.0;
	printf("track %d: %f\n", id, val*100.0);
	pitch.set_text_pct(val*100.0);
	asio->GetTrack(id)->set_output_sampling_frequency(coarse_val+fine_val); 
}

void UITrack::set_fine(double v)
{
	fine_val = 800.0 -  1600.*v;
//	printf("fine_val %f\n", fine_val);
	double val = 48000.0 / (coarse_val+fine_val) - 1.0;
	printf("track %d: %f\n", id, val*100.0);
	pitch.set_text_pct(val*100.0);
	asio->GetTrack(id)->set_output_sampling_frequency(coarse_val+fine_val); 
}

void UIText::set_text(const wchar_t *txt)
{
	callback(id, txt);
}

void UIText::set_text_pct(double pct)
{
	wchar_t buf[64];
	swprintf(buf, L"%.02f%%", pct);
	set_text(buf);
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
	track_t *track = asio->GetTrack(t_id);
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

void Win32UI::set_position(void *t, double p, bool invalidate)
{
	if (asio)
	{
		UITrack *uit = (t == asio->GetTrack(1) ? &_track1 : &_track2);
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
						TCHAR buf[64];
						swprintf(buf, L"%.02f dB", 20.0*log(dwPos/800.));
						SetDlgItemText(g_dlg, IDC_EDIT5, buf);
						asio->_master_xfader->set_gain(1, dwPos/800.);
						asio->_cue->set_gain(1, dwPos/800.);
					}
					else if ((HWND)lParam == ::GetDlgItem(hwndDlg, IDC_SLIDER7))
					{
						DWORD dwPos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0); 
						TCHAR buf[64];
						swprintf(buf, L"%.02f dB", 20.0*log(dwPos/800.));
						SetDlgItemText(g_dlg, IDC_EDIT6, buf);
						asio->_master_xfader->set_gain(2, dwPos/800.);
						asio->_cue->set_gain(2, dwPos/800.);
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
				case IDC_BUTTON25:
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
							asio->GetTrack(LOWORD(wParam)==IDC_BUTTON3 ? 1 : 2)->set_source_file(filepath, &asio->_io_lock);
						}
						else
						{
							asio->set_file_output(filepath);
						}
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
				case IDC_BUTTON13:
				case IDC_BUTTON14:
				case IDC_BUTTON15:
				case IDC_BUTTON16:
				{
					int t_id = LOWORD(wParam)==IDC_BUTTON13||LOWORD(wParam)==IDC_BUTTON14 ? 1 : 2;
					double dt = LOWORD(wParam)==IDC_BUTTON13 || LOWORD(wParam)==IDC_BUTTON15 ? -0.1 : 0.1;
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
			//	return TRUE;
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

	_track1.id = 1;
	_track1.pitch.id = IDC_EDIT3;
	//_track1.pitch.callback = new functor2<GenericUI, int, const wchar_t *>(this, &GenericUI::set_text_field);
	_track2.id = 2;
	_track2.pitch.id = IDC_EDIT4;
	//_track2.pitch.callback = new functor2<GenericUI, int, const wchar_t *>(this, &GenericUI::set_text_field);
	//functor2<GenericUI, int, const wchar_t *> f(this, &GenericUI::set_text_field);
}

UIText::UIText(GenericUI *ui, int i) :
	callback(ui, &GenericUI::set_text_field)
{
}

GenericUI::GenericUI(ASIOProcessor *io) :
	_io(io),
	_track1(this, 1, IDC_EDIT3),
	_track2(this, 2, IDC_EDIT4)
{}

Win32UI::Win32UI(ASIOProcessor *io) :
	GenericUI(io),
	_want_render(false)
{}

UITrack::UITrack(GenericUI *ui, int tid, int pitch_id) :
	id(tid),
	coarse_val(48000.0),
	fine_val(0.0),
	pitch(ui, pitch_id)
{}

void Win32UI::main_loop()
{
	create();

#if USE_NEW_WAVE
	asio->GetTrack(1)->set_display_width(_track1.wave.width());
	asio->GetTrack(2)->set_display_width(_track2.wave.width());
#endif

	br = CreateSolidBrush(RGB(255,255,255));
	pen_dk = CreatePen(PS_SOLID, 1, RGB(0,0,255));
	pen_lt = CreatePen(PS_SOLID, 1, RGB(0,0,192));
	pen_yel = CreatePen(PS_SOLID, 1, RGB(255,255,0));
	pen_oran = CreatePen(PS_SOLID, 1, RGB(255,128,0));

	asio->GetTrack(1)->lockedpaint();
	asio->GetTrack(2)->lockedpaint();

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
