#ifndef	_INC_EVA_H_
#define	_INC_EVA_H_

#include <windows.h>

#define	EVAM_OPEN				(WM_USER+0)
#define	EVAM_CLOSE				(WM_USER+1)
#define	EVAM_PLAY				(WM_USER+2)
#define	EVAM_STOP				(WM_USER+3)
#define	EVAM_SEEK				(WM_USER+4)
#define	EVAM_GETNOWFRAME		(WM_USER+5)
#define	EVAM_GETLENGTH			(WM_USER+6)
#define	EVAM_ADJUST				(WM_USER+7)
#define	EVAM_SETMONO			(WM_USER+8)
#define	EVAM_SETRGB				(WM_USER+9)
#define	EVAM_GETRATE			(WM_USER+10)
#define	EVAM_SETVOL				(WM_USER+11)
#define	EVAM_SETWAVEBUFFERCOUNT	(WM_USER+12)
#define	EVAM_ENABLEFILTER		(WM_USER+13)
#define	EVAM_ENABLESCANLINES	(WM_USER+14)
#define	EVAM_FORCESCREEN8		(WM_USER+15)
#define	EVA_CLASS			"EvaControl"

#define	EVAN_START			100
#define	EVAN_STOP			101
#define	EVAN_POS			102
#define	EVAN_TERMINATE		103
#define	EVAN_ERROR			104

BOOL RegisterEvaControlClass(HINSTANCE hInst);

#endif
