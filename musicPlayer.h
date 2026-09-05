#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <windows.h>

#define BTN_MUSIC_PREV 5000
#define BTN_MUSIC_PLAY 5001
#define BTN_MUSIC_NEXT 5002

#define TIMER_MUSIC_POS 1

void MusicPlayerInit(HWND hwnd);
void MusicPlayerShow(int show);
void MusicPlayerResize(HWND hwnd, int width, int height);
void MusicPlayerHandleCommand(HWND hwnd, WPARAM wParam);
void MusicPlayerOnTimer(void);
void MusicPlayerOnNotify(HWND hwnd, WPARAM wParam, LPARAM lParam);
void MusicPlayerShutdown(void);
void MusicPlayerSaveCounts(void);

int  MusicPlayerGetTrackCount(void);
const char *MusicPlayerGetTrackTitle(int index);
const char *MusicPlayerGetTrackArtist(int index);
int  MusicPlayerGetCurrentTrack(void);
int  MusicPlayerIsPlaying(void);
void MusicPlayerPlayTrack(int index);
const char *MusicPlayerGetTrackPath(int index);
int  MusicPlayerGetPlayCount(int index);

#endif
