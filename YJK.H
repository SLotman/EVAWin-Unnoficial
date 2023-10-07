#ifndef _INC_YJK_H
#define _INC_YJK_H

#include <windows.h>

BOOL DecodeYJK32(LPVOID lpDst, LPVOID lpSrc);
BOOL Decode256(LPVOID lpDst, LPVOID lpSrc);
BOOL Decode16(LPVOID lpDst, LPVOID lpSrc);
void SetPal(int color, BYTE RB, BYTE G);
#endif
