#ifndef DASHBOARDGAME_H
#define DASHBOARDGAME_H

#include <windows.h>

#define MAX_GAMES 200

#define CARD_W 220
#define CARD_H 150

#define BTN_BACK             1300
#define BTN_GAME_LAUNCH_BASE 3000
#define BTN_GAME_FOLDER_BASE 4000

typedef struct {
    char name[256];
    char path[260];
    char icon[260];
    char banner[260];
    int  playtime;   /* minutes */
} GameEntry;

void DashboardGameInit(HWND hwnd);
void DashboardGameShow(int show);
void DashboardGameResize(HWND hwnd, int width, int height);
void DashboardGameDraw(HDC hdc);
void DashboardGameHandleCommand(HWND hwnd, WPARAM wParam);
void DashboardGameWheel(HWND hwnd, int wheelDelta);
int  DashboardGameHitTest(HWND hwnd, int x, int y);
int  DashboardGameIsVisible(void);
int  DashboardGameIndexAt(int x, int y);
const char *GameGetPath(int index);
void GameOpenFolder(int index);
void SaveGamesJson(void);
int  GameCount(void);
const char *GameGetName(int index);
const char *GameGetBanner(int index);
const char *GameGetIcon(int index);
int  GameGetPlaytime(int index);
void GameSetPlaytime(int index, int minutes);
void DashboardGameDestroy(void);

#endif