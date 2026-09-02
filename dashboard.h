#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <windows.h>

#define BTN_RESOLUTION 1001
#define BTN_OPEN_GAME_PAGE 1002
#define BTN_EXIT_APP 1003

#define BTN_RES_800 2001
#define BTN_RES_1280 2002
#define BTN_RES_1600 2003
#define BTN_RES_1920 2004
#define BTN_RES_2560 2005

#define BTN_FULLSCREEN 2006
#define BTN_WINDOWED 2007
#define BTN_BACKGROUND 2008
#define BTN_SAVE 2009
#define BTN_RESET 2010

void DashboardInit(HWND hwnd);
void DashboardShow(int show);
void DashboardResize(HWND hwnd, int width, int height);
void DashboardDraw(HDC hdc);
void DashboardHandleCommand(HWND hwnd, WPARAM wParam);

void SettingsLoad(void);
void SettingsSave(HWND hwnd);
void SettingsReset(HWND hwnd);
void SettingsInit(HWND hwnd);

#endif
