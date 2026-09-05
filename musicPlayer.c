#define _CRT_SECURE_NO_WARNINGS
#define COBJMACROS

#include "musicPlayer.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfplay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAX_MUSIC_TRACKS 256
#define MAX_PATH_LEN     260
#define MAX_TITLE_LEN    256
#define MAX_ARTIST_LEN   256

static HWND hwndParent;

static char trackPaths[MAX_MUSIC_TRACKS][MAX_PATH_LEN];
static char trackTitles[MAX_MUSIC_TRACKS][MAX_TITLE_LEN];
static char trackArtists[MAX_MUSIC_TRACKS][MAX_ARTIST_LEN];
static int trackCount = 0;
static int currentTrack = -1;
static int isPlaying = 0;
static int playCounts[MAX_MUSIC_TRACKS];

static IMFPMediaPlayer *g_player = NULL;
static BOOL deviceOpen = FALSE;

static HWND lblMusicTitle;
static HWND btnPrev;
static HWND btnPlay;
static HWND btnNext;
static HWND lblDuration;

static void LogMsg(const char *msg) {
    char path[260];
    GetModuleFileName(NULL, path, 260);
    char *s = strrchr(path, '\\');
    if (s) { s[1] = '\0'; strcat(path, "debug.log"); }
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}

static void LogFmt(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    LogMsg(buf);
}

static void GetExeDir(char *out, int outSize) {
    GetModuleFileName(NULL, out, outSize);
    char *slash = strrchr(out, '\\');
    if (slash)
        slash[1] = '\0';
    else
        strcat(out, "\\");
}

static void FormatTime(DWORD ms, char *buf) {
    int totalSec = (int)(ms / 1000);
    sprintf(buf, "%d:%02d", totalSec / 60, totalSec % 60);
}

static BOOL IsAudioFile(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return FALSE;
    return (_stricmp(ext, ".mp3") == 0 ||
            _stricmp(ext, ".wav") == 0 ||
            _stricmp(ext, ".wma") == 0 ||
            _stricmp(ext, ".ogg") == 0 ||
            _stricmp(ext, ".flac") == 0 ||
            _stricmp(ext, ".m4a") == 0);
}

static void ParseTrackName(const char *filename, char *title, char *artist) {
    char name[MAX_TITLE_LEN];
    strncpy(name, filename, MAX_TITLE_LEN - 1);
    name[MAX_TITLE_LEN - 1] = '\0';

    char *dot = strrchr(name, '.');
    if (dot) *dot = '\0';

    char *dash = strstr(name, " - ");
    if (dash) {
        *dash = '\0';
        strncpy(artist, name, MAX_ARTIST_LEN - 1);
        artist[MAX_ARTIST_LEN - 1] = '\0';
        strncpy(title, dash + 3, MAX_TITLE_LEN - 1);
        title[MAX_TITLE_LEN - 1] = '\0';
    } else {
        strncpy(title, name, MAX_TITLE_LEN - 1);
        title[MAX_TITLE_LEN - 1] = '\0';
        strcpy(artist, "Artiste inconnu");
    }
}

static void GetAppDir2(char *out, int outSize) {
    GetModuleFileName(NULL, out, outSize);
    char *slash = strrchr(out, '\\');
    if (slash) slash[1] = '\0'; else strcat(out, "\\");
}

static void SaveMusicCounts(void) {
    char dir[520];
    GetAppDir2(dir, sizeof(dir));
    char path[540];
    snprintf(path, sizeof(path), "%smusic_stats.json", dir);

    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\n  \"tracks\": [\n");
    int first = 1;
    for (int i = 0; i < trackCount; i++) {
        if (playCounts[i] <= 0) continue;
        char esc[512];
        {
            size_t o = 0;
            const char *in = trackTitles[i];
            for (; *in && o < sizeof(esc) - 2; in++) {
                char c = *in;
                if (c == '\\' || c == '"') esc[o++] = '\\';
                esc[o++] = c;
            }
            esc[o] = '\0';
        }
        if (!first) fprintf(f, ",\n");
        fprintf(f, "    { \"title\": \"%s\", \"count\": %d }", esc, playCounts[i]);
        first = 0;
    }
    fprintf(f, "\n  ]\n}\n");
    fclose(f);
}

static void LoadMusicCounts(void) {
    char dir[520];
    GetAppDir2(dir, sizeof(dir));
    char path[540];
    snprintf(path, sizeof(path), "%smusic_stats.json", dir);

    FILE *f = fopen(path, "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return; }

    char *buffer = (char*)malloc((size_t)fsize + 1);
    if (!buffer) { fclose(f); return; }
    size_t read = fread(buffer, 1, (size_t)fsize, f);
    buffer[read] = '\0';
    fclose(f);

    char *ptr = buffer;
    while ((ptr = strstr(ptr, "\"title\"")) != NULL) {
        char *open = strchr(ptr, '"');
        if (!open) break;
        open = strchr(open + 1, '"');
        if (!open) break;
        char *close = strchr(open + 1, '"');
        if (!close) break;

        size_t len = (size_t)(close - open - 1);
        char title[512];
        if (len >= sizeof(title)) len = sizeof(title) - 1;
        memcpy(title, open + 1, len);
        title[len] = '\0';

        char unesc[512];
        {
            size_t o = 0;
            for (size_t k = 0; title[k]; k++) {
                if (title[k] == '\\' && (title[k + 1] == '\\' || title[k + 1] == '"')) {
                    unesc[o++] = title[k + 1];
                    k++;
                } else {
                    unesc[o++] = title[k];
                }
            }
            unesc[o] = '\0';
        }

        char *countStart = strstr(close, "\"count\"");
        int val = 0;
        if (countStart) {
            char *cv = strchr(countStart, ':');
            if (cv) val = atoi(cv + 1);
        }

        for (int i = 0; i < trackCount; i++) {
            if (strcmp(trackTitles[i], unesc) == 0 && playCounts[i] < val) {
                playCounts[i] = val;
                break;
            }
        }
        ptr = close + 1;
    }

    free(buffer);
}

static void ScanMusicFolder(void) {
    char dir[MAX_PATH_LEN];
    GetExeDir(dir, MAX_PATH_LEN);
    strcat(dir, "music");

    char searchPath[MAX_PATH_LEN];
    snprintf(searchPath, MAX_PATH_LEN, "%s\\*", dir);

    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        LogFmt("[SCAN] FindFirstFile FAILED (folder not found or empty)");
        return;
    }

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!IsAudioFile(fd.cFileName)) continue;
        if (trackCount >= MAX_MUSIC_TRACKS) break;

        snprintf(trackPaths[trackCount], MAX_PATH_LEN, "%s\\%s", dir, fd.cFileName);
        ParseTrackName(fd.cFileName, trackTitles[trackCount], trackArtists[trackCount]);
        LogFmt("[SCAN] found: %s -> title=%s artist=%s", fd.cFileName, trackTitles[trackCount], trackArtists[trackCount]);
        trackCount++;
    } while (FindNextFile(hFind, &fd));

    FindClose(hFind);
    LogFmt("[SCAN] total tracks: %d", trackCount);
}

/* Media Foundation player callback (manual C COM vtable) */
typedef struct _PlayerCallback PlayerCallback;
struct _PlayerCallback {
    const IMFPMediaPlayerCallbackVtbl *lpVtbl;
};

static HRESULT STDMETHODCALLTYPE CB_QueryInterface(IMFPMediaPlayerCallback *This, REFIID riid, void **ppv) {
    (void)This;
    if (IsEqualIID(riid, &IID_IUnknown)) {
        *ppv = This;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE CB_AddRef(IMFPMediaPlayerCallback *This) { (void)This; return 1; }
static ULONG STDMETHODCALLTYPE CB_Release(IMFPMediaPlayerCallback *This) { (void)This; return 1; }

/* forward declaration for NextTrack used in callback */
static void NextTrack(void);

static void STDMETHODCALLTYPE CB_OnMediaPlayerEvent(IMFPMediaPlayerCallback *This, MFP_EVENT_HEADER *pEventHeader);

static const IMFPMediaPlayerCallbackVtbl g_cbVtbl = {
    CB_QueryInterface,
    CB_AddRef,
    CB_Release,
    CB_OnMediaPlayerEvent
};

static PlayerCallback g_callback = { &g_cbVtbl };

static void STDMETHODCALLTYPE CB_OnMediaPlayerEvent(IMFPMediaPlayerCallback *This, MFP_EVENT_HEADER *pEventHeader) {
    (void)This;
    switch (pEventHeader->eEventType) {
    case MFP_EVENT_TYPE_PLAYBACK_ENDED:
        LogMsg("[CB] playback ended");
        if (isPlaying) {
            isPlaying = 0;
            SetWindowText(btnPlay, "Play");
            NextTrack();
        }
        break;
    default:
        break;
    }
    return;
}

static void CloseMCIDevice(void) {
    if (g_player) {
        IMFPMediaPlayer_Shutdown(g_player);
        IMFPMediaPlayer_Release(g_player);
        g_player = NULL;
    }
    deviceOpen = FALSE;
    isPlaying = 0;
}

static DWORD GetMCIDurationMs(void) {
    if (!g_player) return 0;
    PROPVARIANT var;
    PropVariantInit(&var);
    HRESULT hr = IMFPMediaPlayer_GetDuration(g_player, &MFP_POSITIONTYPE_100NS, &var);
    if (SUCCEEDED(hr) && var.vt == VT_UI8) {
        double ms = (double)var.uhVal.QuadPart / 10000.0;
        PropVariantClear(&var);
        return (DWORD)ms;
    }
    PropVariantClear(&var);
    return 0;
}

static DWORD GetMCIPositionMs(void) {
    if (!g_player) return 0;
    PROPVARIANT var;
    PropVariantInit(&var);
    HRESULT hr = IMFPMediaPlayer_GetPosition(g_player, &MFP_POSITIONTYPE_100NS, &var);
    if (SUCCEEDED(hr) && var.vt == VT_UI8) {
        double ms = (double)var.uhVal.QuadPart / 10000.0;
        PropVariantClear(&var);
        return (DWORD)ms;
    }
    PropVariantClear(&var);
    return 0;
}

static void SaveMusicCounts(void);
static void LoadMusicCounts(void);

static void PlayTrack(int index) {
    if (index < 0 || index >= trackCount) return;
    currentTrack = index;
    playCounts[index]++;

    CloseMCIDevice();

    LogFmt("[PLAY] playing: %s", trackPaths[index]);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_LITE);

    wchar_t wpath[MAX_PATH_LEN];
    MultiByteToWideChar(CP_ACP, 0, trackPaths[index], -1, wpath, MAX_PATH_LEN);

    HRESULT hr = MFPCreateMediaPlayer(wpath, TRUE, MFP_OPTION_NONE,
                                      (IMFPMediaPlayerCallback*)&g_callback,
                                      hwndParent, &g_player);
    if (FAILED(hr)) {
        LogFmt("[PLAY] MFPCreateMediaPlayer FAILED hr=0x%08lX", (unsigned long)hr);
        g_player = NULL;
        return;
    }

    deviceOpen = TRUE;
    isPlaying = 1;
    LogFmt("[PLAY] MFPCreateMediaPlayer OK, playing");

    char display[MAX_TITLE_LEN + MAX_ARTIST_LEN + 10];
    snprintf(display, sizeof(display), "%s - %s", trackTitles[index], trackArtists[index]);
    SetWindowText(lblMusicTitle, display);
    SetWindowText(btnPlay, "Pause");

    DWORD dur = GetMCIDurationMs();
    char posStr[16], durStr[16], buf[48];
    FormatTime(0, posStr);
    FormatTime(dur, durStr);
    snprintf(buf, sizeof(buf), "%s / %s", posStr, durStr);
    SetWindowText(lblDuration, buf);
}

static void PausePlayback(void) {
    if (!g_player || !isPlaying) return;
    IMFPMediaPlayer_Pause(g_player);
    isPlaying = 0;
    SetWindowText(btnPlay, "Play");
}

static void ResumePlayback(void) {
    if (!g_player || isPlaying) return;
    IMFPMediaPlayer_Play(g_player);
    isPlaying = 1;
    SetWindowText(btnPlay, "Pause");
}

static void PrevTrack(void) {
    if (trackCount == 0) return;
    int next = currentTrack - 1;
    if (next < 0) next = trackCount - 1;
    PlayTrack(next);
}

static void NextTrack(void) {
    if (trackCount == 0) return;
    int next = currentTrack + 1;
    if (next >= trackCount) next = 0;
    PlayTrack(next);
}

void MusicPlayerInit(HWND hwnd) {
    hwndParent = hwnd;

    LogMsg("[INIT] MusicPlayerInit called");

    ScanMusicFolder();
    LoadMusicCounts();

    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int cx = w / 2;
    int by = h - 45;
    int ty = h - 75;

    /* Centered symmetric layout (buttons row width 34 each, gap 10) */
    int btnW = 70, btnH = 28, gap = 10;
    int durW = 110;
    int totalW = btnW * 3 + gap * 2 + gap + durW; /* prev+play+next+gaps+dur */
    int x0 = cx - totalW / 2;

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    lblMusicTitle = CreateWindow(
        "STATIC", "Aucune piste",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        cx - 160, ty, 320, 20,
        hwnd, NULL,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessage(lblMusicTitle, WM_SETFONT, (WPARAM)hFont, TRUE);

    btnPrev = CreateWindow(
        "BUTTON", "< Prec",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        x0, by, btnW, btnH,
        hwnd, (HMENU)BTN_MUSIC_PREV,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessage(btnPrev, WM_SETFONT, (WPARAM)hFont, TRUE);

    btnPlay = CreateWindow(
        "BUTTON", "Play",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        x0 + btnW + gap, by, btnW, btnH,
        hwnd, (HMENU)BTN_MUSIC_PLAY,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessage(btnPlay, WM_SETFONT, (WPARAM)hFont, TRUE);

    btnNext = CreateWindow(
        "BUTTON", "Suiv >",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        x0 + 2 * (btnW + gap), by, btnW, btnH,
        hwnd, (HMENU)BTN_MUSIC_NEXT,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessage(btnNext, WM_SETFONT, (WPARAM)hFont, TRUE);

    lblDuration = CreateWindow(
        "STATIC", "0:00 / 0:00",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        x0 + 3 * (btnW + gap), by, durW, btnH,
        hwnd, NULL,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessage(lblDuration, WM_SETFONT, (WPARAM)hFont, TRUE);

    SetTimer(hwnd, TIMER_MUSIC_POS, 500, NULL);
}

void MusicPlayerShow(int show) {
    LogFmt("[SHOW] show=%d", show);
    ShowWindow(lblMusicTitle, show ? SW_SHOW : SW_HIDE);
    ShowWindow(btnPrev, show ? SW_SHOW : SW_HIDE);
    ShowWindow(btnPlay, show ? SW_SHOW : SW_HIDE);
    ShowWindow(btnNext, show ? SW_SHOW : SW_HIDE);
    ShowWindow(lblDuration, show ? SW_SHOW : SW_HIDE);
}

void MusicPlayerResize(HWND hwnd, int width, int height) {
    int cx = width / 2;
    int by = height - 45;
    int ty = height - 75;

    int btnW = 70, btnH = 28, gap = 10;
    int durW = 110;
    int totalW = btnW * 3 + gap * 2 + gap + durW;
    int x0 = cx - totalW / 2;

    SetWindowPos(lblMusicTitle, NULL, cx - 160, ty, 320, 20, SWP_NOZORDER);
    SetWindowPos(btnPrev, NULL, x0, by, btnW, btnH, SWP_NOZORDER);
    SetWindowPos(btnPlay, NULL, x0 + btnW + gap, by, btnW, btnH, SWP_NOZORDER);
    SetWindowPos(btnNext, NULL, x0 + 2 * (btnW + gap), by, btnW, btnH, SWP_NOZORDER);
    SetWindowPos(lblDuration, NULL, x0 + 3 * (btnW + gap), by, durW, btnH, SWP_NOZORDER);
}

void MusicPlayerHandleCommand(HWND hwnd, WPARAM wParam) {
    int id = LOWORD(wParam);
    LogFmt("[CMD] id=%d trackCount=%d isPlaying=%d", id, trackCount, isPlaying);

    if (id == BTN_MUSIC_PREV) {
        PrevTrack();
    } else if (id == BTN_MUSIC_PLAY) {
        if (trackCount == 0) return;
        if (!deviceOpen) {
            PlayTrack(0);
        } else if (isPlaying) {
            PausePlayback();
        } else {
            ResumePlayback();
        }
    } else if (id == BTN_MUSIC_NEXT) {
        NextTrack();
    }
}

void MusicPlayerOnTimer(void) {
    if (!deviceOpen || !isPlaying) return;

    DWORD pos = GetMCIPositionMs();
    DWORD dur = GetMCIDurationMs();

    char posStr[16], durStr[16], buf[48];
    FormatTime(pos, posStr);
    FormatTime(dur, durStr);
    snprintf(buf, sizeof(buf), "%s / %s", posStr, durStr);
    SetWindowText(lblDuration, buf);
}

void MusicPlayerOnNotify(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    (void)hwnd; (void)wParam; (void)lParam;
}

void MusicPlayerShutdown(void) {
    SaveMusicCounts();
    CloseMCIDevice();
    KillTimer(hwndParent, TIMER_MUSIC_POS);
    MFShutdown();
}

void MusicPlayerSaveCounts(void) {
    SaveMusicCounts();
}

int MusicPlayerGetTrackCount(void) { return trackCount; }

const char *MusicPlayerGetTrackTitle(int index) {
    if (index >= 0 && index < trackCount) return trackTitles[index];
    return "---";
}

const char *MusicPlayerGetTrackArtist(int index) {
    if (index >= 0 && index < trackCount) return trackArtists[index];
    return "---";
}

int MusicPlayerGetCurrentTrack(void) { return currentTrack; }

int MusicPlayerIsPlaying(void) { return isPlaying; }

void MusicPlayerPlayTrack(int index) {
    PlayTrack(index);
}

const char *MusicPlayerGetTrackPath(int index) {
    if (index >= 0 && index < trackCount) return trackPaths[index];
    return NULL;
}

int MusicPlayerGetPlayCount(int index) {
    if (index >= 0 && index < trackCount) return playCounts[index];
    return 0;
}
