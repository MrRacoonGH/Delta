#ifndef MUSICPAGE_H
#define MUSICPAGE_H

#include <windows.h>

#define BTN_MUSIC_BACK       5010
#define BTN_MUSIC_NEWPL      5020
#define BTN_MUSIC_RENAME     5021
#define BTN_MUSIC_DELETE     5022
#define BTN_MUSIC_ALL        5030
#define BTN_MUSIC_NEWPL_OK   5040
#define BTN_MUSIC_NEWPL_CANCEL 5041

#define MUSIC_PL_TAB_BASE 5100
#define MUSIC_PL_TAB_MAX  32

#define MUSIC_ADD_MENU_BASE  8000
#define MUSIC_ADD_MENU_NEW   9000
#define MUSIC_MENU_ADDTOTRACK 6000

#define MUSIC_RET_NONE    0
#define MUSIC_RET_BACK    1

void MusicPageInit(HWND hwnd);
void MusicPageShow(int show);
void MusicPageHide(void);
void MusicPageDraw(HDC hdc);
int  MusicPageHandleCommand(HWND hwnd, WPARAM wParam);
void MusicPageOnLButtonDown(int x, int y);
void MusicPageOnRButtonDown(int x, int y);
void MusicPageOnMouseWheel(int delta);
void MusicPageOnMouseMove(int x, int y);
int  MusicPageHandleKey(WPARAM wParam);
void MusicPageOnTimer(void);
void MusicPageShowContextMenu(HWND hwnd, int x, int y, int trackIndex);
void MusicPageSetCover(int plIndex);
void MusicPageCancelNewPl(void);
void MusicPageResetPlaylists(void);

#endif
