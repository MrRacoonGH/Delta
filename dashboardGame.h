#ifndef DASHBOARDGAME_H
#define DASHBOARDGAME_H

#include <windows.h>

#define MAX_GAMES 64

#define BTN_BACK             1300
#define BTN_GAME_LAUNCH_BASE 3000
#define BTN_GAME_FOLDER_BASE 4000

typedef struct {
    char name[256];
    char path[260];
    char icon[260];
} GameEntry;

void DashboardGameInit(HWND hwnd);
void DashboardGameShow(int show);
void DashboardGameResize(HWND hwnd, int width, int height);
void DashboardGameDraw(HDC hdc);
void DashboardGameHandleCommand(HWND hwnd, WPARAM wParam);
void DashboardGameDestroy(void);

#endif