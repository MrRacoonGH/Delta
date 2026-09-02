#include <windows.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include "dashboard.h"
#include "dashboardGame.h"
#include "musicPlayer.h"

enum Page {
    PAGE_DASHBOARD,
    PAGE_GAME
};

static enum Page currentPage = PAGE_DASHBOARD;

static HBITMAP g_bgBitmap = NULL;
static int g_bgW = 0, g_bgH = 0;
static int g_bgDark = 1;
static ULONG_PTR g_gdiplusToken = 0;
static int g_gdiplusInited = 0;
static char g_bgPath[1024] = "";
static const char *CONFIG_FILE = "deltagame.cfg";

static void LoadBackgroundImage(HWND hwnd, const char *path);

static void GetAppDir(char *out, int outSize) {
    GetModuleFileName(NULL, out, outSize);
    char *s = strrchr(out, '\\');
    if (s) s[1] = '\0'; else strcat(out, "\\");
}

void SettingsSave(HWND hwnd) {
    (void)hwnd;
    char dir[1024];
    GetAppDir(dir, sizeof(dir));
    char path[1100];
    snprintf(path, sizeof(path), "%s%s", dir, CONFIG_FILE);

    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "background=%s\n", g_bgPath);
    fclose(f);
}

void SettingsReset(HWND hwnd) {
    if (g_bgBitmap) { DeleteObject(g_bgBitmap); g_bgBitmap = NULL; }
    g_bgW = 0; g_bgH = 0; g_bgDark = 1; g_bgPath[0] = '\0';

    char dir[1024];
    GetAppDir(dir, sizeof(dir));
    char path[1100];
    snprintf(path, sizeof(path), "%s%s", dir, CONFIG_FILE);
    remove(path);

    InvalidateRect(hwnd, NULL, TRUE);
}

void SettingsInit(HWND hwnd) {
    char dir[1024];
    GetAppDir(dir, sizeof(dir));
    char path[1100];
    snprintf(path, sizeof(path), "%s%s", dir, CONFIG_FILE);

    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[1100];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "background=", 11) == 0) {
            line[strcspn(line, "\r\n")] = 0;
            LoadBackgroundImage(hwnd, line + 11);
        }
    }
    fclose(f);
}

static void InitGdiplus(void) {
    if (g_gdiplusInited) return;
    GdiplusStartupInput input;
    input.GdiplusVersion = 1;
    input.DebugEventCallback = NULL;
    input.SuppressBackgroundThread = FALSE;
    input.SuppressExternalCodecs = FALSE;
    if (GdiplusStartup(&g_gdiplusToken, &input, NULL) == Ok)
        g_gdiplusInited = 1;
}

static void LoadBackgroundImage(HWND hwnd, const char *path) {
    InitGdiplus();
    if (!g_gdiplusInited) return;

    if (g_bgBitmap) { DeleteObject(g_bgBitmap); g_bgBitmap = NULL; }

    strncpy(g_bgPath, path, sizeof(g_bgPath) - 1);
    g_bgPath[sizeof(g_bgPath) - 1] = '\0';

    wchar_t wpath[1024];
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, 1024);

    GpImage *image = NULL;
    if (GdipCreateBitmapFromFile(wpath, (GpBitmap**)&image) != Ok || !image) {
        return;
    }

    GdipGetImageWidth(image, (UINT*)&g_bgW);
    GdipGetImageHeight(image, (UINT*)&g_bgH);

    GdipCreateHBITMAPFromBitmap((GpBitmap*)image, &g_bgBitmap, RGB(0,0,0));

    /* compute average luminance */
    g_bgDark = 1;
    GpBitmap *bmp = (GpBitmap*)image;
    GpRect full = {0, 0, g_bgW, g_bgH};
    BitmapData bd;
    if (GdipBitmapLockBits(bmp, &full, ImageLockModeRead,
                           PixelFormat32bppARGB, &bd) == Ok) {
        unsigned long long sum = 0;
        unsigned long long samples = 0;
        unsigned char *base = (unsigned char*)bd.Scan0;
        int stride = (int)bd.Stride;
        for (int y = 0; y < g_bgH; y += 4) {
            unsigned char *row = base + (size_t)y * stride;
            for (int x = 0; x < g_bgW; x += 4) {
                unsigned char b = row[(size_t)x * 4 + 0];
                unsigned char g = row[(size_t)x * 4 + 1];
                unsigned char r = row[(size_t)x * 4 + 2];
                sum += (unsigned)r + (unsigned)g + (unsigned)b;
                samples++;
            }
        }
        if (samples > 0) {
            unsigned long long avg = sum / samples;
            g_bgDark = (avg < 384) ? 1 : 0;   /* avg per pixel of 3 channels */
        }
        GdipBitmapUnlockBits(bmp, &bd);
    }

    GdipDisposeImage(image);

    if (hwnd) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    switch (uMsg) {

        case WM_CREATE:
            DashboardInit(hwnd);
            DashboardGameInit(hwnd);
            DashboardShow(1);
            currentPage = PAGE_DASHBOARD;
            SettingsInit(hwnd);
            return 0;

        case WM_SIZE:
            DashboardResize(hwnd, LOWORD(lParam), HIWORD(lParam));
            DashboardGameResize(hwnd, LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            if (g_bgBitmap) {
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP old = (HBITMAP)SelectObject(memDC, g_bgBitmap);
                StretchBlt(hdc, 0, 0, rc.right, rc.bottom,
                           memDC, 0, 0, g_bgW, g_bgH, SRCCOPY);
                SelectObject(memDC, old);
                DeleteDC(memDC);
            } else {
                HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
                FillRect(hdc, &ps.rcPaint, bgBrush);
                DeleteObject(bgBrush);
            }

            DashboardDraw(hdc);
            DashboardGameDraw(hdc);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_COMMAND:

            if (LOWORD(wParam) >= BTN_MUSIC_PREV && LOWORD(wParam) <= BTN_MUSIC_NEXT) {
                MusicPlayerHandleCommand(hwnd, wParam);
                return 0;
            }

            if (LOWORD(wParam) == BTN_BACKGROUND) {
                char file[1024] = "";
                OPENFILENAME ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = "Images (*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tiff)\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tiff\0Tous les fichiers (*.*)\0*.*\0";
                ofn.lpstrFile = file;
                ofn.nMaxFile = sizeof(file);
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileName(&ofn)) {
                    LoadBackgroundImage(hwnd, file);
                }
                return 0;
            }

            if (currentPage == PAGE_DASHBOARD) {

                if (LOWORD(wParam) == BTN_OPEN_GAME_PAGE) {
                    DashboardShow(0);
                    DashboardGameShow(1);
                    currentPage = PAGE_GAME;
                    return 0;
                }

                DashboardHandleCommand(hwnd, wParam);
            }
            else {

                if (LOWORD(wParam) == BTN_BACK) {
                    DashboardGameShow(0);
                    DashboardShow(1);
                    currentPage = PAGE_DASHBOARD;
                    return 0;
                }

                DashboardGameHandleCommand(hwnd, wParam);
            }

            return 0;

        case WM_TIMER:
            if (wParam == TIMER_MUSIC_POS)
                MusicPlayerOnTimer();
            return 0;

        case MM_MCINOTIFY:
            MusicPlayerOnNotify(hwnd, wParam, lParam);
            return 0;

        case WM_DESTROY:
            MusicPlayerShutdown();
            DashboardGameDestroy();
            if (g_bgBitmap) DeleteObject(g_bgBitmap);
            if (g_gdiplusInited) GdiplusShutdown(g_gdiplusToken);
            PostQuitMessage(0);
            return 0;

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, g_bgDark ? RGB(255, 255, 255) : RGB(0, 0, 0));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    const char CLASS_NAME[] = "MainWindow";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "DeltaGame Launcher",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        900, 600,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
