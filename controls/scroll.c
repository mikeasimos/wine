/*		
 * Interface code to SCROLLBAR widget
 *
 * Copyright  Martin Ayotte, 1993
 *
 * Small fixes and implemented SB_THUMBPOSITION
 * by Peter Broadhurst, 940611
 */

static char Copyright[] = "Copyright Martin Ayotte, 1993";

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "windows.h"
#include "sysmetrics.h"
#include "scroll.h"
#include "heap.h"
#include "win.h"
#include "prototypes.h"
#include "stddebug.h"
/* #define DEBUG_SCROLL */
/* #undef  DEBUG_SCROLL */
#include "debug.h"


HBITMAP hUpArrow = 0;
HBITMAP hDnArrow = 0;
HBITMAP hLfArrow = 0;
HBITMAP hRgArrow = 0;
HBITMAP hUpArrowD = 0;
HBITMAP hDnArrowD = 0;
HBITMAP hLfArrowD = 0;
HBITMAP hRgArrowD = 0;

  /* windows/graphics.c */
extern void GRAPH_DrawReliefRect( HDC hdc, RECT *rect,
                                  int thickness, BOOL pressed );

LPHEADSCROLL ScrollBarGetWindowAndStorage(HWND hWnd, WND **wndPtr);
LPHEADSCROLL ScrollBarGetStorageHeader(HWND hWnd);
LPHEADSCROLL GetScrollObjectStruct(HWND hWnd, int nBar);
void ScrollBarButtonDown(HWND hWnd, int nBar, int x, int y);
void ScrollBarButtonUp(HWND hWnd, int nBar, int x, int y);
void ScrollBarMouseMove(HWND hWnd, int nBar, WORD wParam, int x, int y);
void StdDrawScrollBar(HWND hWnd, HDC hDC, int nBar, LPRECT lprect, LPHEADSCROLL lphs);
int CreateScrollBarStruct(HWND hWnd);
void NC_CreateScrollBars(HWND hWnd);
LPHEADSCROLL AllocScrollBar(DWORD dwStyle, int width, int height);


/***********************************************************************
 *           WIDGETS_ScrollBarWndProc
 */
LONG ScrollBarWndProc( HWND hWnd, WORD message, WORD wParam, LONG lParam )
{    
	WND  	*wndPtr;
	LPHEADSCROLL lphs;
	PAINTSTRUCT ps;
	HDC		hDC;
	RECT 	rect;
	LPCREATESTRUCT lpCreat;
	POINT *pt;
	pt=(POINT*)&lParam;
	switch(message) {
    case WM_CREATE:
		lpCreat = (LPCREATESTRUCT)lParam;
		if (lpCreat->style & SBS_VERT) {
			if (lpCreat->style & SBS_LEFTALIGN)
				SetWindowPos(hWnd, 0, 0, 0, 16, lpCreat->cy, 
								SWP_NOZORDER | SWP_NOMOVE);
			if (lpCreat->style & SBS_RIGHTALIGN)
				SetWindowPos(hWnd, 0, lpCreat->x + lpCreat->cx - 16, 
						lpCreat->y, 16, lpCreat->cy, SWP_NOZORDER);
			}
		if (lpCreat->style & SBS_HORZ) {
			if (lpCreat->style & SBS_TOPALIGN)
				SetWindowPos(hWnd, 0, 0, 0, lpCreat->cx, 16,
								SWP_NOZORDER | SWP_NOMOVE);
			if (lpCreat->style & SBS_BOTTOMALIGN)
				SetWindowPos(hWnd, 0, lpCreat->x, 
						lpCreat->y + lpCreat->cy - 16, 
						lpCreat->cx, 16, SWP_NOZORDER);
			}
		CreateScrollBarStruct(hWnd);
		dprintf_scroll(stddeb,"ScrollBar Creation !\n");
		return 0;
	case WM_DESTROY:
		lphs = ScrollBarGetWindowAndStorage(hWnd, &wndPtr);
		if (lphs == 0) return 0;
		dprintf_scroll(stddeb,"ScrollBar WM_DESTROY %p !\n", lphs);
		free(lphs);
		*((LPHEADSCROLL *)&wndPtr->wExtra[1]) = 0;
		return 0;
	
	case WM_LBUTTONDOWN:
		SetCapture(hWnd);
		ScrollBarButtonDown(hWnd, SB_CTL, pt->x,pt->y);
		break;
	case WM_LBUTTONUP:
		ReleaseCapture();
		ScrollBarButtonUp(hWnd, SB_CTL, pt->x,pt->y);
		break;

	case WM_MOUSEMOVE:
		ScrollBarMouseMove(hWnd, SB_CTL, wParam, pt->x,pt->y);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
		lphs = ScrollBarGetWindowAndStorage(hWnd, &wndPtr);
		return(SendMessage(wndPtr->hwndParent, message, wParam, lParam));

    case WM_TIMER:
		dprintf_scroll(stddeb,"ScrollBar WM_TIMER wParam=%X lParam=%lX !\n", wParam, lParam);
		lphs = ScrollBarGetWindowAndStorage(hWnd, &wndPtr);
		KillTimer(hWnd, wParam);
		switch(lphs->ButtonDown) {
			case 0:
				lphs->TimerPending = FALSE;
				return 0;
			case 1:
			case 3:
				SendMessage(wndPtr->hwndParent, lphs->Direction, 
				SB_LINEUP, MAKELONG(0, hWnd));
				break;
			case 2:
			case 4:
				SendMessage(wndPtr->hwndParent, lphs->Direction, 
				SB_LINEDOWN, MAKELONG(0, hWnd));
				break;
			case 5:
				SendMessage(wndPtr->hwndParent, lphs->Direction, 
				SB_PAGEUP, MAKELONG(0, hWnd));
				break;
			case 6:
				SendMessage(wndPtr->hwndParent, lphs->Direction, 
				SB_PAGEDOWN, MAKELONG(0, hWnd));
				break;
			}
		SetTimer(hWnd, 1, 100, NULL);
		return 0;

	case WM_SETREDRAW:
#ifdef DEBUG_SCROLL
		printf("ScrollBar WM_SETREDRAW hWnd=%04X w=%04X !\n", hWnd, wParam);
#endif
		lphs = ScrollBarGetStorageHeader(hWnd);
		if (lphs == NULL) return 0;
		lphs->bRedrawFlag = wParam;
		break;

	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);
		lphs = ScrollBarGetStorageHeader(hWnd);
		if (lphs != NULL && lphs->bRedrawFlag) {
			GetClientRect(hWnd, &rect);
			StdDrawScrollBar(hWnd, hDC, SB_CTL, &rect, lphs);
			}
		EndPaint(hWnd, &ps);
		break;
	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
	}
	return(0);
}



void ScrollBarButtonDown(HWND hWnd, int nBar, int x, int y)
{
	LPHEADSCROLL lphs;
	HWND	hWndParent;
	RECT	rect;
	int		width, height;
	LONG	dwOwner;
	lphs = GetScrollObjectStruct(hWnd, nBar);
	if (nBar == SB_CTL) {
		hWndParent = GetParent(hWnd);
		dwOwner = MAKELONG(0, lphs->hWndOwner);
		dprintf_scroll(stddeb,"ScrollBarButtonDown SB_CTL // x=%d y=%d\n", x, y);
		}
	else {
		hWndParent = hWnd;
		dwOwner = 0L; 
		dprintf_scroll(stddeb,"ScrollBarButtonDown SB_?SCROLL // x=%d y=%d\n", x, y);
		}
/*
	SetFocus(lphs->hWndOwner);
*/
	CopyRect(&rect, &lphs->rect);
	dprintf_scroll(stddeb,"ScrollDown / x=%d y=%d left=%d top=%d right=%d bottom=%d \n",
					x, y, rect.left, rect.top, rect.right, rect.bottom);
	if (lphs->Direction == WM_VSCROLL) {
		width = rect.right - rect.left;
		if (y <= lphs->rectUp.bottom) {
			lphs->ButtonDown = 1;
			InvalidateRect(lphs->hWndOwner, &lphs->rectUp, TRUE); 
			dprintf_scroll(stddeb,"ScrollBarButtonDown send SB_LINEUP\n");
			SendMessage(hWndParent, lphs->Direction, 
								SB_LINEUP, dwOwner);
			}
		if (y >= lphs->rectDown.top) {
			lphs->ButtonDown = 2;
			InvalidateRect(lphs->hWndOwner, &lphs->rectDown, TRUE); 
			dprintf_scroll(stddeb,"ScrollBarButtonDown send SB_LINEDOWN\n");
			SendMessage(hWndParent, lphs->Direction, 
							SB_LINEDOWN, dwOwner);
			}
		if (y > lphs->rectUp.bottom && y < (lphs->CurPix + width)) {
			lphs->ButtonDown = 5;
			dprintf_scroll(stddeb,"ScrollBarButtonDown send SB_PAGEUP\n");
			SendMessage(hWndParent, lphs->Direction, 
								SB_PAGEUP, dwOwner);
			}
		if (y < lphs->rectDown.top && y > (lphs->CurPix + (width << 1))) {
			lphs->ButtonDown = 6;
			dprintf_scroll(stddeb,"ScrollBarButtonDown send SB_PAGEDOWN\n");
			SendMessage(hWndParent, lphs->Direction, 
							SB_PAGEDOWN, dwOwner);
			}
		if (lphs->MaxPix > 0 && y > (lphs->CurPix + width) &&
			y < (lphs->CurPix + (width << 1))) {
			lphs->ThumbActive = TRUE;
			dprintf_scroll(stddeb,"THUMB DOWN !\n");
			}
		}
	else {
		height = rect.bottom - rect.top;
		if (x <= lphs->rectUp.right) {
			lphs->ButtonDown = 3;
			InvalidateRect(lphs->hWndOwner, &lphs->rectUp, TRUE); 
			dprintf_scroll(stddeb,"ScrollBarButtonDown send SB_LINEUP\n");
			SendMessage(hWndParent, lphs->Direction, 
								SB_LINEUP, dwOwner);
			}
		if (x >= lphs->rectDown.left) {
			lphs->ButtonDown = 4;
			InvalidateRect(lphs->hWndOwner, &lphs->rectDown, TRUE); 
			dprintf_scroll(stddeb,"ScrollBarButtonDown send SB_LINEDOWN\n");
			SendMessage(hWndParent, lphs->Direction, 
							SB_LINEDOWN, dwOwner);
			}
		if (x > lphs->rectUp.right && x < (lphs->CurPix + height)) {
			lphs->ButtonDown = 5;
			dprintf_scroll(stddeb,"ScrollBarButtonDown send SB_PAGEUP\n");
			SendMessage(hWndParent, lphs->Direction, 
								SB_PAGEUP, dwOwner);
			}
		if (x < lphs->rectDown.left && x > (lphs->CurPix + (height << 1))) {
			lphs->ButtonDown = 6;
			dprintf_scroll(stddeb,"ScrollBarButtonDown send SB_PAGEDOWN\n");
			SendMessage(hWndParent, lphs->Direction, 
							SB_PAGEDOWN, dwOwner);
			}
		if (lphs->MaxPix > 0 && x > (lphs->CurPix + height) &&
			x < (lphs->CurPix + (height << 1))) {
			lphs->ThumbActive = TRUE;
			dprintf_scroll(stddeb,"THUMB DOWN !\n");
			}
		}
	if (lphs->ButtonDown != 0) {
		UpdateWindow(lphs->hWndOwner);
		if (!lphs->TimerPending && nBar == SB_CTL) {
			lphs->TimerPending = TRUE;
			SetTimer(lphs->hWndOwner, 1, 500, NULL);
			}
		}
}


void ScrollBarButtonUp(HWND hWnd, int nBar, int x, int y)
{
	LPHEADSCROLL lphs;
	RECT	rect;
	HDC		hDC;
	dprintf_scroll(stddeb,"ScrollBarButtonUp // x=%d y=%d\n", x, y); 
	lphs = GetScrollObjectStruct(hWnd, nBar);
	if(lphs->ThumbActive)
	  {
	    HWND hWndOwner,hWndParent;
	    if (nBar == SB_CTL) {
		hWndParent = GetParent(hWnd);
		hWndOwner = lphs->hWndOwner;
		}
	    else {
		hWndParent = hWnd;
		hWndOwner = 0;
		}

	
	    SendMessage(hWndParent, lphs->Direction, 
			SB_THUMBPOSITION, MAKELONG(lphs->ThumbVal, hWndOwner));
	    lphs->ThumbActive = FALSE;
	  }
	  
	if (lphs->ButtonDown != 0) {
		lphs->ButtonDown = 0;
		if (nBar == SB_CTL) {
			GetClientRect(lphs->hWndOwner, &rect);
			InvalidateRect(lphs->hWndOwner, &rect, TRUE);
			UpdateWindow(lphs->hWndOwner);
			}
		else {
			hDC = GetWindowDC(lphs->hWndOwner);
			StdDrawScrollBar(lphs->hWndOwner, hDC, nBar, &lphs->rect, lphs);
			ReleaseDC(lphs->hWndOwner, hDC);
			}
		}
}


void ScrollBarMouseMove(HWND hWnd, int nBar, WORD wParam, int x, int y)
{
	LPHEADSCROLL lphs;
	HWND	hWndParent;
	HWND	hWndOwner;

	if ((wParam & MK_LBUTTON) == 0) return;
	lphs = GetScrollObjectStruct(hWnd, nBar);
	if (lphs->ThumbActive == 0) return;
	if (nBar == SB_CTL) {
		hWndParent = GetParent(hWnd);
		hWndOwner = lphs->hWndOwner;
		dprintf_scroll(stddeb,"ScrollBarButtonMove SB_CTL // x=%d y=%d\n", x, y);
		}
	else {
		hWndParent = hWnd;
		hWndOwner = 0;
		dprintf_scroll(stddeb,"ScrollBarButtonMove SB_?SCROLL // x=%d y=%d\n", x, y);
		}

	if(x<lphs->rect.left||x>lphs->rect.right||
	   y<lphs->rect.top||y>lphs->rect.bottom)
	  {

	    dprintf_scroll(stddeb,"Rejecting thumb position !\n");
	    lphs->ThumbVal=lphs->CurVal;/*revert to last set position*/
	  }
	else
	  {
	
	    if (lphs->Direction == WM_VSCROLL) {
	      int butsiz = lphs->rect.right - lphs->rect.left;
	      y = y - butsiz - (butsiz >> 1);
	    }
	    else {
	      int butsiz = lphs->rect.bottom - lphs->rect.top;
	      y = x - butsiz - (butsiz >> 1);
	    }
	    if(y<0)y=0;
	    if(y>lphs->MaxPix)y=lphs->MaxPix;
	    lphs->ThumbVal = (y * (lphs->MaxVal - lphs->MinVal) / 
			      lphs->MaxPix) + lphs->MinVal;
	  }

	dprintf_scroll(stddeb,"Scroll WM_MOUSEMOVE val=%d pix=%d\n", 
		       lphs->ThumbVal, y);
	SendMessage(hWndParent, lphs->Direction, 
		SB_THUMBTRACK, MAKELONG(lphs->ThumbVal, hWndOwner));
}


LPHEADSCROLL ScrollBarGetWindowAndStorage(HWND hWnd, WND **wndPtr)
{
    WND  *Ptr;
    LPHEADSCROLL lphs;
    *(wndPtr) = Ptr = WIN_FindWndPtr(hWnd);
    if (Ptr == 0) {
    	fprintf(stderr,"Bad Window handle on ScrollBar !\n");
    	return 0;
    	}
    lphs = *((LPHEADSCROLL *)&Ptr->wExtra[1]);
    return lphs;
}


LPHEADSCROLL ScrollBarGetStorageHeader(HWND hWnd)
{
    WND  *wndPtr;
    LPHEADSCROLL lphs;
    wndPtr = WIN_FindWndPtr(hWnd);
    if (wndPtr == 0) {
    	fprintf(stderr,"Bad Window handle on ScrollBar !\n");
    	return 0;
    	}
    lphs = *((LPHEADSCROLL *)&wndPtr->wExtra[1]);
    return lphs;
}



void StdDrawScrollBar(HWND hWnd, HDC hDC, int nBar, LPRECT lprect, LPHEADSCROLL lphs)
{
	HWND	hWndParent;
	HBRUSH 	hBrush;
	HDC 	hMemDC;
        HBITMAP hOldBmp;
	BITMAP	bm;
	RECT 	rect;
	UINT  	w, w2, h, h2;

	if (lphs == NULL) return;
	if (!lphs->bRedrawFlag) return;
	dprintf_scroll(stddeb,"StdDrawScrollBar nBar=%04X !\n", nBar);
	if (lphs->Direction == WM_VSCROLL)
		dprintf_scroll(stddeb,"StdDrawScrollBar Vertical left=%d top=%d right=%d bottom=%d !\n", 
			lprect->left, lprect->top, lprect->right, lprect->bottom);
	else
		dprintf_scroll(stddeb,"StdDrawScrollBar Horizontal left=%d top=%d right=%d bottom=%d !\n", 
			lprect->left, lprect->top, lprect->right, lprect->bottom);
	if (nBar == SB_CTL)
		hWndParent = GetParent(hWnd);
	else
		hWndParent = lphs->hWndOwner;
	hBrush = SendMessage(hWndParent, WM_CTLCOLOR, (WORD)hDC,
					MAKELONG(hWnd, CTLCOLOR_SCROLLBAR));
	if (hBrush == (HBRUSH)NULL)  hBrush = GetStockObject(LTGRAY_BRUSH);
	CopyRect(&lphs->rect, lprect);
	CopyRect(&lphs->rectUp, lprect);
	CopyRect(&lphs->rectDown, lprect);
	CopyRect(&rect, lprect);
	w = rect.right - rect.left;
	h = rect.bottom - rect.top;
	if (w == 0 || h == 0) return;
	if (lphs->Direction == WM_VSCROLL) {
		if (h > 3 * w)
			lphs->MaxPix = h - 3 * w;
		else
			lphs->MaxPix = 0;
		if (h > 2 * w)
			h2 = w;
		else
			h2 = (h - 4) / 2;
		lphs->rectUp.bottom = h2;
		lphs->rectDown.top = rect.bottom - h2;
		}
	else {
		if (w > 3 * h)
			lphs->MaxPix = w - 3 * h;
		else
			lphs->MaxPix = 0;
		if (w > 2 * h)
			w2 = h;
		else
			w2 = (w - 4) / 2;
		lphs->rectUp.right = w2;
		lphs->rectDown.left = rect.right - w2;
		}
	if (lphs->MaxVal != lphs->MinVal)
	lphs->CurPix = lphs->MaxPix * (lphs->CurVal - lphs->MinVal) / 
    		(lphs->MaxVal - lphs->MinVal);
	if(lphs->CurPix <0)lphs->CurPix=0;
	if (lphs->CurPix > lphs->MaxPix)  lphs->CurPix = lphs->MaxPix;

	hMemDC = CreateCompatibleDC(hDC);
	if (lphs->Direction == WM_VSCROLL) {
		GetObject(hUpArrow, sizeof(BITMAP), (LPSTR)&bm);
		if (lphs->ButtonDown == 1)
			hOldBmp = SelectObject(hMemDC, hUpArrowD);
		else
			hOldBmp = SelectObject(hMemDC, hUpArrow);
		StretchBlt(hDC, rect.left, rect.top, w, h2, hMemDC, 
			0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
		GetObject(hDnArrow, sizeof(BITMAP), (LPSTR)&bm);
		if (lphs->ButtonDown == 2)
			SelectObject(hMemDC, hDnArrowD);
		else
			SelectObject(hMemDC, hDnArrow);
		StretchBlt(hDC, rect.left, rect.bottom - h2, w, h2, hMemDC, 
			0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
		rect.top += h2;
		rect.bottom -= h2;
		}
	else {
		GetObject(hLfArrow, sizeof(BITMAP), (LPSTR)&bm);
		if (lphs->ButtonDown == 3)
			hOldBmp = SelectObject(hMemDC, hLfArrowD);
		else
			hOldBmp = SelectObject(hMemDC, hLfArrow);
		StretchBlt(hDC, rect.left, rect.top, w2, h, hMemDC, 
			0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
		GetObject(hRgArrow, sizeof(BITMAP), (LPSTR)&bm);
		if (lphs->ButtonDown == 4)
			SelectObject(hMemDC, hRgArrowD);
		else
			SelectObject(hMemDC, hRgArrow);
		StretchBlt(hDC, rect.right - w2, rect.top, w2, h, hMemDC, 
			0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
		rect.left += w2;
		rect.right -= w2;
		}
        SelectObject( hMemDC, hOldBmp );
	DeleteDC(hMemDC);
	FillRect(hDC, &rect, hBrush);
	if (lphs->MaxPix != 0) {
		if (lphs->Direction == WM_VSCROLL)
			SetRect(&rect, rect.left, rect.top + lphs->CurPix, 
				rect.left + w, rect.top + lphs->CurPix + h2);
		else
			SetRect(&rect, rect.left + lphs->CurPix, rect.top, 
				rect.left + lphs->CurPix + w2, rect.top + h);
		FrameRect(hDC, &rect, GetStockObject(BLACK_BRUSH));
		InflateRect(&rect, -1, -1);
		FillRect(hDC, &rect, GetStockObject(LTGRAY_BRUSH));
		GRAPH_DrawReliefRect(hDC, &rect, 2, 0);
		InflateRect(&rect, -3, -3);
		GRAPH_DrawReliefRect(hDC, &rect, 1, 1);
		}
}



int CreateScrollBarStruct(HWND hWnd)
{
    int		width, height;
    WND  *wndPtr;
    LPHEADSCROLL lphs;
    wndPtr = WIN_FindWndPtr(hWnd);
    width = wndPtr->rectClient.right - wndPtr->rectClient.left;
    height = wndPtr->rectClient.bottom - wndPtr->rectClient.top;
    if (width <= height)
	lphs = AllocScrollBar(WS_VSCROLL, width, height);
    else
	lphs = AllocScrollBar(WS_HSCROLL, width, height);
    dprintf_scroll(stddeb,"CreateScrollBarStruct %p !\n", lphs);
    *((LPHEADSCROLL *)&wndPtr->wExtra[1]) = lphs;
    lphs->hWndOwner = hWnd;
    CopyRect(&lphs->rect, &wndPtr->rectClient);
    return TRUE;
}



LPHEADSCROLL AllocScrollBar(DWORD dwStyle, int width, int height)
{
	LPHEADSCROLL lphs;
	if (hUpArrow == (HBITMAP)NULL) 
		hUpArrow = LoadBitmap((HINSTANCE)NULL, MAKEINTRESOURCE(OBM_UPARROWI));
	if (hDnArrow == (HBITMAP)NULL) 
		hDnArrow = LoadBitmap((HINSTANCE)NULL, MAKEINTRESOURCE(OBM_DNARROWI));
	if (hLfArrow == (HBITMAP)NULL) 
		hLfArrow = LoadBitmap((HINSTANCE)NULL, MAKEINTRESOURCE(OBM_LFARROWI));
	if (hRgArrow == (HBITMAP)NULL) 
		hRgArrow = LoadBitmap((HINSTANCE)NULL, MAKEINTRESOURCE(OBM_RGARROWI));
	if (hUpArrowD == (HBITMAP)NULL) 
		hUpArrowD = LoadBitmap((HINSTANCE)NULL, MAKEINTRESOURCE(OBM_UPARROWD));
	if (hDnArrowD == (HBITMAP)NULL) 
		hDnArrowD = LoadBitmap((HINSTANCE)NULL, MAKEINTRESOURCE(OBM_DNARROWD));
	if (hLfArrowD == (HBITMAP)NULL) 
		hLfArrowD = LoadBitmap((HINSTANCE)NULL, MAKEINTRESOURCE(OBM_LFARROWD));
	if (hRgArrowD == (HBITMAP)NULL) 
		hRgArrowD = LoadBitmap((HINSTANCE)NULL, MAKEINTRESOURCE(OBM_RGARROWD));
	lphs = (LPHEADSCROLL)malloc(sizeof(HEADSCROLL));
	if (lphs == 0) {
		fprintf(stderr,"Bad Memory Alloc on ScrollBar !\n");
		return NULL;
		}
	lphs->bRedrawFlag = TRUE;
	lphs->ThumbActive = FALSE;
	lphs->TimerPending = FALSE;
	lphs->ButtonDown = 0;
	lphs->MinVal = 0;
	lphs->MaxVal = 100;
	lphs->CurVal = 0;
	lphs->CurPix = 0;
	SetRect(&lphs->rect, 0, 0, width, height);
	if (dwStyle & WS_VSCROLL) {
		if (height > 3 * width)
			lphs->MaxPix = height - 3 * width;
		else
			lphs->MaxPix = 0;
		lphs->Direction = WM_VSCROLL;
		}
	else {
		if (width > 3 * height)
			lphs->MaxPix = width - 3 * height;
		else
			lphs->MaxPix = 0;
		lphs->Direction = WM_HSCROLL;
		}
	if (lphs->MaxPix < 1)  lphs->MaxPix = 1;
	return lphs;
}


void NC_CreateScrollBars(HWND hWnd)
{
	RECT	rect;
	int		width, height;
	WND  	*wndPtr;
	LPHEADSCROLL lphs;
	wndPtr = WIN_FindWndPtr(hWnd);
	GetWindowRect(hWnd, &rect);
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;
	if (wndPtr->dwStyle & WS_VSCROLL) {
		if (wndPtr->dwStyle & WS_HSCROLL) height -= SYSMETRICS_CYHSCROLL;
		lphs = AllocScrollBar(WS_VSCROLL, SYSMETRICS_CXVSCROLL, height);
		dprintf_scroll(stddeb,"NC_CreateScrollBars Vertical %p !\n", 
			       lphs);
		lphs->rect.left = width - SYSMETRICS_CYVSCROLL;
		lphs->rect.right = width;
		lphs->hWndOwner = hWnd;
		wndPtr->VScroll = lphs;
	    wndPtr->scroll_flags |= 0x0001;
		if (wndPtr->dwStyle & WS_HSCROLL) height += SYSMETRICS_CYHSCROLL;
		}
	if (wndPtr->dwStyle & WS_HSCROLL) {
		if (wndPtr->dwStyle & WS_VSCROLL) width -= SYSMETRICS_CYVSCROLL;
		lphs = AllocScrollBar(WS_HSCROLL, width, SYSMETRICS_CYHSCROLL);
		dprintf_scroll(stddeb,"NC_CreateScrollBars Horizontal %p !\n", lphs);
		lphs->rect.top = height - SYSMETRICS_CYHSCROLL;
		lphs->rect.bottom = height;
		lphs->hWndOwner = hWnd;
		wndPtr->HScroll = lphs;
	    wndPtr->scroll_flags |= 0x0002;
		}
}


/*************************************************************************
 *			GetScrollObjectStruct [internal]
 */
LPHEADSCROLL GetScrollObjectStruct(HWND hWnd, int nBar)
{
    WND *wndPtr;
    if (nBar != SB_CTL) {
	wndPtr = WIN_FindWndPtr(hWnd);
    	if (nBar == SB_VERT) return (LPHEADSCROLL)wndPtr->VScroll;
    	if (nBar == SB_HORZ) return (LPHEADSCROLL)wndPtr->HScroll;
    	return NULL;
	}
    return ScrollBarGetStorageHeader(hWnd);
}


/*************************************************************************
 *			SetScrollPos [USER.62]
 */
int SetScrollPos(HWND hWnd, int nBar, int nPos, BOOL bRedraw)
{
	LPHEADSCROLL lphs;
	HDC		hDC;
	int 	nRet;
	lphs = GetScrollObjectStruct(hWnd, nBar);
	if (lphs == NULL) return 0;
	nRet = lphs->CurVal;
	lphs->CurVal = (short)nPos;
	if (lphs->MaxVal != lphs->MinVal)
	lphs->CurPix = lphs->MaxPix * (lphs->CurVal - lphs->MinVal) / 
    		(lphs->MaxVal - lphs->MinVal);
	if(lphs->CurPix <0)lphs->CurPix=0;

    if (lphs->CurPix > lphs->MaxPix)  lphs->CurPix = lphs->MaxPix;
    dprintf_scroll(stddeb,"SetScrollPos val=%d pixval=%d pixmax%d\n",
	    (short)nPos, lphs->CurPix, lphs->MaxPix);
    dprintf_scroll(stddeb,"SetScrollPos min=%d max=%d\n", 
	    lphs->MinVal, lphs->MaxVal);
    if ((bRedraw) && (IsWindowVisible(lphs->hWndOwner))) {
		if (nBar == SB_CTL) {
	        InvalidateRect(lphs->hWndOwner, &lphs->rect, TRUE);
	        UpdateWindow(lphs->hWndOwner);
			}
		else {
			if (lphs->rect.right != 0 && lphs->rect.bottom != 0) {
				hDC = GetWindowDC(lphs->hWndOwner);
				StdDrawScrollBar(lphs->hWndOwner, hDC, nBar, &lphs->rect, lphs);
				ReleaseDC(lphs->hWndOwner, hDC);
				}
			}
        }
    return nRet;
}



/*************************************************************************
 *			GetScrollPos [USER.63]
 */
int GetScrollPos(HWND hWnd, int nBar)
{
    LPHEADSCROLL lphs;
    lphs = GetScrollObjectStruct(hWnd, nBar);
    if (lphs == NULL) return 0;
    return lphs->CurVal;
}



/*************************************************************************
 *			SetScrollRange [USER.64]
 */
void SetScrollRange(HWND hWnd, int nBar, int MinPos, int MaxPos, BOOL bRedraw)
{
    LPHEADSCROLL lphs;
	HDC		hDC;
    lphs = GetScrollObjectStruct(hWnd, nBar);
    if (lphs == NULL) return;

/*    should a bad range be rejected here? 
 */
    lphs->MinVal = (short)MinPos;
    lphs->MaxVal = (short)MaxPos;
    if (lphs->MaxVal != lphs->MinVal)
      lphs->CurPix = lphs->MaxPix * (lphs->CurVal - lphs->MinVal) / 
	  (lphs->MaxVal - lphs->MinVal);
    if(lphs->CurPix <0)lphs->CurPix=0;
    if (lphs->CurPix > lphs->MaxPix)  lphs->CurPix = lphs->MaxPix;
    dprintf_scroll(stddeb,"SetScrollRange min=%d max=%d\n", 
		   lphs->MinVal, lphs->MaxVal);
    if ((bRedraw) && (IsWindowVisible(lphs->hWndOwner))) {
		if (nBar == SB_CTL) {
	        InvalidateRect(lphs->hWndOwner, &lphs->rect, TRUE);
	        UpdateWindow(lphs->hWndOwner);
			}
		else {
			if (lphs->rect.right != 0 && lphs->rect.bottom != 0) {
				hDC = GetWindowDC(lphs->hWndOwner);
				StdDrawScrollBar(lphs->hWndOwner, hDC, nBar, &lphs->rect, lphs);
				ReleaseDC(lphs->hWndOwner, hDC);
				}
			}
        }
}



/*************************************************************************
 *			GetScrollRange [USER.65]
 */
void GetScrollRange(HWND hWnd, int nBar, LPINT lpMin, LPINT lpMax)
{
    LPHEADSCROLL lphs;
    lphs = GetScrollObjectStruct(hWnd, nBar);
    if (lphs == NULL) return;
    *lpMin = lphs->MinVal;
    *lpMax = lphs->MaxVal;
}



/*************************************************************************
 *			ShowScrollBar [USER.267]
 */
void ShowScrollBar(HWND hWnd, WORD wBar, BOOL bFlag)
{
	WND  *wndPtr;
	dprintf_scroll(stddeb,"ShowScrollBar hWnd=%04X wBar=%d bFlag=%d\n", 
		       hWnd, wBar, bFlag);
	if (wBar == SB_CTL) {
		if (bFlag)
			ShowWindow(hWnd, SW_SHOW);
		else
			ShowWindow(hWnd, SW_HIDE);
		return;
		}
	wndPtr = WIN_FindWndPtr(hWnd);
	if ((wBar == SB_VERT) || (wBar == SB_BOTH)) {
		if (bFlag)
			wndPtr->scroll_flags |= 1;
		else
			wndPtr->scroll_flags &= ~1;
		}
	if ((wBar == SB_HORZ) || (wBar == SB_BOTH)) {
		if (bFlag)
			wndPtr->scroll_flags |= 2;
		else
			wndPtr->scroll_flags &= ~2;
		}
	SetWindowPos(hWnd, 0, 0, 0, 0, 0, 
		SWP_NOZORDER | SWP_NOMOVE | 
		SWP_NOSIZE | SWP_FRAMECHANGED);
}



