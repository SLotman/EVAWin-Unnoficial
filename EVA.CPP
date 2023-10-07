#include <windows.h>
#include <mmsystem.h>
#include "yjk.h"
#include "eva.h"
#include <string.h>
#include <stdio.h>
#include "rc.h"

#define	MAXWAVEBUFFER			8
#define	MAXEVABUFFER			(MAXWAVEBUFFER+2)
#define	EVAWIDTH				128							// EVA データの映像の幅
#define	EVAHEIGHT				106							// EVA データの映像の高さ
#define	EVAPCMRATE				15750						// PCM 再生レート
#define	EVAFILEFRAMESIZE		0x3C00						// EVA file の 1frame のサイズ
#define	EVAFILEIMAGESIZE		(EVAWIDTH*EVAHEIGHT)		// 映像データのサイズ
#define	EVAFILEPCMSIZEOFFSET	0x3BFE						// PCM サイズが格納されている位置

char	szEvaCtrlName[] = "EVA CONTROL";

//struct msxpal { BYTE RB; BYTE G; };

//extern msxpal pal[16];

typedef struct {
	BOOL		bEnhance;						// データファイルの種類 FALSE=10FPS data / TRUE=12FPS data
	LPCTSTR		sFilename;
	BYTE		byBuff[MAXEVABUFFER][EVAFILEFRAMESIZE];	// データ保存バッファ
	int			nAudio;							// 音声バッファ番号
	int			nVideo;							// 映像バッファ番号

	HANDLE		hFile;							// ファイルハンドル
	BOOL		bPlay;							// 再生中フラグ
	BOOL		bSkip;

	int			nSendFrame;						// wave 送信フレーム
	int			nNowFrame;						// 再生終了フレーム
	int			nLastFrame;						// 総フレーム数

	int			nFPS;							// フレームレート

	HWND		hwndEVA;						// 自分のウィンドウハンドル
	RECT		rcClient;						// 自分のクライアント矩形

	HBITMAP		hbmSurface;						// 描画先のビットマップハンドル
	LPVOID		lpSurface;						// 描画先のアドレス

	int			nContrast;						// コントラスト
	int			nBright;						// ブライトネス
	BOOL		bScanLines;
	BOOL		bScreen8;
	BOOL		bisScreen8;
	BOOL		bisScreen5;
	BOOL		bMono;							// 白黒モード
	BOOL		bRGB;							// RGB モード
	int			nVol;							// 音量(0～256)
	BOOL		bEnableFilter;					// ディザ除去

	HWAVEOUT	hwo;							// WaveOut ハンドル
	int			nWaveOutPost;					// whdr 配列使用位置
	int			nFreeAudio;
	WAVEHDR		whdr[MAXWAVEBUFFER];			// WaveOut用ヘッダ
	DWORD		dwSample;
	int			nWaveBufferCount;

	BOOL		bBreak;
	HANDLE		hthreadAudio;
	HANDLE		heventAudio;
	HANDLE		hthreadVideo;
	HANDLE		heventVideo;
} EVACTRLWORK, FAR *LPEVACTRLWORK;



/********************************************************************
  Wave バッファの開放
 ********************************************************************/
void WaveBufferFree(HWAVEOUT hwo, LPWAVEHDR pwh)
{
	waveOutUnprepareHeader(hwo, pwh, sizeof(WAVEHDR));
	pwh->dwFlags = 0;
	GlobalFree((HGLOBAL)pwh->lpData);
}


/********************************************************************
  WAVE出力
 ********************************************************************/
BOOL OutputWave(LPEVACTRLWORK lpew, LPVOID lpData, int nSize, int nVol)
{
	LPWAVEHDR	pwh;
	HGLOBAL		hg;

	if(!lpew->hwo) return FALSE;
	if(nSize == 0) return FALSE;

	// バッファを確保する
	hg = GlobalAlloc(GMEM_FIXED, nSize);
	if(hg == NULL) return FALSE;

	// バッファへコピーする(ついでに音量も変える)
	if(nVol >= 256){
		CopyMemory((LPVOID)hg, lpData, nSize);
	} else {
		int		nn = nSize;
		LPBYTE	lps = (LPBYTE)lpData;
		LPBYTE	lpd = (LPBYTE)hg;
		while(nn--){
			*lpd++ = (BYTE)(((((int)*lps++ - 128) * nVol / 256) + 128) & 255);
		}
	}

	// ヘッダの設定
	pwh = &lpew->whdr[lpew->nWaveOutPost];
	ZeroMemory(pwh, sizeof(WAVEHDR));
	pwh->lpData = (LPSTR)hg;
	pwh->dwBufferLength = nSize;
	pwh->dwBytesRecorded = nSize;
	pwh->dwUser = lpew->nWaveOutPost;
	pwh->dwFlags = 0;
	pwh->dwLoops = 0;
	if(waveOutPrepareHeader(lpew->hwo, pwh, sizeof(WAVEHDR))){
		GlobalFree(hg);
		return FALSE;
	}

	// wave へ出力
	if(waveOutWrite(lpew->hwo, pwh, sizeof(WAVEHDR))){
		waveOutUnprepareHeader(lpew->hwo, pwh, sizeof(WAVEHDR));
		GlobalFree(hg);
		return FALSE;
	}

	lpew->nWaveOutPost = (lpew->nWaveOutPost + 1) % lpew->nWaveBufferCount;
	lpew->dwSample += nSize;
	return TRUE;
}


/********************************************************************
  WaveOut Stop
 ********************************************************************/
BOOL StopWave(LPEVACTRLWORK lpew)
{
	int		i;

	if(!lpew->hwo) return FALSE;

	// 再生を止める
	waveOutPause(lpew->hwo);
	waveOutReset(lpew->hwo);

	// バッファの開放
	for(i=0;i<MAXWAVEBUFFER;i++){
		if(lpew->whdr[i].dwFlags & WHDR_PREPARED){
			WaveBufferFree(lpew->hwo, &lpew->whdr[i]);
		}
	}
	return TRUE;
}


/********************************************************************
  DIB を作成
 ********************************************************************/
BOOL CreateDIB(HWND hwnd, LPEVACTRLWORK lpew)
{
	BITMAPINFOHEADER	bi;
	HDC					hdc;
	int					nBpp;

	// DC を得る
	hdc = GetDC(hwnd);
	if(hdc == NULL) return NULL;

	nBpp = 32;

	// DIB の作成
	ZeroMemory(&bi, sizeof(bi));
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = EVAWIDTH;
	bi.biHeight = EVAHEIGHT;
	bi.biPlanes = 1;
	bi.biBitCount = (WORD)(nBpp & 0xFFFF);
	bi.biCompression = BI_RGB;
	bi.biSizeImage = EVAFILEIMAGESIZE*2;
	bi.biXPelsPerMeter = 96;
	bi.biYPelsPerMeter = 96;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;
	lpew->hbmSurface = CreateDIBSection(hdc, (LPBITMAPINFO)&bi, DIB_RGB_COLORS, &lpew->lpSurface, NULL, 0);

	// DC を開放
	ReleaseDC(hwnd, hdc);

	return lpew->hbmSurface != NULL;
}


/********************************************************************
  DIB を削除
 ********************************************************************/
void DeleteDIB(LPEVACTRLWORK lpew)
{
	if(lpew->hbmSurface){
		DeleteObject(lpew->hbmSurface);
		lpew->hbmSurface = NULL;
	}
}


/********************************************************************
  Read Frame
 ********************************************************************/
BOOL ReadFrame(LPEVACTRLWORK lpew, LPBYTE lpbBuff, int *pnSize)
{
	DWORD	dwReaded;
	int		nSize;
	BYTE	byBuffer[EVAFILEFRAMESIZE];

	if(lpew->bEnhance){
		if(!ReadFile(lpew->hFile, byBuffer, EVAFILEFRAMESIZE, &dwReaded, NULL)) dwReaded = 0xFFFFFFFF;

		if(dwReaded == EVAFILEFRAMESIZE){
			
			if (lpew->bisScreen5)
			{
				// read frame palette
				int v = 0;
				for (long u=0x3BDD; u<0x3BFD; u+=2)
				{
					//pal[v].RB = byBuffer[u];
					//pal[v].G  = byBuffer[u+1];
					SetPal(v, byBuffer[u], byBuffer[u+1]);
					v++;
				}
			}
			
			nSize = *(LPWORD)(byBuffer+EVAFILEPCMSIZEOFFSET);
			LPBYTE	p = byBuffer;
			int nPcm = 0;
			int nVid = 0;
			
			for(int i = 0; i < 106; i++){
				for(int j = 0; j < 11; j++){
					lpbBuff[EVAFILEIMAGESIZE+nPcm] = *p++;
					nPcm++;
					CopyMemory(&lpbBuff[nVid], p, 11);
					p+=11;
					nVid+=11;
				}
				lpbBuff[EVAFILEIMAGESIZE+nPcm] = *p++;
				nPcm++;
				CopyMemory(&lpbBuff[nVid], p, 7);
				p+=7;
				nVid+=7;
			}
			
			if(nSize > 12 * 106){
				int	n0 = EVAFILEIMAGESIZE + nPcm;
				int n1 = nSize - nPcm;
				if(n1 > EVAFILEFRAMESIZE - n0) n1 = EVAFILEFRAMESIZE - n0;
				if(n1 > 0 && n0 < EVAFILEFRAMESIZE){
					CopyMemory(&lpbBuff[n0], p, n1);
				}
			}
			
		}
	} else {
		if(!ReadFile(lpew->hFile, lpbBuff, EVAFILEFRAMESIZE, &dwReaded, NULL)) dwReaded = 0xFFFFFFFF;
		nSize = *(LPWORD)(lpbBuff+EVAFILEPCMSIZEOFFSET);
		
		if (lpew->bisScreen5)
		{
			// read frame palette
			int v = 0;
			for (long u=0x3BDD; u<0x3BFD; u+=2)
			{
				//pal[v].RB = lpbBuff[u];
				//pal[v].G  = lpbBuff[u+1];
				SetPal(v, lpbBuff[u], lpbBuff[u+1]);
				v++;
			}
		}
		
	}

	
	if(nSize == 0){
		nSize = 1;
		lpbBuff[EVAFILEIMAGESIZE] = 0x80;
	}

	if(pnSize) *pnSize = nSize;

	return dwReaded == EVAFILEFRAMESIZE;
}


/********************************************************************
  Wave 送信
 ********************************************************************/
BOOL SendWave(LPEVACTRLWORK lpew)
{
	int		nSize;

	if(lpew->nSendFrame >= lpew->nLastFrame) return TRUE;
	if(ReadFrame(lpew, &lpew->byBuff[lpew->nAudio][0], &nSize)){
		if(nSize <= 1790){
			if(OutputWave(lpew, &lpew->byBuff[lpew->nAudio][EVAFILEIMAGESIZE], nSize, lpew->nVol)){
				lpew->bSkip = ((lpew->nAudio + 1) % MAXEVABUFFER) == lpew->nVideo;
				if(!lpew->bSkip){
					lpew->nAudio = (lpew->nAudio + 1) % MAXEVABUFFER;
				}
				lpew->nSendFrame++;
				return TRUE;
			}
		}
	}

	/* error */
	lpew->bPlay = FALSE;
	while(!PostMessage(lpew->hwndEVA, WM_USER+100, 0, 0));
	return FALSE;
}


/********************************************************************
  ディザ除去
 ********************************************************************/
void LPF(LPVOID lpDst, LPVOID lpSrc)
{
	int		cy[128*106];
	int		cu[128*106];
	int		cv[128*106];
	int		sy, su, sv;
	LPDWORD	lpdw;
	int		x,y,u,v,xx,yy,n,a;
	int		i, j;
	int		r,g,b;

	lpdw = (LPDWORD)lpSrc;
	for(i = 0; i < 128*106; i++){
		r = (*lpdw >> 16) & 255;
		g = (*lpdw >> 8) & 255;
		b = (*lpdw) & 255;
		lpdw++;
		cy[i] = ( 306 * r + 601 * g + 117 * b) / 1024;
		cu[i] = (-172 * r - 340 * g + 512 * b) / 1024;
		cv[i] = ( 512 * r - 428 * g -  84 * b) / 1024;
	}

	lpdw = (LPDWORD)lpDst;
	i = 0;
	for(y = 0; y < 106; y++){
		for(x = 0; x < 128; x++, i++){
			n = 0;
			sy = su = sv = 0;
			j = i - 128 - 1;
			for(u = -1; u <= 1; u++){
				for(v = -1; v <= 1; v++, j++){
					xx = x + u;
					yy = y + v;
					if(xx >= 0 && xx < 128 && yy >= 0 && yy < 106){
						if(abs(cy[i] - cy[j]) <= 12 && abs(cu[i] - cu[j]) <= 16 && abs(cv[i] - cv[j]) <= 16){
							a = (v == 0 && u == 0) ? 6 : 4;
							n += a;
							sy += cy[j] * a;
							su += cu[j] * a;
							sv += cv[j] * a;
						}
					}
				}
				j += 128 - 3;
			}
			sy *= 1024;
			sy /= n;
			su /= n;
			sv /= n;
			b = (sy + 1815 * su) / 1024;
			r = (sy + 1436 * sv) / 1024;
			g = (sy -  731 * sv - 352 * su) / 1024;
			if(r < 0) r = 0;
			if(r > 255) r = 255;
			if(g < 0) g = 0;
			if(g > 255) g = 255;
			if(b < 0) b = 0;
			if(b > 255) b = 255;
			*lpdw++ = b|(g<<8)|(r<<16);
		}
	}
}


/********************************************************************
  ディザ除去
 ********************************************************************/
void Adjust(LPVOID lpDst, LPVOID lpSrc, int nC, int nB, BOOL bMono)
{
	LPDWORD	lpdwSrc = (LPDWORD)lpSrc;
	LPDWORD	lpdwDst = (LPDWORD)lpDst;
	int		i;
	int		nn;
	int		r,g,b;

	nC = nC * 256 / 32;
	nB = nB * 255 / 31;
	if(nC > 0){
		nn = nB - 255 * nC / ((255 - nC) * 2);
	} else {
		nn = nB - nC / 2;
	}

	if(nC == 0){
		for(i = 0; i < 128*106; i++){
			r = (*lpdwSrc >> 16) & 255;
			g = (*lpdwSrc >> 8) & 255;
			b = *lpdwSrc++ & 255;
			r += nB;
			if(r < 0) r = 0;
			if(r > 255) r = 255;
			g += nB;
			if(g < 0) g = 0;
			if(g > 255) g = 255;
			b += nB;
			if(b < 0) b = 0;
			if(b > 255) b = 255;
			if(bMono) r = g = b = (g * 9 + r * 4 + b * 2) >> 4;
			*lpdwDst++ = b | (g << 8 ) | (r << 16);
		}
	} else if(nC > 0){
		for(i = 0; i < 128*106; i++){
			r = (*lpdwSrc >> 16) & 255;
			g = (*lpdwSrc >> 8) & 255;
			b = *lpdwSrc++ & 255;
			r = r * 255 / (256 - nC) + nn;
			if(r < 0) r = 0;
			if(r > 255) r = 255;
			g = g * 255 / (256 - nC) + nn;
			if(g < 0) g = 0;
			if(g > 255) g = 255;
			b = b * 255 / (256 - nC) + nn;
			if(b < 0) b = 0;
			if(b > 255) b = 255;
			if(bMono) r = g = b = (g * 9 + r * 4 + b * 2) >> 4;
			*lpdwDst++ = b | (g << 8 ) | (r << 16);
		}
	} else {
		for(i = 0; i < 128*106; i++){
			r = (*lpdwSrc >> 16) & 255;
			g = (*lpdwSrc >> 8) & 255;
			b = *lpdwSrc++ & 255;
			r = r * (255 + nC) / 255 + nn;
			if(r < 0) r = 0;
			if(r > 255) r = 255;
			g = g * (255 + nC) / 255 + nn;
			if(g < 0) g = 0;
			if(g > 255) g = 255;
			b = b * (255 + nC) / 255 + nn;
			if(b < 0) b = 0;
			if(b > 255) b = 255;
			if(bMono) r = g = b = (g * 9 + r * 4 + b * 2) >> 4;
			*lpdwDst++ = b | (g << 8 ) | (r << 16);
		}
	}
}


/********************************************************************
  フレーム描画
 ********************************************************************/
BOOL DrawFrame(LPEVACTRLWORK lpew, LPBYTE lpbBuff)
{
	HDC	hdcDst;
	HDC	hdcSrc;
	BYTE	byBuff[128*106*4];
/*
	BYTE	tpal[32];
		if (lpew->bisScreen5)
		{
			FILE *f = fopen(lpew->sFilename, "rb");
			if (f!=NULL)
			{
				long fp = (lpew->nNowFrame *  EVAFILEFRAMESIZE) + 0x3BFD; // + 0x3BFD;
				fseek(f,  fp , SEEK_SET);
				fread(&tpal, 32,1,f);
				fclose(f);

				for (int fr=0; fr<32; fr+=2)
				{
					SetPal(fr, tpal[fr], tpal[fr+1]);
				}
			}
		}
*/

	// 画像をデコード
	if(lpew->nContrast == 0 && lpew->nBright == 0 && !lpew->bMono)
	{
		if(lpew->bEnableFilter)
		{
			// forced screen 8?
			if (!lpew->bScreen8)
			{
			   // no, is screen 8 video?
			   if (lpew->bisScreen8)
			   {
				   Decode256(byBuff, lpbBuff);
			   } else {
				   // no, is screen 5 video?
				   if (lpew->bisScreen5)
				   {
					   Decode16(byBuff, lpbBuff);
				   } else {
					   // it's screen 12 video
				       DecodeYJK32(byBuff, lpbBuff);
				   }
			   }
			} else {
			   // forced screen 8 video
			   Decode256(byBuff, lpbBuff);
			}
			LPF(lpew->lpSurface, byBuff);
		} else {
			// forced screen 8?
			if (!lpew->bScreen8)
			{
			   // is it screen 8?
			   if (lpew->bisScreen8)
			   {
				    Decode256(lpew->lpSurface, lpbBuff);
			   } else {
				   // is it screen 5?
				   if (lpew->bisScreen5)
				   {
					   Decode16(lpew->lpSurface, lpbBuff);
				   } else {
					   // screen 12
					   DecodeYJK32(lpew->lpSurface, lpbBuff);
				   }
			   }
			} else {
			   // forced screen 8
			   Decode256(lpew->lpSurface, lpbBuff);
			}
		}
	} else {
		if(lpew->bEnableFilter)
		{
			if (!lpew->bScreen8)
			{
				if (lpew->bisScreen8)
				{
					Decode256(lpew->lpSurface, lpbBuff);
				} else {
					if (lpew->bisScreen5)
					{
					   Decode16(lpew->lpSurface, lpbBuff);
					} else {
					   DecodeYJK32(lpew->lpSurface, lpbBuff);
					}
				}
			} else {
			   Decode256(lpew->lpSurface, lpbBuff);
			}
			LPF(byBuff, lpew->lpSurface);
			Adjust(lpew->lpSurface, byBuff, lpew->nContrast, lpew->nBright, lpew->bMono);
		} else {
			if (!lpew->bScreen8)
			{
			   if (lpew->bisScreen8)
			   {
				    Decode256(byBuff, lpbBuff);
			   } else {
				   if (lpew->bisScreen5)
				   {
					  Decode16(byBuff, lpbBuff);
				   } else {
					  DecodeYJK32(byBuff, lpbBuff);
				   }
			   }
			} else {
			   Decode256(byBuff, lpbBuff);
			}
			Adjust(lpew->lpSurface, byBuff, lpew->nContrast, lpew->nBright, lpew->bMono);
		}
	}

	// 描画
	hdcDst = GetDC(lpew->hwndEVA);
	hdcSrc = CreateCompatibleDC(hdcDst);
	HBITMAP hbmOld = (HBITMAP)::SelectObject(hdcSrc, lpew->hbmSurface);

	HDC hdcTmp = CreateCompatibleDC(hdcDst);

	HBITMAP mem_bm = CreateCompatibleBitmap(hdcDst, lpew->rcClient.right, lpew->rcClient.bottom);
    HBITMAP hbmOld2 = (HBITMAP)::SelectObject(hdcTmp, mem_bm);

	StretchBlt(hdcTmp, 0, 0, lpew->rcClient.right, lpew->rcClient.bottom, hdcSrc, 0, 0, EVAWIDTH, EVAHEIGHT, SRCCOPY);


	if (lpew->bScanLines)
	{
		HBRUSH hbr = (HBRUSH) SelectObject(hdcDst, CreateSolidBrush(RGB(0, 0, 0)));
		for (int y=lpew->rcClient.top; y<lpew->rcClient.bottom; y+=2)
		{
			Rectangle(hdcTmp, lpew->rcClient.left, y, lpew->rcClient.right, y+1);
		}
	}
	

	BitBlt(hdcDst, 0,0,lpew->rcClient.right, lpew->rcClient.bottom, hdcTmp, 0, 0, SRCCOPY);

	SelectObject(hdcTmp, hbmOld2);
	DeleteObject(mem_bm);
	DeleteDC(hdcTmp);
	ReleaseDC(lpew->hwndEVA, hdcTmp);

	SelectObject(hdcSrc, hbmOld);
	DeleteDC(hdcSrc);


	ReleaseDC(lpew->hwndEVA, hdcDst);
	return TRUE;
}


/********************************************************************
  フレームレートとファイルフォーマットをチェックする
 ********************************************************************/
BOOL CheckRate(LPEVACTRLWORK lpew)
{
	int			i;
	DWORD		dwReaded;
	BYTE		byBuffer[EVAFILEFRAMESIZE];

	// PCM データ数をカウントする
	lpew->nFPS = 0;
	SetFilePointer(lpew->hFile, 0, NULL, FILE_BEGIN);
	for(i=0;i<16;i++){
		if(!ReadFile(lpew->hFile, byBuffer, EVAFILEFRAMESIZE, &dwReaded, NULL)) dwReaded = 0xFFFFFFFF;

		if(dwReaded != EVAFILEFRAMESIZE) break;

		lpew->nFPS += *(LPWORD)(byBuffer+EVAFILEPCMSIZEOFFSET);
		if(i == 0) lpew->bEnhance = *(LPWORD)(byBuffer+EVAFILEPCMSIZEOFFSET) < 1440;	// file format check
	}

	if(i == 0) return FALSE;	// 1frame も読めないのでエラー

	// PCM データ数からフレームレートを得る
	lpew->nFPS = (EVAPCMRATE * 1000) * i / lpew->nFPS;

	return TRUE;
}


/********************************************************************
  WaveOut Close
 ********************************************************************/
BOOL CloseWave(LPEVACTRLWORK lpew)
{
	if(!lpew->hwo) return FALSE;

	lpew->bBreak = TRUE;

	StopWave(lpew);
	waveOutClose(lpew->hwo);
	lpew->hwo = NULL;

	SetEvent(lpew->heventAudio);
	WaitForSingleObject(lpew->hthreadAudio, INFINITE);
	CloseHandle(lpew->hthreadAudio);
	lpew->hthreadAudio = NULL;
	CloseHandle(lpew->heventAudio);
	lpew->heventAudio = NULL;

	SetEvent(lpew->heventVideo);
	WaitForSingleObject(lpew->hthreadVideo, INFINITE);
	CloseHandle(lpew->hthreadVideo);
	lpew->hthreadVideo = NULL;
	CloseHandle(lpew->heventVideo);
	lpew->heventVideo = NULL;

	return TRUE;
}


/********************************************************************
  映像スレッド
 ********************************************************************/
DWORD VideoThread(LPEVACTRLWORK lpew)
{
	for(;;){
		WaitForSingleObject(lpew->heventVideo, INFINITE);
		if (lpew->bBreak) break;

		long fp = lpew->nNowFrame * EVAFILEFRAMESIZE;
		int nSize;
		ReadFrame(lpew, &lpew->byBuff[lpew->nVideo][0], &nSize);
		SetFilePointer(lpew->hFile, fp, 0, SEEK_SET);

		DrawFrame(lpew, &lpew->byBuff[lpew->nVideo][0]);

//		if(lpew->bSkip){
			lpew->nVideo = (lpew->nAudio - (lpew->nWaveBufferCount-1) + MAXEVABUFFER) % MAXEVABUFFER;
//		} else {
//			lpew->nVideo = (lpew->nVideo + 1) % MAXEVABUFFER;
//		}
	}
	return 0;
}


/********************************************************************
  音声スレッド
 ********************************************************************/
DWORD AudioThread(LPEVACTRLWORK lpew)
{
	int		n;

	for(;;){
		WaitForSingleObject(lpew->heventAudio, INFINITE);
		if(lpew->bBreak) break;

		long fp = lpew->nNowFrame * EVAFILEFRAMESIZE;
		SetFilePointer(lpew->hFile, fp, 0, SEEK_SET);

		n = 0;
		for(int i=0;i < lpew->nWaveBufferCount;i++){
			if(lpew->whdr[i].dwFlags & WHDR_DONE){
				n++;
				WaveBufferFree(lpew->hwo, &lpew->whdr[i]);
			}
		}

		if(lpew->bPlay){
			lpew->nNowFrame += n;
			if(lpew->nNowFrame >= lpew->nLastFrame){
				lpew->nNowFrame = lpew->nLastFrame;
				while(!PostMessage(lpew->hwndEVA, WM_USER+98, 0, 0));
				while(!PostMessage(GetParent(lpew->hwndEVA), WM_COMMAND, MAKELONG(GetWindowLong(lpew->hwndEVA, GWL_ID), EVAN_POS), (LPARAM)lpew->hwndEVA));
				while(!PostMessage(GetParent(lpew->hwndEVA), WM_COMMAND, MAKELONG(GetWindowLong(lpew->hwndEVA, GWL_ID), EVAN_TERMINATE), (LPARAM)lpew->hwndEVA));
			} else {
				PostMessage(GetParent(lpew->hwndEVA), WM_COMMAND, MAKELONG(GetWindowLong(lpew->hwndEVA, GWL_ID), EVAN_POS), (LPARAM)lpew->hwndEVA);
				for(int i=0;i<n;i++){
					SendWave(lpew);
				}
				SetEvent(lpew->heventVideo);
			}
		}
	}
	return 0;
}


/********************************************************************
  WaveOut Open
 ********************************************************************/
BOOL OpenWave(LPEVACTRLWORK lpew)
{
	MMRESULT		hr;
	WAVEFORMATEX	wfx;
	DWORD			dwID;

	lpew->nFreeAudio = 0;
	lpew->nWaveOutPost = 0;
	lpew->dwSample = 0;
	lpew->bBreak = FALSE;

	lpew->heventAudio = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(lpew->heventAudio != NULL){
		lpew->heventVideo = CreateEvent(NULL, FALSE, FALSE, NULL);
		if(lpew->heventVideo != NULL){
			wfx.wFormatTag = WAVE_FORMAT_PCM;
			wfx.nChannels = 1;
			wfx.nSamplesPerSec = EVAPCMRATE;
			wfx.nAvgBytesPerSec = EVAPCMRATE;
			wfx.nBlockAlign = 1;
			wfx.wBitsPerSample = 8;
			wfx.cbSize = 0;
			hr = waveOutOpen(&lpew->hwo, WAVE_MAPPER, &wfx, (DWORD)lpew->heventAudio, (DWORD)lpew, CALLBACK_EVENT);
			if(!hr){
				lpew->hthreadAudio = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AudioThread, lpew, 0, &dwID);
				if(lpew->hthreadAudio != NULL){
					lpew->hthreadVideo = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)VideoThread, lpew, 0, &dwID);
					if(lpew->hthreadVideo != NULL){
						return TRUE;
					} else MessageBox(lpew->hwndEVA, "can not create video thread", szEvaCtrlName, MB_OK);
					lpew->bBreak = TRUE;
					SetEvent(lpew->heventAudio);
					WaitForSingleObject(lpew->hthreadAudio, INFINITE);
					CloseHandle(lpew->hthreadAudio);
					lpew->hthreadAudio = NULL;
				} else MessageBox(lpew->hwndEVA, "can not create audio thread", szEvaCtrlName, MB_OK);
				waveOutClose(lpew->hwo);
				lpew->hwo = NULL;
			} else {
				char	szBuffer[256];
				waveOutGetErrorText(hr, szBuffer, sizeof(szBuffer));
				MessageBox(lpew->hwndEVA, szBuffer, szEvaCtrlName, MB_OK);
			}
			CloseHandle(lpew->heventVideo);
			lpew->heventVideo = NULL;
		} else MessageBox(lpew->hwndEVA, "can not create video event", szEvaCtrlName, MB_OK);
		CloseHandle(lpew->heventAudio);
		lpew->heventAudio = NULL;
	} else MessageBox(lpew->hwndEVA, "can not create audio event", szEvaCtrlName, MB_OK);
	return FALSE;
}




/********************************************************************
  WM_CREATE
 ********************************************************************/
LRESULT EvaControl_Create(HWND hwnd)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)malloc(sizeof(EVACTRLWORK));
	SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)lpew);
	if(lpew == NULL) return -1;

	ZeroMemory(lpew, sizeof(EVACTRLWORK));
	lpew->hFile = INVALID_HANDLE_VALUE;
	lpew->nContrast = 0;
	lpew->nBright = 0;
	lpew->nVol = 256;
	lpew->hwndEVA = hwnd;
	lpew->nWaveBufferCount = 4;
	if(CreateDIB(hwnd, lpew)) return 0;
	return -1;
}


/********************************************************************
  WM_DESTROY
 ********************************************************************/
LRESULT EvaControl_Destroy(HWND hwnd)
{
	SendMessage(hwnd, EVAM_STOP, 0, 0);
	SendMessage(hwnd, EVAM_CLOSE, 0, 0);

	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew != NULL){
		CloseWave(lpew);
		DeleteDIB(lpew);
		free(lpew);
	}
	return 0;
}


/********************************************************************
  WM_PAINT
 ********************************************************************/
LRESULT EvaControl_Paint(HWND hwnd)
{
	LPEVACTRLWORK	lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	HDC				hdcDst;
	PAINTSTRUCT		ps;
	HDC				hdcSrc;

	hdcDst = BeginPaint(hwnd, &ps);
	if(lpew != NULL){
		if(lpew->hFile == INVALID_HANDLE_VALUE){
			BitBlt(hdcDst, 0, 0, lpew->rcClient.right, lpew->rcClient.bottom, NULL, 0, 0, BLACKNESS);

		
		} else {
			hdcSrc = CreateCompatibleDC(hdcDst);
			HBITMAP hbmOld = (HBITMAP)::SelectObject(hdcSrc, lpew->hbmSurface);
			StretchBlt(hdcDst, 0, 0, lpew->rcClient.right, lpew->rcClient.bottom, hdcSrc, 0, 0, EVAWIDTH, EVAHEIGHT, SRCCOPY);

			SelectObject(hdcSrc, hbmOld);
			DeleteDC(hdcSrc);
		}

	}


	EndPaint(hwnd, &ps);
	return 0;
}


/********************************************************************
  WM_SIZE
 ********************************************************************/
LRESULT EvaControl_Size(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew != NULL){
		GetClientRect(hwnd, &lpew->rcClient);
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


/********************************************************************
  EVAM_OPEN
 ********************************************************************/
LRESULT EvaControl_EVA_Open(HWND hwnd, LPCSTR lpcstrFilename)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(!lpew) return 1;

	/* 再生中ならエラー */
	if(lpew->bPlay) return 1;

	/* ファイル閉じる */
	if(lpew->hFile != INVALID_HANDLE_VALUE){
		SendMessage(hwnd, EVAM_CLOSE, 0, 0);
	}

	//strcpy(lpew->sFilename, lpcstrFilename);
	lpew->sFilename = lpcstrFilename;

	/* ファイル開く */
	lpew->hFile = CreateFile(lpcstrFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(lpew->hFile != INVALID_HANDLE_VALUE){
		// check if screen8 file
		FILE *f = fopen(lpcstrFilename, "rb");
		BYTE flags;
		//char *dbg;

		if (f!=NULL)
		{
			fseek(f, 0x3BFD, SEEK_SET);
			fread(&flags, 1, 1, f);
			fclose(f);
			//sprintf(dbg, "flags: %d\n", flags);
			//OutputDebugString(dbg);
			if (flags==2)
			{
				lpew->bisScreen8 = TRUE;
			} else {
				if (flags==5)
				{
				   lpew->bisScreen5 = TRUE;
				} else {
				   lpew->bisScreen8 = FALSE;
				}
			}
		}

		DWORD dwSize = GetFileSize(lpew->hFile, NULL);
		if(dwSize != 0xFFFFFFFF){
			lpew->nLastFrame = dwSize / EVAFILEFRAMESIZE;
			CheckRate(lpew);

			/* とりあえず最初のフレーム表示 */
			lpew->nNowFrame = 0;
			return SendMessage(hwnd, WM_USER+99, 0, 0);
		}
	}
	MessageBox(hwnd, "Cannot open file", szEvaCtrlName, MB_OK);
	return 1;
}


/********************************************************************
  EVAM_CLOSE
 ********************************************************************/
LRESULT EvaControl_EVA_Close(HWND hwnd)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	BYTE	byBuffer[EVAFILEFRAMESIZE];

	if(!lpew) return 1;

	if(lpew->bPlay){
		SendMessage(hwnd, EVAM_STOP, 0, 0);
	}
	if(lpew->hFile != INVALID_HANDLE_VALUE){
		CloseHandle(lpew->hFile);
		lpew->hFile = INVALID_HANDLE_VALUE;
		ZeroMemory(byBuffer, sizeof(byBuffer));
		InvalidateRect(hwnd, NULL, TRUE);
	}
	return 0;
}


/********************************************************************
  EVAM_PLAY
 ********************************************************************/
LRESULT EvaControl_EVA_Play(HWND hwnd)
{
	if(SendMessage(hwnd,WM_USER+97,0,0)) return 1;
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(!lpew) return 1;

	SendMessage(GetParent(lpew->hwndEVA), WM_COMMAND, MAKELONG(GetWindowLong(lpew->hwndEVA, GWL_ID), EVAN_START), (LPARAM)lpew->hwndEVA);
	return 0;
}


/********************************************************************
  EVAM_STOP
 ********************************************************************/
LRESULT EvaControl_EVA_Stop(HWND hwnd)
{
	if(SendMessage(hwnd,WM_USER+98,0,0)) return 1;
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(!lpew) return 1;
	SendMessage(GetParent(lpew->hwndEVA), WM_COMMAND, MAKELONG(GetWindowLong(lpew->hwndEVA, GWL_ID), EVAN_STOP), (LPARAM)lpew->hwndEVA);
	return 0;
}


/********************************************************************
  EVAM_SEEK
 ********************************************************************/
LRESULT EvaControl_EVA_Seek(HWND hwnd, int nFrame)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);

	if(!lpew) return 1;

	if(nFrame >= lpew->nLastFrame) nFrame = lpew->nLastFrame;

	if(lpew->bPlay){
		SendMessage(hwnd, WM_USER+98, 0, 0);
		lpew->nNowFrame = nFrame;
		if(lpew->nNowFrame >= lpew->nLastFrame){
			SendMessage(GetParent(lpew->hwndEVA), WM_COMMAND, MAKELONG(GetWindowLong(lpew->hwndEVA, GWL_ID), EVAN_TERMINATE), (LPARAM)lpew->hwndEVA);
		} else {
			SendMessage(hwnd, WM_USER+97, 0, 0);
		}
	} else {
		lpew->nNowFrame = nFrame;
		SendMessage(hwnd, WM_USER+99, 0, 0);
		SendMessage(GetParent(lpew->hwndEVA), WM_COMMAND, MAKELONG(GetWindowLong(lpew->hwndEVA, GWL_ID), EVAN_POS), (LPARAM)lpew->hwndEVA);
	}
	return 0;
}


/********************************************************************
  EVAM_GETNOWFRAME
 ********************************************************************/
LRESULT EvaControl_EVA_GetNowFrame(HWND hwnd)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return -1;
	return lpew->nNowFrame;
}


/********************************************************************
  EVAM_GETRATE
 ********************************************************************/
LRESULT EvaControl_EVA_GetRate(HWND hwnd)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return -1;
	return lpew->nFPS;
}


/********************************************************************
  EVAM_GETLENGTH
 ********************************************************************/
LRESULT EvaControl_EVA_GetLength(HWND hwnd)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return -1;
	return lpew->nLastFrame;
}


/********************************************************************
  EVAM_ADJUST
 ********************************************************************/
LRESULT EvaControl_EVA_Adjust(HWND hwnd, int nContrast, int nBright)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return -1;
	lpew->nContrast = nContrast;
	lpew->nBright = nBright;
	if(!lpew->bPlay){
		SendMessage(hwnd, WM_USER+99, 0, 0);
	}
	return 0;
}


/********************************************************************
  EVAM_SETMONO
 ********************************************************************/
LRESULT EvaControl_EVA_SetMono(HWND hwnd, BOOL bMono)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return -1;
	lpew->bMono = bMono;
	if(!lpew->bPlay){
		SendMessage(hwnd, WM_USER+99, 0, 0);
	}
	return 0;
}


/********************************************************************
  EVAM_SETRGB
 ********************************************************************/
LRESULT EvaControl_EVA_SetRGB(HWND hwnd, BOOL bRGB)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return -1;
	lpew->bRGB = bRGB;
	if(lpew->bPlay){
		SendMessage(hwnd, WM_USER+98, 0, 0);
		DeleteDIB(lpew);
		CreateDIB(hwnd, lpew);
		SendMessage(hwnd, WM_USER+97, 0, 0);
	} else {
		DeleteDIB(lpew);
		CreateDIB(hwnd, lpew);
		SendMessage(hwnd, WM_USER+99, 0, 0);
	}
	return 0;
}


/********************************************************************
  EVAM_SETVOL
 ********************************************************************/
LRESULT EvaControl_EVA_SetVol(HWND hwnd, int nVol)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return -1;
	lpew->nVol = nVol;
	return 0;
}


/********************************************************************
  EVAM_SETWAVEBUFFERCOUNT
 ********************************************************************/
LRESULT EvaControl_EVA_SetWaveBufferCount(HWND hwnd, int nCount)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return -1;
	if(!lpew->bPlay){
		if(nCount > MAXWAVEBUFFER) nCount = MAXWAVEBUFFER;
		if(nCount < 2) nCount = 2;
		lpew->nWaveBufferCount = nCount;
	}
	return 0;
}


/********************************************************************
  EVAM_ENABLEFILTER
 ********************************************************************/
LRESULT EvaControl_EVA_EnableFilter(HWND hwnd, int n)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return -1;
	lpew->bEnableFilter = (BOOL)n;
	if(!lpew->bPlay){
		SendMessage(hwnd, WM_USER+99, 0, 0);
	}
	return 0;
}

LRESULT EvaControl_EVA_EnableScanlines(HWND hwnd, int n)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return -1;
	lpew->bScanLines = (BOOL)n;

	if(!lpew->bPlay){
		SendMessage(hwnd, WM_USER+99, 0, n);
	}
	return 0;
}

LRESULT EvaControl_EVA_ForceScreen8(HWND hwnd, int n)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return -1;
	lpew->bScreen8 = (BOOL)n;

	if(!lpew->bPlay){
		SendMessage(hwnd, WM_USER+99, 0, 0);
	}
	return 0;
}


/********************************************************************
  再生処理
 ********************************************************************/
LRESULT EvaControl_EVA_Play_L(HWND hwnd)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return 1;

	if(lpew->nNowFrame >= lpew->nLastFrame) lpew->nNowFrame = lpew->nLastFrame;

	if(lpew->hFile != INVALID_HANDLE_VALUE && lpew->bPlay == FALSE){
		if(OpenWave(lpew)){
			lpew->bPlay = TRUE;
			SetFilePointer(lpew->hFile, lpew->nNowFrame * EVAFILEFRAMESIZE, NULL, FILE_BEGIN);
			lpew->nVideo = lpew->nAudio = 0;
			lpew->nSendFrame = lpew->nNowFrame;
			BOOL bErr = FALSE;
			for(int i = 0; i < lpew->nWaveBufferCount; i++){
				if(!SendWave(lpew)){
					bErr = TRUE;
					break;
				}
				if(i == 0) SetEvent(lpew->heventVideo);
			}
			if(!bErr) return 0;
		}
	}
	return 1;
}


/********************************************************************
  停止処理
 ********************************************************************/
LRESULT EvaControl_EVA_Stop_L(HWND hwnd)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return 1;

	if(lpew->bPlay){
		lpew->bPlay = FALSE;
		CloseWave(lpew);
	}
	return 0;
}


/********************************************************************
  画面描画処理
 ********************************************************************/
LRESULT EvaControl_EVA_Preview_L(HWND hwnd)
{
	BYTE	byBuff[EVAFILEFRAMESIZE];
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return 1;

	int n = lpew->nNowFrame;
	if(n >= lpew->nLastFrame) n = lpew->nLastFrame - 1;
	SetFilePointer(lpew->hFile, n * EVAFILEFRAMESIZE, NULL, FILE_BEGIN);
	
	if(ReadFrame(lpew, byBuff, NULL)){
		DrawFrame(lpew, byBuff);
		return 0;
	}
	
	return 1;
}


/********************************************************************
  エラー処理
 ********************************************************************/
LRESULT EvaControl_EVA_Error_L(HWND hwnd)
{
	LPEVACTRLWORK lpew = (LPEVACTRLWORK)GetWindowLong(hwnd, GWL_USERDATA);
	if(lpew == NULL) return 1;

	lpew->bPlay = FALSE;
	CloseWave(lpew);
	SendMessage(hwnd, EVAM_CLOSE, 0, 0);
	SendMessage(GetParent(lpew->hwndEVA), WM_COMMAND, MAKELONG(GetWindowLong(lpew->hwndEVA, GWL_ID), EVAN_STOP), (LPARAM)lpew->hwndEVA);
	SendMessage(GetParent(lpew->hwndEVA), WM_COMMAND, MAKELONG(GetWindowLong(lpew->hwndEVA, GWL_ID), EVAN_ERROR), (LPARAM)lpew->hwndEVA);
	MessageBox(hwnd, "Cannot play this data", szEvaCtrlName, MB_OK);
	return 0;
}


/********************************************************************
  MainWindow の Window プロシージャ
 ********************************************************************/
LRESULT CALLBACK EvaControlProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
		case WM_CREATE:
			return EvaControl_Create(hwnd);
		case WM_DESTROY:
			return EvaControl_Destroy(hwnd);
		case WM_PAINT:
			return EvaControl_Paint(hwnd);
		case WM_SIZE:
			return EvaControl_Size(hwnd, uMsg, wParam, lParam);

		case EVAM_OPEN:
			return EvaControl_EVA_Open(hwnd, (LPCSTR)lParam);
		case EVAM_CLOSE:
			return EvaControl_EVA_Close(hwnd);
		case EVAM_PLAY:
			return EvaControl_EVA_Play(hwnd);
		case EVAM_STOP:
			return EvaControl_EVA_Stop(hwnd);
		case EVAM_SEEK:
			return EvaControl_EVA_Seek(hwnd, (int)lParam);
		case EVAM_GETNOWFRAME:
			return EvaControl_EVA_GetNowFrame(hwnd);
		case EVAM_GETRATE:
			return EvaControl_EVA_GetRate(hwnd);
		case EVAM_GETLENGTH:
			return EvaControl_EVA_GetLength(hwnd);
		case EVAM_ADJUST:
			return EvaControl_EVA_Adjust(hwnd, (int)wParam, (int)lParam);
		case EVAM_SETMONO:
			return EvaControl_EVA_SetMono(hwnd, (BOOL)lParam);
		case EVAM_SETRGB:
			return EvaControl_EVA_SetRGB(hwnd, (BOOL)lParam);
		case EVAM_SETVOL:
			return EvaControl_EVA_SetVol(hwnd, (int)wParam);
		case EVAM_SETWAVEBUFFERCOUNT:
			return EvaControl_EVA_SetWaveBufferCount(hwnd, (int)wParam);
		case EVAM_ENABLEFILTER:
			return EvaControl_EVA_EnableFilter(hwnd, (int)wParam);
		case EVAM_ENABLESCANLINES:
			return EvaControl_EVA_EnableScanlines(hwnd, (int)wParam);
		case EVAM_FORCESCREEN8:
			return EvaControl_EVA_ForceScreen8(hwnd, (int)wParam);

		case WM_USER+97:	/* PLAY */
			return EvaControl_EVA_Play_L(hwnd);
		case WM_USER+98:	/* STOP */
			return EvaControl_EVA_Stop_L(hwnd);
		case WM_USER+99:	/* PREVIEW */
			return EvaControl_EVA_Preview_L(hwnd);
		case WM_USER+100:	/* error */
			return EvaControl_EVA_Error_L(hwnd);
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


/********************************************************************
  WindowClass を登録する
 ********************************************************************/
BOOL RegisterEvaControlClass(HINSTANCE hInst)
{
	WNDCLASSEX	wcl;

	ZeroMemory(&wcl, sizeof(wcl));
	wcl.hInstance		= hInst;
	wcl.lpszClassName	= EVA_CLASS;
	wcl.lpfnWndProc		= EvaControlProc;
	wcl.style			= CS_VREDRAW|CS_HREDRAW;
	wcl.cbSize			= sizeof(WNDCLASSEX);
	wcl.hIcon			= LoadIcon(NULL, IDI_APPLICATION);
	wcl.hIconSm			= LoadIcon(NULL, IDI_WINLOGO);
	wcl.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wcl.lpszMenuName	= NULL;
	wcl.cbClsExtra		= 0;
	wcl.cbWndExtra		= 0;
	wcl.hbrBackground	= (HBRUSH)(COLOR_WINDOWTEXT+1);
	return RegisterClassEx(&wcl);
}
