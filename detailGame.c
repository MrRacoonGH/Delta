#define _CRT_SECURE_NO_WARNINGS

#include "detailGame.h"
#include "dashboardGame.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <gdiplus.h>

static HWND g_hwnd = NULL;
static int g_gameIndex = -1;
static int g_visible = 0;
static int g_clientW = 900, g_clientH = 600;

static HWND g_btnBack;
static HWND g_btnLaunch;

static HBITMAP g_banner = NULL;
static int g_bannerW = 0, g_bannerH = 0;
static int g_hasBanner = 0;

static HICON g_icon = NULL;

static ULONG_PTR g_gdiToken = 0;
static int g_gdiInited = 0;

/* playtime tracking */
static HANDLE g_proc = NULL;
static DWORD g_sessionStartTick = 0;   /* when launched (GetTickCount) */
static int g_sessionRunning = 0;       /* process currently open */
static int g_sessionAccumMsLast = 0;   /* ms mark of last committed update  */
static int g_sessionGameIndex = -1;    /* game being tracked */

static void InitGdi(void) {
    if (g_gdiInited) return;
    GdiplusStartupInput in;
    in.GdiplusVersion = 1;
    in.DebugEventCallback = NULL;
    in.SuppressBackgroundThread = FALSE;
    in.SuppressExternalCodecs = FALSE;
    if (GdiplusStartup(&g_gdiToken, &in, NULL) == Ok)
        g_gdiInited = 1;
}

static void FreeBanner(void) {
    if (g_banner) { DeleteObject(g_banner); g_banner = NULL; }
    g_hasBanner = 0;
    g_bannerW = g_bannerH = 0;
}

static void LoadBanner(const char *path) {
    if (!path || !path[0]) { FreeBanner(); return; }
    InitGdi();
    if (!g_gdiInited) { FreeBanner(); return; }

    FreeBanner();

    wchar_t w[1024];
    MultiByteToWideChar(CP_ACP, 0, path, -1, w, 1024);

    GpImage *img = NULL;
    if (GdipCreateBitmapFromFile(w, (GpBitmap**)&img) != Ok || !img) return;

    GdipGetImageWidth(img, (UINT*)&g_bannerW);
    GdipGetImageHeight(img, (UINT*)&g_bannerH);
    GdipCreateHBITMAPFromBitmap((GpBitmap*)img, &g_banner, RGB(30,30,30));
    g_hasBanner = 1;
    GdipDisposeImage(img);
}

void DetailInit(HWND hwnd) {
    g_hwnd = hwnd;
    g_btnBack = CreateWindow("BUTTON", "Retour",
        WS_CHILD | BS_DEFPUSHBUTTON,
        20, 20, 150, 30, hwnd, (HMENU)BTN_DETAIL_BACK,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    g_btnLaunch = CreateWindow("BUTTON", "Lancer le jeu",
        WS_CHILD | BS_DEFPUSHBUTTON,
        0, 0, 200, 44, hwnd, (HMENU)BTN_DETAIL_LAUNCH,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
}

void DetailShow(HWND hwnd, int show, int gameIndex) {
    (void)hwnd;
    g_visible = show;
    g_gameIndex = gameIndex;
    ShowWindow(g_btnBack, show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_btnLaunch, show ? SW_SHOW : SW_HIDE);

    if (show) {
        LoadBanner(GameGetBanner(gameIndex));

        if (g_icon) { DestroyIcon(g_icon); g_icon = NULL; }
        const char *ic = GameGetIcon(gameIndex);
        if (ic && ic[0]) {
            g_icon = (HICON)LoadImage(NULL, ic, IMAGE_ICON, 96, 96, LR_LOADFROMFILE);
        }
    } else {
        FreeBanner();
        if (g_icon) { DestroyIcon(g_icon); g_icon = NULL; }
    }

    if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
}

int DetailIsVisible(void) { return g_visible; }

void DetailResize(HWND hwnd, int width, int height) {
    g_clientW = width;
    g_clientH = height;
    SetWindowPos(g_btnBack, NULL, 20, 20, 150, 30, SWP_NOZORDER);
    SetWindowPos(g_btnLaunch, NULL,
                 (width - 200) / 2, height - 120, 200, 44, SWP_NOZORDER);
    InvalidateRect(hwnd, NULL, TRUE);
}

static void PlaytimeText(char *out, size_t n) {
    int minutes = GameGetPlaytime(g_gameIndex);
    if (g_sessionRunning && g_sessionGameIndex == g_gameIndex) {
        minutes += (int)((GetTickCount() - g_sessionStartTick) / 60000);
    }
    if (minutes >= 60) {
        snprintf(out, n, "Temps de jeu : %d h %02d min", minutes / 60, minutes % 60);
    } else {
        snprintf(out, n, "Temps de jeu : %d min", minutes);
    }
}

void DetailDraw(HDC hdc) {
    if (!g_visible) return;

    RECT rc = {0, 0, g_clientW, g_clientH};

    /* banner background or dark */
    if (g_hasBanner && g_banner) {
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP old = (HBITMAP)SelectObject(mem, g_banner);
        StretchBlt(hdc, 0, 0, g_clientW, g_clientH,
                   mem, 0, 0, g_bannerW, g_bannerH, SRCCOPY);
        SelectObject(mem, old);
        DeleteDC(mem);
    } else {
        HBRUSH b = CreateSolidBrush(RGB(25, 25, 30));
        FillRect(hdc, &rc, b);
        DeleteObject(b);
    }

    /* translucent overlay for readability */
    RECT topPlate = {0, 0, g_clientW, 60};
    HBRUSH plateBr = CreateSolidBrush(RGB(0,0,0));
    HPEN platePen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ oBr = SelectObject(hdc, plateBr);
    HGDIOBJ oPe = SelectObject(hdc, platePen);
    Rectangle(hdc, topPlate.left, topPlate.top, topPlate.right, topPlate.bottom);
    SelectObject(hdc, oBr);
    SelectObject(hdc, oPe);
    DeleteObject(platePen);
    DeleteObject(plateBr);

    HFONT bigFont = CreateFont(34, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    HFONT medFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, bigFont);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    /* large icon */
    HICON icon = g_icon ? g_icon : LoadIcon(NULL, IDI_APPLICATION);
    DrawIconEx(hdc, 40, 70, icon, 96, 96, 0, NULL, DI_NORMAL);

    /* name */
    RECT nameRc = {40, 190, g_clientW - 40, 230};
    DrawText(hdc, GameGetName(g_gameIndex), -1, &nameRc, DT_LEFT | DT_END_ELLIPSIS);

    /* playtime */
    SelectObject(hdc, medFont);
    RECT ptRc = {40, 240, g_clientW - 40, 270};
    char pt[128];
    PlaytimeText(pt, sizeof(pt));
    DrawText(hdc, pt, -1, &ptRc, DT_LEFT | DT_SINGLELINE);

    /* last played (from Steam) */
    SetTextColor(hdc, RGB(200, 200, 200));
    int lp = GameGetLastPlayed(g_gameIndex);
    if (lp > 0) {
        time_t t = (time_t)lp;
        struct tm *tm = localtime(&t);
        char datestr[32];
        if (tm) strftime(datestr, sizeof(datestr), "%d/%m/%Y", tm);
        else strcpy(datestr, "inconnue");
        char lbl[128];
        snprintf(lbl, sizeof(lbl), "Derniere partie : %s", datestr);
        RECT lpRc = {40, 270, g_clientW - 40, 296};
        DrawText(hdc, lbl, -1, &lpRc, DT_LEFT | DT_SINGLELINE);
    }

    /* path */
    SetTextColor(hdc, RGB(200, 200, 200));
    RECT pathRc = {40, 300, g_clientW - 40, 330};
    char lb[300];
    snprintf(lb, sizeof(lb), "Chemin : %s", GameGetPath(g_gameIndex));
    DrawText(hdc, lb, -1, &pathRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(hdc, oldFont);
    DeleteObject(bigFont);
    DeleteObject(medFont);
}

void DetailHandleCommand(HWND hwnd, WPARAM wParam) {
    int id = LOWORD(wParam);

    if (id == BTN_DETAIL_LAUNCH) {
        const char *path = GameGetPath(g_gameIndex);
        if (!path) return;

        /* launch and start tracking the process */
        STARTUPINFOA si = {0};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {0};
        if (CreateProcessA(NULL, (char*)path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            if (g_proc) CloseHandle(g_proc);
            g_proc = pi.hProcess;
            g_sessionStartTick = GetTickCount();
            g_sessionAccumMsLast = (int)GetTickCount();
            g_sessionGameIndex = g_gameIndex;
            g_sessionRunning = 1;
            SetTimer(hwnd, 2, 1000, NULL);
        } else {
            /* fallback to shell execute */
            ShellExecute(NULL, "open", path, NULL, NULL, SW_SHOW);
        }
        return;
    }

    if (id == BTN_DETAIL_BACK) {
        SaveGamesJson();
        DetailShow(hwnd, 0, -1);
        return;
    }
}

/* Called once per second while the detail page could have a running session */
void DetailOnTick(void) {
    if (!g_sessionRunning || !g_proc || g_sessionGameIndex < 0) return;

    /* refresh live playtime while running */
    if (g_hwnd && g_visible) InvalidateRect(g_hwnd, NULL, TRUE);

    DWORD code = 0;
    if (GetExitCodeProcess(g_proc, &code) && code != STILL_ACTIVE) {
        /* game closed -> commit elapsed time in whole minutes */
        DWORD now = GetTickCount();
        int elapsedMin = (int)((now - g_sessionStartTick) / 60000);
        if (elapsedMin > 0) {
            GameSetPlaytime(g_sessionGameIndex,
                            GameGetPlaytime(g_sessionGameIndex) + elapsedMin);
            SaveGamesJson();
        }
        CloseHandle(g_proc);
        g_proc = NULL;
        g_sessionRunning = 0;
        if (g_hwnd) KillTimer(g_hwnd, 2);
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
    }
}

void DetailDestroy(void) {
    /* commit any still-running session on app exit */
    if (g_sessionRunning && g_sessionGameIndex >= 0) {
        DWORD now = GetTickCount();
        int elapsedMin = (int)((now - g_sessionStartTick) / 60000);
        if (elapsedMin > 0)
            GameSetPlaytime(g_sessionGameIndex,
                            GameGetPlaytime(g_sessionGameIndex) + elapsedMin);
        SaveGamesJson();
    }
    if (g_proc) { CloseHandle(g_proc); g_proc = NULL; }
    g_sessionRunning = 0;
    FreeBanner();
    if (g_icon) { DestroyIcon(g_icon); g_icon = NULL; }
    if (g_gdiInited) GdiplusShutdown(g_gdiToken);
    g_gdiInited = 0;
}