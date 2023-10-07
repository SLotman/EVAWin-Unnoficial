#include <windows.h>
#include <commctrl.h>
#include "eva.h"
#include "rc.h"

#define	VERSION		5

#define	MAXTOOLBARBUTTON	8

static	char		szAppName[] = "EVAWIN";
static	char		szIniFile[_MAX_PATH];
static	char		szCaption[] = "EVA player for Windows";
static	char		szWindowClassName[] = "EvaWin";
static	char		szFrameWindowClassName[] = "EvaWin_Frame";
static	char		szPanelWindowClassName[] = "EvaWin_Panel";
static	char		szSpakerPopupWindowClassName[] = "EvaWin_Volume";
static	HWND		hwndAdjustDlg;					// 画質調整ダイアログのハンドル
static	HWND		hwndEva;						// EVA 再生エリアのウィンドウハンドル
static	HWND		hwndFrame;						// 表示エリアのウィンドウハンドル
static	HWND		hwndTrack;						// トラックバーのウィンドウハンドル
static	HWND		hwndPanel;						// 操作パネルのウィンドウハンドル
static	HWND		hwndStatus;						// ステータスバーのウィンドウハンドル
static	TBBUTTON	tbButtons[MAXTOOLBARBUTTON];	// ツールバーのボタン
static	BOOL		bRepeat;						// 繰り返し再生
static	BOOL		bMono;							// 白黒モード
static	BOOL		bFilter;						// ディザ除去
static	BOOL		bRGB;							// SCREEN 8 モード
static	int			nAspect;						// アスペクト比 0 = MSX / 1=1:1 / 2=free
static	int			nStretch;						// 拡大
static	int			nContrast, nBright;				// 画質調整
static	BOOL		bScanlines;
static	BOOL		bScreen8;
static	int			nEvaWidth;						// 表示サイズ横
static	int			nEvaHeight;						// 表示サイズ縦
static	int			nMsxRateH;						// MSX のアスペクト比
static	int			nMsxRateV;						// MSX のアスペクト比
static	BOOL		bUpdateEnabled = FALSE;
static	BOOL		bPlayed;
static	int			nVol = 256;

void RegisterEVAFiles()
{
	long ERStatus;
	//long HKEY_CLASSES_ROOT = 0x80000000;
	HKEY Resultado;
	char filename[2048];
	char *param;
	
       ERStatus = RegCreateKey(HKEY_CLASSES_ROOT, ".eva", &Resultado);
    
       if (Resultado == 0) return; 
       //'seta valor padrao para "DPlayer.file"
	   param="EVAWin.file";

       ERStatus = RegSetValueEx(Resultado, "", NULL, 1, (BYTE *)param, sizeof(param));
       
	   //'seta tipo de arquivo
	   param = "video";
       ERStatus = RegSetValueEx(Resultado, "Content Type", NULL, 1, (BYTE *)param, sizeof(param));
       
    //'**********************************
    //'* Cria associa鈬o "MSXPad.file" *
    //'**********************************
    //'cria chave \MSXPad.file
    ERStatus = RegCreateKey(HKEY_CLASSES_ROOT, "EVAWin.file", &Resultado);

    //seta valor "Arquivo Digital Player(mp3 ou playlist)"
    param = "EVA video";
    ERStatus = RegSetValueEx(Resultado, "", NULL, 1, (BYTE *)param, sizeof(param));

    //'cria chave \DPlayer.file\DefaultIcon
    //ERStatus = RegCreateKey(HKEY_CLASSES_ROOT, "EVAWin.file\DefaultIcon", Resultado);
    //'seta valor para App.path + "\DPlayer.exe,1"
    //Valor = App.Path + "\msxpad.ico"
    //ERStatus = RegSetValueEx(Resultado, "", 0&, 1, ByVal Valor, Len(Valor) + 1)

    //'cria chave \DPlayer.file\shell
    ERStatus = RegCreateKey(HKEY_CLASSES_ROOT, "EVAWin.file\\shell", &Resultado);
    //'seta valor para "Tocar"
    //Valor = "Open"
	param="Open";
    ERStatus = RegSetValueEx(Resultado, "", NULL, 1, (BYTE *)param, sizeof(param));
    //'cria chave \DPlayer.file\shell\Tocar\command

    ERStatus = RegCreateKey(HKEY_CLASSES_ROOT, "EVAWin.file\\shell\\Open\\command", &Resultado);
    //'seta valor para App.path + "\DPlayer.exe %1"
    //Valor = App.Path + "\MSXPad.exe %1"
	DWORD resp = GetModuleFileName(NULL, filename, 2048);
	strcat(filename, " %1");

    ERStatus = RegSetValueEx(Resultado, "", NULL, 1, (BYTE * )filename, sizeof(filename));

}

void InitToolbarTable()
{
	tbButtons[0].iBitmap = 0;
	tbButtons[0].idCommand = IDM_PLAY;
	tbButtons[0].fsState = TBSTATE_ENABLED;
	tbButtons[0].fsStyle = TBSTYLE_BUTTON;
	tbButtons[0].dwData = 0;
	tbButtons[0].iString = 0;

	tbButtons[1].iBitmap = 1;
	tbButtons[1].idCommand = IDM_PAUSE;
	tbButtons[1].fsState = TBSTATE_ENABLED;
	tbButtons[1].fsStyle = TBSTYLE_BUTTON;
	tbButtons[1].dwData = 0;
	tbButtons[1].iString = 0;

	tbButtons[2].iBitmap = 2;
	tbButtons[2].idCommand = IDM_STOP;
	tbButtons[2].fsState = TBSTATE_ENABLED;
	tbButtons[2].fsStyle = TBSTYLE_BUTTON;
	tbButtons[2].dwData = 0;
	tbButtons[2].iString = 0;

	tbButtons[3].iBitmap = 0;
	tbButtons[3].idCommand = 0;
	tbButtons[3].fsState = TBSTATE_ENABLED;
	tbButtons[3].fsStyle = TBSTYLE_SEP;
	tbButtons[3].dwData = 0;
	tbButtons[3].iString = 0;

	tbButtons[4].iBitmap = 3;
	tbButtons[4].idCommand = IDM_REW;
	tbButtons[4].fsState = TBSTATE_ENABLED;
	tbButtons[4].fsStyle = TBSTYLE_BUTTON;
	tbButtons[4].dwData = 0;
	tbButtons[4].iString = 0;

	tbButtons[5].iBitmap = 4;
	tbButtons[5].idCommand = IDM_FRD;
	tbButtons[5].fsState = TBSTATE_ENABLED;
	tbButtons[5].fsStyle = TBSTYLE_BUTTON;
	tbButtons[5].dwData = 0;
	tbButtons[5].iString = 0;

	tbButtons[6].iBitmap = 0;
	tbButtons[6].idCommand = 0;
	tbButtons[6].fsState = TBSTATE_ENABLED;
	tbButtons[6].fsStyle = TBSTYLE_SEP;
	tbButtons[6].dwData = 0;
	tbButtons[6].iString = 0;

	tbButtons[7].iBitmap = 5;
	tbButtons[7].idCommand = IDM_SPAKER;
	tbButtons[7].fsState = TBSTATE_ENABLED;
	tbButtons[7].fsStyle = TBSTYLE_BUTTON;
	tbButtons[7].dwData = 0;
	tbButtons[7].iString = 0;
}


/********************************************************************
  バージョン情報ダイアログボックスの Window プロシージャ
 ********************************************************************/
BOOL APIENTRY VersionDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)  
{
	switch(uMsg){
		case WM_INITDIALOG:
			{
				char	szBuf[256];
				wsprintf(szBuf, "%d.%d", VERSION / 10, VERSION % 10);
				SetWindowText(GetDlgItem(hwndDlg, 200), szBuf);
			}
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wParam)){
				case IDOK:
				case IDCANCEL:
				case IDCLOSE:
					EndDialog(hwndDlg, 0);
					return TRUE;
			}
			break;
	}
	return FALSE;
} 


/********************************************************************
  画質調整ダイアログボックスの Window プロシージャ
 ********************************************************************/
BOOL APIENTRY AdjustDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)  
{
	switch(uMsg){
		case WM_INITDIALOG:
			{
				char szBuf[256];
				SendMessage(GetDlgItem(hwndDlg, IDC_UPDOWN1), UDM_SETBUDDY, (WPARAM)GetDlgItem(hwndDlg, IDC_EDIT1), 0);
				SendMessage(GetDlgItem(hwndDlg, IDC_UPDOWN1), UDM_SETRANGE, 0, MAKELONG(31, -31));
				SendMessage(GetDlgItem(hwndDlg, IDC_UPDOWN2), UDM_SETBUDDY, (WPARAM)GetDlgItem(hwndDlg, IDC_EDIT2), 0);
				SendMessage(GetDlgItem(hwndDlg, IDC_UPDOWN2), UDM_SETRANGE, 0, MAKELONG(31, -31));
				wsprintf(szBuf, "%d", nBright);
				SetWindowText(GetDlgItem(hwndDlg, IDC_EDIT1), szBuf);
				wsprintf(szBuf, "%d", nContrast);
				SetWindowText(GetDlgItem(hwndDlg, IDC_EDIT2), szBuf);
			}
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wParam)){
				case IDCANCEL:
					DestroyWindow(hwndDlg);
					return TRUE;
				case IDC_EDIT1:
				case IDC_EDIT2:
					if(HIWORD(wParam) == EN_CHANGE){
						char	szBuf[256];
						char	*pName;

						GetWindowText(GetDlgItem(hwndDlg, LOWORD(wParam)), szBuf, sizeof(szBuf));
						int n = atoi(szBuf);
						if(n < -31) n = -31;
						if(n > 31) n = 31;

						switch(LOWORD(wParam)){
							case IDC_EDIT1:
								nBright = n;
								pName = "BRIGHT";
								break;
							case IDC_EDIT2:
								nContrast = n;
								pName = "CONTRAST";
								break;
						}
						wsprintf(szBuf, "%d", n);
						WritePrivateProfileString(szAppName, pName, szBuf, szIniFile);

						SendMessage(hwndEva, EVAM_ADJUST, (WPARAM)nContrast, (LPARAM)nBright);
						return TRUE;
					}
					break;
			}
			break;
		case WM_CLOSE:
			DestroyWindow(hwndDlg);
			return TRUE;
		case WM_DESTROY:
			hwndAdjustDlg = NULL;
			return TRUE;
	}
	return FALSE;
} 


/********************************************************************
  VolumeWindow の Window プロシージャ
 ********************************************************************/
LRESULT CALLBACK VolumeWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static	HWND	hwndOwner;
	static	HWND	hwndTrack = NULL;
	static	int		nTimeout;

	switch(uMsg){
		case WM_CREATE:
			{
				RECT	rc;
				GetClientRect(hwnd, &rc);
				hwndTrack = CreateWindow(
					TRACKBAR_CLASS,
					NULL,
					WS_CHILD|WS_VISIBLE|TBS_NOTICKS|TBS_VERT|TBS_BOTH,
					0,0,
					rc.right,rc.bottom,
					hwnd,
					(HMENU)100,
					(HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE),
					NULL);
				SendMessage(hwndTrack, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(0,256));
				SendMessage(hwndTrack, TBM_SETPAGESIZE, 0, 32);
			}
			SetFocus(hwndTrack);
			nTimeout = 0;
			SetTimer(hwnd, 1, 100, NULL);
			return 0;
		case WM_USER:
			hwndOwner = (HWND)wParam;
			SendMessage(hwndTrack, TBM_SETPOS, (WPARAM)TRUE, 256-lParam);
			return 0;
		case WM_TIMER:
			if(++nTimeout >= 40){	// 4 秒間操作しなかった場合は閉じる
				SendMessage(hwnd, WM_CLOSE, 0, 0);
			}
			return 0;
		case WM_CLOSE:
			SendMessage(hwndOwner, WM_USER+100, 0, 0);
			return 0;
		case WM_DESTROY:
			if(hwndTrack != NULL){
				DestroyWindow(hwndTrack);
				hwndTrack = NULL;
			}
			KillTimer(hwnd, 1);
			return 0;
		case WM_ACTIVATE:
			if(!LOWORD(wParam)){
				SendMessage(hwnd, WM_CLOSE, 0, 0);
			}
			break;
		case WM_VSCROLL:
			if(hwndTrack == (HWND)lParam){
				nTimeout = 0;
				switch(LOWORD(wParam)){
					case TB_TOP:
					case TB_BOTTOM:
					case TB_LINEUP:
					case TB_LINEDOWN:
					case TB_PAGEUP:
					case TB_PAGEDOWN:
					case TB_THUMBPOSITION:
					case TB_THUMBTRACK:
						{
							int n = SendMessage(hwndTrack, TBM_GETPOS, 0, 0);
							SendMessage(hwndOwner,WM_USER+101,256-n,0);
						}
						return 0;
				}
			}
			break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


/********************************************************************
  PanelWindow の Window プロシージャ
 ********************************************************************/
LRESULT CALLBACK PanelWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static	BOOL	bPlay, bPause, bLoad;
	static	HWND	hwndToolbar = NULL;
	static	HWND	hwndSpakerPopup = NULL;

	switch(uMsg){
		case WM_CREATE:
			{
				RECT	rc;
				int		nWidth;
				GetWindowRect(hwnd, &rc);
				nWidth = rc.right - rc.left;
				SetWindowPos(hwnd, NULL, 0, 0, nWidth, 16, SWP_NOZORDER|SWP_NOMOVE);
				InitToolbarTable();

				hwndToolbar = CreateToolbarEx(hwnd,
					WS_VISIBLE | WS_CHILD | TBSTYLE_TOOLTIPS | 0x800,
					100,
					MAXTOOLBARBUTTON,
					(HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE),	
					IDTB_BMP,
					tbButtons,
					MAXTOOLBARBUTTON,
					0, 0, 16, 16,
					sizeof(TBBUTTON));

				GetWindowRect(hwndToolbar, &rc);
				SetWindowPos(hwnd, NULL, 0, 0, nWidth, rc.bottom-rc.top, SWP_NOZORDER|SWP_NOMOVE);
			}
			SendMessage(hwnd, WM_USER+99, 0, 0);
			return 0;

		case WM_SIZE:
			SetWindowPos(hwndToolbar, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER|SWP_NOMOVE);
			return 0;

		/* ウィンドウの破棄 */
		case WM_DESTROY:
			if(hwndToolbar != NULL) DestroyWindow(hwndToolbar);
			if(hwndSpakerPopup != NULL) DestroyWindow(hwndSpakerPopup);
			hwndToolbar = hwndSpakerPopup = NULL;
			return 0;

		// ボタンを再生可能状態にする
		case WM_USER:
			bLoad = lParam;
			bPause = bPlay = FALSE;
			SendMessage(hwnd, WM_USER+99, 0, 0);
			return 0;

		// ボタンを停止状態にする(lParam = 0:停止/1:ポーズ)
		case WM_USER+1:
			{
				bPlay = bPause = (BOOL)lParam;
				SendMessage(hwnd, WM_USER+99, 0, 0);
			}
			return 0;

		// ボタンを再生状態にする
		case WM_USER+2:
			{
				bPlay = TRUE;
				bPause = FALSE;
				SendMessage(hwnd, WM_USER+99, 0, 0);
			}
			return 0;

		case WM_USER+99:
			{
				HMENU	hMenu = GetMenu(GetParent(hwnd));
				EnableMenuItem(hMenu, IDM_PLAY,  MF_BYCOMMAND | (bLoad ? MF_ENABLED : MF_GRAYED));
				EnableMenuItem(hMenu, IDM_PAUSE, MF_BYCOMMAND | (bLoad & bPlay ? MF_ENABLED : MF_GRAYED));
				EnableMenuItem(hMenu, IDM_STOP,  MF_BYCOMMAND | (bLoad ? MF_ENABLED : MF_GRAYED));
				EnableMenuItem(hMenu, IDM_REW,   MF_BYCOMMAND | (bLoad ? MF_ENABLED : MF_GRAYED));
				EnableMenuItem(hMenu, IDM_FRD,   MF_BYCOMMAND | (bLoad ? MF_ENABLED : MF_GRAYED));
			}
			SendMessage(hwndToolbar, TB_ENABLEBUTTON, IDM_PLAY, bLoad);
			SendMessage(hwndToolbar, TB_ENABLEBUTTON, IDM_PAUSE, bLoad & bPlay);
			SendMessage(hwndToolbar, TB_ENABLEBUTTON, IDM_STOP, bLoad);
			SendMessage(hwndToolbar, TB_ENABLEBUTTON, IDM_REW, bLoad);
			SendMessage(hwndToolbar, TB_ENABLEBUTTON, IDM_FRD, bLoad);
			SendMessage(hwndToolbar, TB_CHECKBUTTON, IDM_PLAY, bPlay & !bPause & bLoad);
			SendMessage(hwndToolbar, TB_CHECKBUTTON, IDM_PAUSE, bPlay & bPause & bLoad);
			SendMessage(hwndToolbar, TB_CHECKBUTTON, IDM_STOP, !bPlay & !bPause & bLoad);
			SendMessage(hwndToolbar, TB_CHECKBUTTON, IDM_REW, 0);
			SendMessage(hwndToolbar, TB_CHECKBUTTON, IDM_FRD, 0);
			return 0;

		case WM_TIMER:
			switch(wParam){
				case IDM_REW:
				case IDM_FRD:
					SendMessage(hwnd, WM_COMMAND, MAKELONG(wParam & 0xFFFF, 0), 0);
					return 0;
			}
			break;

		case WM_NOTIFY:
			switch(wParam){
				case 100:
					{
						LPTBNOTIFY	ptbn = (LPTBNOTIFY)lParam;
						if(ptbn->hdr.code == TBN_BEGINDRAG){
							if(ptbn->iItem == IDM_REW || ptbn->iItem == IDM_FRD){
								if(bPlayed) SendMessage(hwndEva, EVAM_STOP, 0, 0);
								SetTimer(hwnd, ptbn->iItem, 100, NULL);
							}
						} else if(ptbn->hdr.code == TBN_ENDDRAG){
							if(ptbn->iItem == IDM_REW || ptbn->iItem == IDM_FRD){
								KillTimer(hwnd, ptbn->iItem);
								if(bPlayed) SendMessage(hwndEva, EVAM_PLAY, 0, 0);
							}
						}
					}
					break;
			}
			break;

		case WM_COMMAND:
			switch(LOWORD(wParam)){
				case IDM_PLAY:
					if(SendMessage(hwndEva, EVAM_GETNOWFRAME, 0, 0) == SendMessage(hwndEva, EVAM_GETLENGTH, 0, 0)){
						SendMessage(hwndEva, EVAM_SEEK, 0, 0);
					}
					if(SendMessage(hwndEva, EVAM_PLAY, 0, 0)) return 0;
					bPlayed = TRUE;
					bPlay = TRUE;
					bPause = FALSE;
					SendMessage(hwnd, WM_USER+99, 0, 0);
					return 0;
				case IDM_PAUSE:
					bPlayed = FALSE;
					bPlay = TRUE;
					bPause = TRUE;
					SendMessage(hwnd, WM_USER+99, 0, 0);
					SendMessage(hwndEva, EVAM_STOP, 0, 0);
					return 0;
				case IDM_STOP:
					SendMessage(hwndEva, EVAM_STOP, 0, 0);
					SendMessage(hwndEva, EVAM_SEEK, 0, 0);
					bPlayed = FALSE;
					bPlay = FALSE;
					bPause = FALSE;
					SendMessage(hwnd, WM_USER+99, 0, 0);
					return 0;
				case IDM_REW:
					{
						int n = SendMessage(hwndEva, EVAM_GETNOWFRAME, 0, 0);
						n -= 10;
						if(n < 0) n = 0;
						SendMessage(hwndEva, EVAM_SEEK, 0, n);
						SendMessage(hwnd, WM_USER+99, 0, 0);
					}
					return 0;
				case IDM_FRD:
					{
						int n = SendMessage(hwndEva, EVAM_GETNOWFRAME, 0, 0);
						n += 10;
						if(n > SendMessage(hwndEva, EVAM_GETLENGTH, 0, 0)) n = SendMessage(hwndEva, EVAM_GETLENGTH, 0, 0);
						SendMessage(hwndEva, EVAM_SEEK, 0, n);
						SendMessage(hwnd, WM_USER+99, 0, 0);
					}
					return 0;
				case IDM_SPAKER:
					if(hwndSpakerPopup == NULL){
						RECT	rc, rc2;
						int		n;
						GetWindowRect(hwndToolbar, &rc);
						n = SendMessage(hwndToolbar, TB_COMMANDTOINDEX, IDM_SPAKER, 0);
						SendMessage(hwndToolbar, TB_GETITEMRECT, n, (LPARAM)&rc2);
						rc2.right += rc.left;
						rc2.bottom += rc.top;
						rc2.right -= 24;
						rc2.bottom -= 48;
						if(rc2.bottom + 96 >= GetSystemMetrics(SM_CYSCREEN)) rc2.bottom = GetSystemMetrics(SM_CYSCREEN) - 96;
						if(rc2.right + 24 >= GetSystemMetrics(SM_CXSCREEN)) rc2.right = GetSystemMetrics(SM_CXSCREEN) - 24;
						if(rc2.right < 0) rc2.right = 0;
						hwndSpakerPopup = CreateWindowEx(
							WS_EX_PALETTEWINDOW,
							szSpakerPopupWindowClassName,
							NULL,
							WS_POPUP|WS_DLGFRAME|WS_VISIBLE,
							rc2.right, rc2.bottom,
							32,96,
							NULL,
							(HMENU)0,
							(HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE),	/* インスタンス	*/
							NULL);
						if(hwndSpakerPopup != NULL){
							SendMessage(hwndSpakerPopup, WM_USER, (WPARAM)hwnd, nVol);
						}
					}
					return 0;
			}
		case WM_USER+100:
			if(hwndSpakerPopup != NULL){
				DestroyWindow(hwndSpakerPopup);
				hwndSpakerPopup = NULL;
			}
			return 0;
		case WM_USER+101:
			nVol = wParam;
			SendMessage(hwndEva, EVAM_SETVOL, nVol, 0);
			{
				char	szBuf[256];
				wsprintf(szBuf, "%d", nVol);
				WritePrivateProfileString(szAppName, "VOL", szBuf, szIniFile);
			}
			return 0;
	}

	/* デフォルトの処理を行う */
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


/********************************************************************
  FrameWindow の Window プロシージャ
 ********************************************************************/
LRESULT CALLBACK FrameWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
		case WM_CREATE:
			hwndEva = CreateWindow(
				EVA_CLASS,				/* クラス			*/
				NULL,					/* テキスト			*/
				WS_CHILD|WS_VISIBLE,	/* スタイル			*/
				0,0,					/* 座標				*/
				128*1,106*1,			/* 大きさ			*/
				hwnd,					/* 親				*/
				(HMENU)100,				/* コントロールID	*/
				(HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE),	/* インスタンス	*/
				NULL);
			return 0;

		case WM_USER:
			{
				RECT	rc;
				GetClientRect(hwnd, &rc);
				SendMessage(hwnd, WM_SIZE, 0, MAKELONG(rc.right, rc.bottom));
			}
			return 0;

		case WM_SIZE:
			{
				int		x,y,w,h;

				w = LOWORD(lParam);
				h = HIWORD(lParam);
				if(nAspect == 0){
					if(w >= h * (128 * nMsxRateH) / (106 * nMsxRateV)){
						w = h * (128 * nMsxRateH) / (106 * nMsxRateV);
					} else {
						h = w * (106 * nMsxRateV) / (128 * nMsxRateH);
					}
				} else if(nAspect == 1){
					if(h < w * 106 / 128){
						w = h * 128 / 106;
					} else {
						h = w * 106 / 128;
					}
				}
				x = (LOWORD(lParam) - w) / 2;
				y = (HIWORD(lParam) - h) / 2;
				SetWindowPos(hwndEva, NULL, x, y, w, h, SWP_NOZORDER);
				nEvaWidth = w;
				nEvaHeight = h;
			}
			return 0;

		/* ウィンドウの破棄 */
		case WM_DESTROY:
			if(hwndEva) DestroyWindow(hwndEva);
			return 0;

		case WM_COMMAND:
			switch(LOWORD(wParam)){
				case 100:
					switch(HIWORD(wParam)){
						case EVAN_TERMINATE:
							if(bRepeat){
								SendMessage(hwndEva, EVAM_SEEK, 0, 0);
								SendMessage(hwndEva, EVAM_PLAY, 0, 0);
							} else {
								bPlayed = FALSE;
								SendMessage(hwndPanel, WM_USER+1, 0, 0);
							}
							return 0;
						case EVAN_START:
							SendMessage(hwndPanel, WM_USER+2, 0, 0);
							return 0;
						case EVAN_STOP:
							SendMessage(hwndPanel, WM_USER+1, 0, 1);
							return 0;
						case EVAN_POS:
							{
								char	szBuf[256];
								int		n;
								n = SendMessage(hwndEva, EVAM_GETNOWFRAME, 0, 0),
                                                                wsprintf(szBuf, "%d/%d Frames/Total", 
										n,
										SendMessage(hwndEva, EVAM_GETLENGTH, 0, 0));
								SendMessage(hwndStatus, SB_SETTEXT, 0, (LPARAM)szBuf);
								SendMessage(hwndTrack, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)n);
							}
							return 0;
						case EVAN_ERROR:
							{
                                                                SendMessage(hwndStatus, SB_SETTEXT, 0, (LPARAM)"Error");
							}
							return 0;
					}
					return 0;
			}
			break;
	}

	/* デフォルトの処理を行う */
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


/********************************************************************
  MainWindow の Window プロシージャ
 ********************************************************************/
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
		case WM_CREATE:
			hwndFrame = CreateWindow(
				szFrameWindowClassName,				/* クラス			*/
				NULL,					/* テキスト			*/
				WS_CHILD|WS_VISIBLE,	/* スタイル			*/
				0,106*1,					/* 座標				*/
				200,0,					/* 大きさ			*/
				hwnd,					/* 親				*/
				(HMENU)100,				/* コントロールID	*/
				(HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE),	/* インスタンス	*/
				NULL);

			hwndTrack = CreateWindow(
				TRACKBAR_CLASS,			/* クラス			*/
				NULL,					/* テキスト			*/
				WS_CHILD|WS_VISIBLE|TBS_NOTICKS,	/* スタイル			*/
				0,106*1,					/* 座標				*/
				200,0,					/* 大きさ			*/
				hwnd,					/* 親				*/
				(HMENU)102,				/* コントロールID	*/
				(HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE),	/* インスタンス	*/
				NULL);
			SendMessage(hwndTrack, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(0,0));
			SendMessage(hwndTrack, TBM_SETPAGESIZE, 0, 10);

			hwndPanel = CreateWindow(
				szPanelWindowClassName,				/* クラス			*/
				NULL,					/* テキスト			*/
				WS_CHILD|WS_VISIBLE,	/* スタイル			*/
				0,106*1,					/* 座標				*/
				200,0,					/* 大きさ			*/
				hwnd,					/* 親				*/
				(HMENU)101,				/* コントロールID	*/
				(HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE),	/* インスタンス	*/
				NULL);

			hwndStatus = CreateStatusWindow(WS_CHILD|WS_VISIBLE, NULL, hwnd, 102);

			EnableMenuItem(GetMenu(hwnd), IDM_CLOSE, MF_BYCOMMAND | MF_GRAYED);

			DragAcceptFiles(hwnd, TRUE);

			CheckMenuItem(GetMenu(hwnd), IDM_REPEAT, bRepeat ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(GetMenu(hwnd), IDM_MONO, bMono ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(GetMenu(hwnd), IDM_FILTER, bFilter ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(GetMenu(hwnd), IDM_SCANLINES, bScanlines ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(GetMenu(hwnd), IDM_SCREEN8, bScreen8 ? MF_CHECKED : MF_UNCHECKED);

			SendMessage(hwndEva, EVAM_SETMONO, 0, bMono);
			SendMessage(hwndEva, EVAM_ENABLEFILTER, bFilter, 0);
			SendMessage(hwndEva, EVAM_ENABLESCANLINES, bScanlines, 0);
			SendMessage(hwndEva, EVAM_FORCESCREEN8, bScreen8, 0);
			SendMessage(hwndEva, EVAM_ADJUST, (WPARAM)nContrast, (LPARAM)nBright);
			return 0;

		case WM_SIZE:
			{
				RECT	rc;
				RECT	rcClient;

				SetRect(&rcClient,0,0,LOWORD(lParam),HIWORD(lParam));

				GetWindowRect(hwndStatus, &rc);
				rcClient.bottom -= rc.bottom - rc.top;
				SetWindowPos(hwndStatus, NULL, 0, rcClient.bottom, rcClient.right, rc.bottom - rc.top, SWP_NOZORDER);

				GetWindowRect(hwndPanel, &rc);
				rcClient.bottom -= rc.bottom - rc.top;
				SetWindowPos(hwndPanel, NULL, 0, rcClient.bottom, rcClient.right, rc.bottom - rc.top, SWP_NOZORDER);

				rcClient.bottom -= rc.bottom - rc.top;
				SetWindowPos(hwndTrack, NULL, 0, rcClient.bottom, rcClient.right, rc.bottom - rc.top, SWP_NOZORDER);

				SetWindowPos(hwndFrame, NULL, 0, 0, rcClient.right, rcClient.bottom, SWP_NOZORDER);

				int w,h;
				if(nAspect == 0){
					w = rcClient.right  % (128 * nMsxRateH / nMsxRateV) ? 0 : rcClient.right  / (128 * nMsxRateH / nMsxRateV);
					h = rcClient.bottom % 106 ? 0 : rcClient.bottom / 106;
				} else {
					w = rcClient.right  % 128 ? 0 : rcClient.right  / 128;
					h = rcClient.bottom % 106 ? 0 : rcClient.bottom / 106;
				}
				if(w == h && w >= 1 && w <= 4){
					nStretch = w;
				} else {
					nStretch = 0;
				}
				CheckMenuItem(GetMenu(hwnd), IDM_STRETCH1, nStretch == 1 ? MF_CHECKED : MF_UNCHECKED);
				CheckMenuItem(GetMenu(hwnd), IDM_STRETCH2, nStretch == 2 ? MF_CHECKED : MF_UNCHECKED);
				CheckMenuItem(GetMenu(hwnd), IDM_STRETCH3, nStretch == 3 ? MF_CHECKED : MF_UNCHECKED);
				CheckMenuItem(GetMenu(hwnd), IDM_STRETCH4, nStretch == 4 ? MF_CHECKED : MF_UNCHECKED);
			}
			{
				int			lParts[2];
				int			nWidth = LOWORD(lParam);
				lParts[0] = nWidth * 6/10;
				lParts[1] = nWidth;
				SendMessage(hwndStatus, SB_SETPARTS, 2, (WPARAM)lParts);
			}
			if(bUpdateEnabled){
				char szBuf[256];
				if((wParam & SIZE_MAXIMIZED) != 0){
					WritePrivateProfileString(szAppName, "MAXIMIZED", "1", szIniFile);
				} else if((wParam & SIZE_MINIMIZED) == 0){
					wsprintf(szBuf, "%d", nEvaWidth);
					WritePrivateProfileString(szAppName, "WIDTH", szBuf, szIniFile);
					wsprintf(szBuf, "%d", nEvaHeight);
					WritePrivateProfileString(szAppName, "HEIGHT", szBuf, szIniFile);
					WritePrivateProfileString(szAppName, "MAXIMIZED", "0", szIniFile);
				}
			}
			return 0;

		/* 表示サイズ変更 */
		case WM_USER:
			{
				RECT	rc;
				RECT	rcClient;
				int		h,w;

				rcClient.right = LOWORD(lParam);
				rcClient.bottom = HIWORD(lParam);

				GetWindowRect(hwndPanel, &rc);
				rcClient.bottom += rc.bottom - rc.top;

				GetWindowRect(hwndTrack, &rc);
				rcClient.bottom += rc.bottom - rc.top;

				GetWindowRect(hwndStatus, &rc);
				rcClient.bottom += rc.bottom - rc.top;

				w = rcClient.right;
				h = rcClient.bottom;
				rcClient.top = rcClient.left = 0;
				AdjustWindowRectEx(&rcClient, GetWindowLong(hwnd, GWL_STYLE), TRUE, GetWindowLong(hwnd, GWL_EXSTYLE));

				SetWindowPos(hwnd, NULL, 0, 0, rcClient.right-rcClient.left, rcClient.bottom-rcClient.top, SWP_NOZORDER|SWP_NOMOVE);

				GetClientRect(hwnd, &rc);
				if(rc.bottom != h || rc.right != w){
					rcClient.bottom += h - rc.bottom;
					rcClient.right += w - rc.right;
					SetWindowPos(hwnd, NULL, 0, 0, rcClient.right-rcClient.left, rcClient.bottom-rcClient.top, SWP_NOZORDER|SWP_NOMOVE);
				}
			}
			return 0;

		/* アスペクト */
		case WM_USER+2:
			{
				nAspect = lParam;
				CheckMenuItem(GetMenu(hwnd), IDM_ASPECT0, nAspect == 0 ? MF_CHECKED : MF_UNCHECKED);
				CheckMenuItem(GetMenu(hwnd), IDM_ASPECT1, nAspect == 1 ? MF_CHECKED : MF_UNCHECKED);
				CheckMenuItem(GetMenu(hwnd), IDM_ASPECT2, nAspect == 2 ? MF_CHECKED : MF_UNCHECKED);

				if(nStretch <= 0 || nAspect == 2){
					SendMessage(hwndFrame, WM_USER, 0, 0);
				} else {
					SendMessage(hwnd, WM_USER+3, 0, nStretch);
				}

				char	szBuf[256];
				wsprintf(szBuf, "%d", nAspect);
				WritePrivateProfileString(szAppName, "ASPECT", szBuf, szIniFile);
			}
			return 0;

		/* 拡大 */
		case WM_USER+3:
			{
				int		h,w;

				if(IsZoomed(hwnd)){	// 最大化中なら元に戻す
					ShowWindow(hwnd, SW_RESTORE);
				}

				if(nAspect == 0){
					w = (128 * nMsxRateH / nMsxRateV) * lParam;
					h = 106 * lParam;
				} else {
					w = 128 * lParam;
					h = 106 * lParam;
				}
				SendMessage(hwnd, WM_USER, 0, MAKELONG(w, h));
			}
			return 0;

		/* ウィンドウの破棄 */
		case WM_DESTROY:
			DragAcceptFiles(hwnd, FALSE);
			if(hwndAdjustDlg) DestroyWindow(hwndAdjustDlg);
			if(hwndFrame) DestroyWindow(hwndFrame);
			if(hwndTrack) DestroyWindow(hwndTrack);
			if(hwndStatus) DestroyWindow(hwndStatus);
			if(hwndPanel) DestroyWindow(hwndPanel);
			PostQuitMessage(0);
			return 0;

		case WM_HSCROLL:
			if(hwndTrack == (HWND)lParam){
				switch(LOWORD(wParam)){
					case TB_TOP:
					case TB_BOTTOM:
					case TB_LINEUP:
					case TB_LINEDOWN:
					case TB_PAGEUP:
					case TB_PAGEDOWN:
						{
							int n = SendMessage(hwndTrack, TBM_GETPOS, 0, 0);
							SendMessage(hwndEva, EVAM_SEEK, 0, n);
						}
						return 0;
					case TB_THUMBPOSITION:
						{
							int n = SendMessage(hwndTrack, TBM_GETPOS, 0, 0);
							SendMessage(hwndEva, EVAM_SEEK, 0, n);
							if(bPlayed) SendMessage(hwndEva, EVAM_PLAY, 0, 0);
						}
						return 0;
					case TB_THUMBTRACK:
						{
							if(bPlayed) SendMessage(hwndEva, EVAM_STOP, 0, 0);
							int n = SendMessage(hwndTrack, TBM_GETPOS, 0, 0);
							SendMessage(hwndEva, EVAM_SEEK, 0, n);
						}
						return 0;
				}
			}
			break;

		case WM_COMMAND:
			switch(LOWORD(wParam)){
				case IDM_PLAY:
				case IDM_PAUSE:
				case IDM_STOP:
				case IDM_REW:
				case IDM_FRD:
					return SendMessage(hwndPanel, uMsg, wParam, lParam);
				case IDM_OPEN:
					{
						OPENFILENAME	ofn;
						char			szFile[_MAX_PATH];
						*szFile = 0;
						ZeroMemory(&ofn, sizeof(ofn));
						ofn.lStructSize = sizeof(OPENFILENAME);
						ofn.hwndOwner = hwnd;
                        ofn.lpstrFilter = "EVA file (*.eva)\0*.eva\0All files (*.*)\0*.*\0";
						ofn.nFilterIndex = 0;
						ofn.lpstrFile = szFile;
						ofn.nMaxFile = sizeof(szFile);
						ofn.lpstrFileTitle = NULL;
						ofn.nMaxFileTitle = 0;
						ofn.lpstrInitialDir = NULL;
						ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
						if(GetOpenFileName(&ofn)){
							SendMessage(hwnd, WM_USER+1, 0, (LPARAM)szFile);
						}
					}
					return 0;
				case IDM_CLOSE:
					SendMessage(hwndEva, EVAM_STOP, 0, 0);
					SendMessage(hwndEva, EVAM_CLOSE, 0, 0);
					SetWindowText(hwnd, szCaption);
					SendMessage(hwndStatus, SB_SETTEXT, 0, (LPARAM)"");
					SendMessage(hwndStatus, SB_SETTEXT, 1, (LPARAM)"");
					SendMessage(hwndPanel, WM_USER, 0, 0);
					EnableMenuItem(GetMenu(hwnd), IDM_CLOSE, MF_BYCOMMAND | MF_GRAYED);
					EnableWindow(hwndTrack, FALSE);
					SendMessage(hwndTrack, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(0,0));
					SendMessage(hwndTrack, TBM_SETPAGESIZE, 0, 10);
					return 0;
				case IDM_EXIT:
					SendMessage(hwnd, WM_CLOSE, 0, 0);
					return 0;
				case IDM_STRETCH1:
				case IDM_STRETCH2:
				case IDM_STRETCH3:
				case IDM_STRETCH4:
					SendMessage(hwnd, WM_USER+3, 0, LOWORD(wParam) - IDM_STRETCH1 + 1);
					return 0;
				case IDM_ASPECT0:
				case IDM_ASPECT1:
				case IDM_ASPECT2:
					SendMessage(hwnd, WM_USER+2, 0, LOWORD(wParam) - IDM_ASPECT0);
					return 0;
				case IDM_REPEAT:
					bRepeat = GetMenuState(GetMenu(hwnd), IDM_REPEAT, MF_BYCOMMAND) == MF_UNCHECKED;
					CheckMenuItem(GetMenu(hwnd), IDM_REPEAT, bRepeat ? MF_CHECKED : MF_UNCHECKED);
					{
						char	szBuf[256];
						wsprintf(szBuf, "%d", bRepeat);
						WritePrivateProfileString(szAppName, "REPEAT", szBuf, szIniFile);
					}
					return 0;
				case IDM_MONO:
					bMono = GetMenuState(GetMenu(hwnd), IDM_MONO, MF_BYCOMMAND) == MF_UNCHECKED;
					CheckMenuItem(GetMenu(hwnd), IDM_MONO, bMono ? MF_CHECKED : MF_UNCHECKED);
					SendMessage(hwndEva, EVAM_SETMONO, 0, bMono);
					{
						char	szBuf[256];
						wsprintf(szBuf, "%d", bMono);
						WritePrivateProfileString(szAppName, "MONO", szBuf, szIniFile);
					}
					return 0;
				case IDM_FILTER:
					bFilter = GetMenuState(GetMenu(hwnd), IDM_FILTER, MF_BYCOMMAND) == MF_UNCHECKED;
					CheckMenuItem(GetMenu(hwnd), IDM_FILTER, bFilter ? MF_CHECKED : MF_UNCHECKED);
					SendMessage(hwndEva, EVAM_ENABLEFILTER, bFilter, 0);
					{
						char	szBuf[256];
						wsprintf(szBuf, "%d", bFilter);
						WritePrivateProfileString(szAppName, "FILTER", szBuf, szIniFile);
					}
					return 0;

				case IDM_SCANLINES:
					bScanlines = GetMenuState(GetMenu(hwnd), IDM_SCANLINES, MF_BYCOMMAND) == MF_UNCHECKED;
					CheckMenuItem(GetMenu(hwnd), IDM_SCANLINES, bScanlines ? MF_CHECKED : MF_UNCHECKED);
					SendMessage(hwndEva, EVAM_ENABLESCANLINES, bScanlines, 0);
					{
						char	szBuf[256];
						wsprintf(szBuf, "%d", bScanlines);
						WritePrivateProfileString(szAppName, "SCANLINES", szBuf, szIniFile);
					}
					return 0;

				case IDM_SCREEN8:
					bScreen8 = GetMenuState(GetMenu(hwnd), IDM_SCREEN8, MF_BYCOMMAND) == MF_UNCHECKED;
					CheckMenuItem(GetMenu(hwnd), IDM_SCREEN8, bScreen8 ? MF_CHECKED : MF_UNCHECKED);
					SendMessage(hwndEva, EVAM_FORCESCREEN8, bScreen8, 0);
					{
						char	szBuf[256];
						wsprintf(szBuf, "%d", bScreen8);
						WritePrivateProfileString(szAppName, "SCREEN8", szBuf, szIniFile);
					}
					return 0;

				case IDM_RGB:
					bRGB = GetMenuState(GetMenu(hwnd), IDM_RGB, MF_BYCOMMAND) == MF_UNCHECKED;
					CheckMenuItem(GetMenu(hwnd), IDM_RGB, bRGB ? MF_CHECKED : MF_UNCHECKED);
					SendMessage(hwndEva, EVAM_SETRGB, 0, bRGB);
					return 0;
				case IDM_ADJUST:
					if(!hwndAdjustDlg){
						hwndAdjustDlg = CreateDialog((HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE), "ADJUSTDLG", hwnd, (DLGPROC)AdjustDlgProc);
					}
					ShowWindow(hwndAdjustDlg, SW_SHOW);
					return 0;
				case IDM_ABOUT:
					DialogBox((HINSTANCE)GetWindowLong(hwnd, GWL_HINSTANCE), "VERSIONDLG", hwnd, (DLGPROC)VersionDlgProc);
					return 0;
			}
			break;

		/* ファイル開く */
		case WM_USER+1:
			{
				LPSTR	pFile = (LPSTR)lParam;
				SendMessage(hwnd, WM_COMMAND, MAKELONG(IDM_CLOSE, 0), 0);

				if(!SendMessage(hwndEva, EVAM_OPEN, 0, (LPARAM)pFile)){
					char szBuf[1024];
					LPSTR	p = pFile;
					LPSTR	pLast = pFile;
					int		n;

					while(*p){
						if(*p == '\\') pLast = p + 1;
						p = CharNext(p);
					}

					SendMessage(hwndTrack, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(0,0));
					SendMessage(hwndTrack, TBM_SETPAGESIZE, 0, 10);
					EnableWindow(hwndTrack, TRUE);

					strcpy(szBuf, pLast);
					strcat(szBuf, " - ");
					strcat(szBuf, szCaption);
					SetWindowText(hwnd, szBuf);

					n = SendMessage(hwndEva, EVAM_GETRATE, 0, 0),
					wsprintf(szBuf, "%d.%d[FPS]", 
								n/1000, (n/100)%10);
					SendMessage(hwndStatus, SB_SETTEXT, 1, (LPARAM)szBuf);

					SendMessage(hwndTrack, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(0,SendMessage(hwndEva, EVAM_GETLENGTH, 0, 0)));

					SendMessage(hwndPanel, WM_USER, 0, 1);
					SendMessage(hwndPanel, WM_COMMAND, MAKELONG(IDM_PLAY, 0), 0);

					EnableMenuItem(GetMenu(hwnd), IDM_CLOSE, MF_BYCOMMAND | MF_ENABLED);
				}
			}
			return 0;

		/* ファイルドロップ */
		case WM_DROPFILES:
			{
				char	szFile[_MAX_PATH];
				int		cFiles;
				cFiles = DragQueryFile((HDROP) wParam, 0xFFFFFFFF, (LPSTR) NULL, 0);
				for(int i=0; i < cFiles; i++){
					DragQueryFile((HDROP) wParam, i, szFile, sizeof(szFile));
					if(!SendMessage(hwnd,WM_USER+1,0,(LPARAM)szFile)) break;
				}
				DragFinish((HDROP) wParam);
			}
			return 0;
	}

	/* デフォルトの処理を行う */
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


/********************************************************************
  WindowClass を登録する
 ********************************************************************/
BOOL RegisterWindowClass(HINSTANCE hInst)
{
	WNDCLASSEX	wcl;

	ZeroMemory(&wcl, sizeof(wcl));
	wcl.hInstance		= hInst;
	wcl.lpszClassName	= szWindowClassName;
	wcl.lpfnWndProc		= WindowProc;
	wcl.style			= CS_VREDRAW|CS_HREDRAW;
	wcl.cbSize			= sizeof(WNDCLASSEX);
	wcl.hIcon			= (HICON) LoadImage(hInst, "MAIN", IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	//wcl.hIcon			= LoadIcon(hInst, IDI_APPLICATION);
	wcl.hIconSm			= (HICON) LoadImage(hInst, "MAIN", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	//wcl.hIconSm			= LoadIcon(hInst, IDI_APPLICATION);
	wcl.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcl.lpszMenuName	= "MainMenu";
	wcl.cbClsExtra		= 0;
	wcl.cbWndExtra		= 0;
	wcl.hbrBackground	= (HBRUSH)(COLOR_3DFACE+1);
	if(!RegisterClassEx(&wcl)) return FALSE;

	ZeroMemory(&wcl, sizeof(wcl));
	wcl.hInstance		= hInst;
	wcl.lpszClassName	= szFrameWindowClassName;
	wcl.lpfnWndProc		= FrameWindowProc;
	wcl.style			= CS_VREDRAW|CS_HREDRAW;
	wcl.cbSize			= sizeof(WNDCLASSEX);
	wcl.hIcon			= NULL;//LoadIcon(NULL, IDI_APPLICATION);
	wcl.hIconSm			= NULL;//LoadIcon(NULL, IDI_WINLOGO);
	wcl.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcl.lpszMenuName	= NULL;
	wcl.cbClsExtra		= 0;
	wcl.cbWndExtra		= 0;
	wcl.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
	if(!RegisterClassEx(&wcl)) return FALSE;

	ZeroMemory(&wcl, sizeof(wcl));
	wcl.hInstance		= hInst;
	wcl.lpszClassName	= szSpakerPopupWindowClassName;
	wcl.lpfnWndProc		= VolumeWindowProc;
	wcl.style			= CS_VREDRAW|CS_HREDRAW;
	wcl.cbSize			= sizeof(WNDCLASSEX);
	wcl.hIcon			= NULL;
	wcl.hIconSm			= NULL;
	wcl.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcl.lpszMenuName	= NULL;
	wcl.cbClsExtra		= 0;
	wcl.cbWndExtra		= 0;
	wcl.hbrBackground	= (HBRUSH)(COLOR_3DFACE+1);
	if(!RegisterClassEx(&wcl)) return FALSE;

	ZeroMemory(&wcl, sizeof(wcl));
	wcl.hInstance		= hInst;
	wcl.lpszClassName	= szPanelWindowClassName;
	wcl.lpfnWndProc		= PanelWindowProc;
	wcl.style			= CS_VREDRAW|CS_HREDRAW;
	wcl.cbSize			= sizeof(WNDCLASSEX);
	wcl.hIcon			= NULL;
	wcl.hIconSm			= NULL;
	wcl.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcl.lpszMenuName	= NULL;
	wcl.cbClsExtra		= 0;
	wcl.cbWndExtra		= 0;
	wcl.hbrBackground	= (HBRUSH)(COLOR_3DFACE+1);
	return RegisterClassEx(&wcl);
}


/********************************************************************
  メッセージループ
 ********************************************************************/
WPARAM MessageLoop()
{
	MSG		msg;

	while(GetMessage(&msg, NULL, 0, 0)){
		if(!IsWindow(hwndAdjustDlg) || !IsDialogMessage(hwndAdjustDlg, &msg)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return msg.wParam;
}


/********************************************************************
   コマンドラインから再生するファイルを得る
 ********************************************************************/
BOOL GetEvaFile(LPSTR lpszArgs, char *szPath, int max)
{
	char	*p = lpszArgs;
	char	*pFileStart = NULL, *pFileEnd = NULL;
	int		len;
	int		nState = 0;

	*szPath = 0;
	if(p == NULL) return TRUE;

	while(*p){
		if(nState == 0){
			if(*p == '/' || *p == '-'){
				nState = 1;
			} else if(*p == '\"'){
				nState = 2;
				pFileStart = p + 1;
			} else if(*p != ' '){
				nState = 3;
				pFileStart = p;
			}
		} else if(nState == 1){		// オプション
			if(*p == ' ') nState = 0;
		} else if(nState == 2){		// ファイル名 "" 区切り
			if(*p == '\"'){
				nState = 0;
				pFileEnd = p;
			}
		} else if(nState == 3){		// ファイル名
			if(*p == ' '){
				nState = 0;
				pFileEnd = p;
			}
		}
		p = CharNext(p);
	}

	if(pFileStart){
		if(pFileEnd == NULL) pFileEnd = p;
		len = pFileEnd - pFileStart;
		if(len > max - 1) len = max - 1;
		CopyMemory(szPath, pFileStart, len);
		szPath[len] = 0;
	}

	return TRUE;
}


/********************************************************************
   Ini ファイルのパスを作成
 ********************************************************************/
void CreateIniFilePath()
{
	LPSTR	lp = szIniFile;
	LPSTR	lpLast = szIniFile;

	GetModuleFileName(NULL, szIniFile, sizeof(szIniFile));
	while(*lp){
		if(*lp == '\\') lpLast = lp;
		lp = CharNext(lp);
	}
	lstrcpy(lpLast, "\\EVAWIN.INI");
}


/********************************************************************
  スタートエントリ
 ********************************************************************/
int WINAPI WinMain(HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs, int nWinMode)
{
	HWND		hwnd;
	char		szPath[_MAX_PATH];
	BOOL		bMaximized;

	CreateIniFilePath();
	RegisterEVAFiles();

	// 各種コントロールの初期化
	InitCommonControls();
	RegisterEvaControlClass(hThisInst);
	if(!RegisterWindowClass(hThisInst)){
                MessageBox(NULL, "can not regist WINDOW CLASS", "error", MB_OK);
		return 1;
	}

	// 再生するファイルを得る
	GetEvaFile(lpszArgs, szPath, sizeof(szPath));

	// MSX のアスペクト比を得る
	nMsxRateH = GetPrivateProfileInt(szAppName, "RATEH", 10, szIniFile);
	nMsxRateV = GetPrivateProfileInt(szAppName, "RATEV", 8, szIniFile);

	// 保存された設定項目値を得る
	nContrast = GetPrivateProfileInt(szAppName, "CONTRAST", 0, szIniFile);
	nBright = GetPrivateProfileInt(szAppName, "BRIGHT", 0, szIniFile);
	bRepeat = (BOOL)GetPrivateProfileInt(szAppName, "REPEAT", 0, szIniFile);
	bMono = (BOOL)GetPrivateProfileInt(szAppName, "MONO", 0, szIniFile);
	bFilter = (BOOL)GetPrivateProfileInt(szAppName, "FILTER", 0, szIniFile);
	bScanlines = (BOOL)GetPrivateProfileInt(szAppName, "SCANLINES", 0, szIniFile);
	bScreen8 = (BOOL)GetPrivateProfileInt(szAppName, "SCREEN8", 0, szIniFile);
	bMaximized = (BOOL)GetPrivateProfileInt(szAppName, "MAXIMIZED", 0, szIniFile);
	nVol = GetPrivateProfileInt(szAppName, "VOL", 256, szIniFile);

	/* Window を作成 */
	hwnd = CreateWindow(
			szWindowClassName,
			szCaption,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			HWND_DESKTOP,
			NULL,
			hThisInst,
			NULL);
	ShowWindow(hwnd, nWinMode);
	UpdateWindow(hwnd);

	// 設定
	SendMessage(hwnd, WM_USER+2, 0, GetPrivateProfileInt(szAppName, "ASPECT", 1, szIniFile));
	{
		int		w, h;
		w = GetPrivateProfileInt(szAppName, "WIDTH", 128, szIniFile);
		h = GetPrivateProfileInt(szAppName, "HEIGHT", 106, szIniFile);
		SendMessage(hwnd, WM_USER+0, 0, MAKELONG(w,h));
	}
	if(bMaximized) ShowWindow(hwnd, SW_MAXIMIZE);
	SendMessage(hwndEva, EVAM_SETVOL, nVol, 0);
	SendMessage(hwndEva, EVAM_ENABLESCANLINES, bScanlines, 0);
	SendMessage(hwndEva, EVAM_FORCESCREEN8, bScreen8, 0);
	SendMessage(hwndEva, EVAM_SETWAVEBUFFERCOUNT, GetPrivateProfileInt(szAppName, "WAVEBUFF", 4, szIniFile), 0);

	bUpdateEnabled = TRUE;

	// ファイルを開く
	if(*szPath){
		SendMessage(hwnd,WM_USER+1,0,(LPARAM)szPath);
	}

	// メッセージループ
	return MessageLoop();
}

