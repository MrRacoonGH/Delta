#ifndef DETAILGAME_H
#define DETAILGAME_H

#include <windows.h>

#define BTN_DETAIL_BACK     1500
#define BTN_DETAIL_LAUNCH   1501

void DetailInit(HWND hwnd);
void DetailShow(HWND hwnd, int show, int gameIndex);
int  DetailIsVisible(void);
void DetailResize(HWND hwnd, int width, int height);
void DetailDraw(HDC hdc);
void DetailHandleCommand(HWND hwnd, WPARAM wParam);
void DetailOnTick(void);
void DetailDestroy(void);

#endif