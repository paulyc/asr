#include "ui.h"
#include "io.h"

#include <cstdio>

extern ASIOThinger<SamplePairf, short> * asio;

#if WINDOWS
#include <commctrl.h>
#include "resource.h"
#define SLEEP 0

HWND g_dlg = NULL;

INT_PTR CALLBACK MyDialogProc(HWND hwndDlg,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
	switch (uMsg)
	{
		case WM_HSCROLL:
			switch (LOWORD(wParam))
			{
				case SB_THUMBPOSITION:
				case SB_THUMBTRACK:
					WORD foo = LOWORD(lParam);
					foo = LOWORD(wParam);
					foo = IDC_SLIDER3;
					double val;
				//	switch (LOWORD(wParam))
				//	{
				//	case 5:
						printf("%d ", HIWORD(wParam));
#if 0
						val = 48000.0 / (1.0 + .001 * HIWORD(wParam) -0.05);
						printf("%f\n", val);
						asio->SetResamplerate(val); 
#else
						val = 10*60*HIWORD(wParam)*0.01;
						printf("%f\n", val);
						asio->SetPos(val);
#endif
						break;
					//case 6:
				//		break;
				//	}
					
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

void CreateUI()
{
	INITCOMMONCONTROLSEX iccx;
	iccx.dwSize = sizeof(iccx);
	iccx.dwICC = ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&iccx);
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
}

void MainLoop_Win32()
{
	CreateUI();

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

#endif // !SLEEP
}
#endif // WINDOWS
