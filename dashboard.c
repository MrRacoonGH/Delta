// dashboard.c

#include "dashboard.h"
#include "dashboardGame.h"
#include "musicPlayer.h"
#include "statsPanel.h"
#include <windows.h>

static HWND btnResolution;
static HWND btnGames;
static HWND btnExit;
static HWND btnBackground;
static HWND btnSave;
static HWND btnReset;
static HWND btnMusic;

static HWND btnRes800;
static HWND btnRes1280;
static HWND btnRes1600;
static HWND btnRes1920;
static HWND btnRes2560;

static HWND btnFullscreen;
static HWND btnWindowed;

static int displayMode = 0; // 0 = fenêtré, 1 = plein écran (sélection)
static int lastW = 0;
static int lastH = 0;
static int hasResolution = 0;

static int dashboardVisible = 0;

void DashboardInit(HWND hwnd) {

    btnResolution = CreateWindow(
        "BUTTON", "Resolution",
        WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        20, 20, 200, 30,
        hwnd, (HMENU)BTN_RESOLUTION,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    btnGames = CreateWindow(
        "BUTTON", "Jeux",
        WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        20, 70, 200, 30,
        hwnd, (HMENU)BTN_OPEN_GAME_PAGE,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    btnMusic = CreateWindow(
        "BUTTON", "Musique",
        WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        20, 120, 200, 30,
        hwnd, (HMENU)BTN_MUSIC_PAGE,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    btnExit = CreateWindow(
        "BUTTON", "Fermer l'application",
        WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        20, 170, 200, 30,
        hwnd, (HMENU)BTN_EXIT_APP,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    btnBackground = CreateWindow(
        "BUTTON", "Background",
        WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        20, 220, 200, 30,
        hwnd, (HMENU)BTN_BACKGROUND,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    btnSave = CreateWindow(
        "BUTTON", "Sauvegarder",
        WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        20, 270, 200, 30,
        hwnd, (HMENU)BTN_SAVE,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    btnReset = CreateWindow(
        "BUTTON", "Reset",
        WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        20, 320, 200, 30,
        hwnd, (HMENU)BTN_RESET,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    // Boutons de résolution (cachés au début)
    btnRes800 = CreateWindow(
        "BUTTON", "800 x 600",
        WS_CHILD | BS_DEFPUSHBUTTON,
        250, 20, 120, 30,
        hwnd, (HMENU)BTN_RES_800,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    btnRes1280 = CreateWindow(
        "BUTTON", "1280 x 720",
        WS_CHILD | BS_DEFPUSHBUTTON,
        250, 60, 120, 30,
        hwnd, (HMENU)BTN_RES_1280,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    btnRes1600 = CreateWindow(
        "BUTTON", "1600 x 900",
        WS_CHILD | BS_DEFPUSHBUTTON,
        250, 100, 120, 30,
        hwnd, (HMENU)BTN_RES_1600,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    btnRes1920 = CreateWindow(
        "BUTTON", "1920 x 1080",
        WS_CHILD | BS_DEFPUSHBUTTON,
        250, 140, 120, 30,
        hwnd, (HMENU)BTN_RES_1920,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    btnRes2560 = CreateWindow(
        "BUTTON", "2560 x 1440",
        WS_CHILD | BS_DEFPUSHBUTTON,
        250, 180, 120, 30,
        hwnd, (HMENU)BTN_RES_2560,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    // Boutons de mode d’affichage (cachés au début)
    btnFullscreen = CreateWindow(
        "BUTTON", "Plein écran",
        WS_CHILD | WS_GROUP | BS_AUTORADIOBUTTON,
        250, 230, 120, 30,
        hwnd, (HMENU)BTN_FULLSCREEN,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    btnWindowed = CreateWindow(
        "BUTTON", "Fenêtré",
        WS_CHILD | BS_AUTORADIOBUTTON,
        250, 270, 120, 30,
        hwnd, (HMENU)BTN_WINDOWED,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    MusicPlayerInit(hwnd);
}

void DashboardShow(int show) {
    dashboardVisible = show;

    ShowWindow(btnResolution, show ? SW_SHOW : SW_HIDE);
    ShowWindow(btnGames, show ? SW_SHOW : SW_HIDE);
    ShowWindow(btnMusic, show ? SW_SHOW : SW_HIDE);
    ShowWindow(btnExit, show ? SW_SHOW : SW_HIDE);
    ShowWindow(btnBackground, show ? SW_SHOW : SW_HIDE);
    ShowWindow(btnSave, show ? SW_SHOW : SW_HIDE);
    ShowWindow(btnReset, show ? SW_SHOW : SW_HIDE);

    /* Mini-player controls are hidden on dashboard — only visible on music page */
    MusicPlayerShow(0);

    // Les boutons de résolution et de mode sont cachés par défaut
    ShowWindow(btnRes800, SW_HIDE);
    ShowWindow(btnRes1280, SW_HIDE);
    ShowWindow(btnRes1600, SW_HIDE);
    ShowWindow(btnRes1920, SW_HIDE);
    ShowWindow(btnRes2560, SW_HIDE);

    ShowWindow(btnFullscreen, SW_HIDE);
    ShowWindow(btnWindowed, SW_HIDE);

    // Synchronise la sélection du mode au prochain affichage
    SendMessage(btnFullscreen, BM_SETCHECK, displayMode == 1 ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(btnWindowed, BM_SETCHECK, displayMode == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
}

void DashboardResize(HWND hwnd, int width, int height) {
    SetWindowPos(btnResolution, NULL, 20, 20, 200, 30, SWP_NOZORDER);
    SetWindowPos(btnGames, NULL, 20, 70, 200, 30, SWP_NOZORDER);
    SetWindowPos(btnMusic, NULL, 20, 120, 200, 30, SWP_NOZORDER);
    SetWindowPos(btnExit, NULL, 20, 170, 200, 30, SWP_NOZORDER);
    SetWindowPos(btnBackground, NULL, 20, 220, 200, 30, SWP_NOZORDER);
    SetWindowPos(btnSave, NULL, 20, 270, 200, 30, SWP_NOZORDER);
    SetWindowPos(btnReset, NULL, 20, 320, 200, 30, SWP_NOZORDER);

    SetWindowPos(btnRes800, NULL, 250, 20, 120, 30, SWP_NOZORDER);
    SetWindowPos(btnRes1280, NULL, 250, 60, 120, 30, SWP_NOZORDER);
    SetWindowPos(btnRes1600, NULL, 250, 100, 120, 30, SWP_NOZORDER);
    SetWindowPos(btnRes1920, NULL, 250, 140, 120, 30, SWP_NOZORDER);
    SetWindowPos(btnRes2560, NULL, 250, 180, 120, 30, SWP_NOZORDER);

    SetWindowPos(btnFullscreen, NULL, 250, 230, 120, 30, SWP_NOZORDER);
    SetWindowPos(btnWindowed, NULL, 250, 270, 120, 30, SWP_NOZORDER);

    MusicPlayerResize(hwnd, width, height);
}

void DashboardDraw(HDC hdc) {
    (void)hdc;
}

void DashboardStatsRefresh(void) {
    StatsPanelRefresh();
}

void DashboardStatsDraw(HDC hdc, int width, int height) {
    if (!dashboardVisible) return;
    StatsPanelDraw(hdc, width, height);
}

static void ApplyWindowed(HWND hwnd, int w, int h) {
    RECT rc = {0, 0, w, h};
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);

    SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    SetWindowPos(hwnd, NULL, 0, 0,
                 rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    ShowWindow(hwnd, SW_RESTORE);
}

static void ApplyFullscreen(HWND hwnd) {
    SetWindowLong(hwnd, GWL_STYLE, WS_POPUP);
    SetWindowPos(hwnd, NULL, 0, 0,
                 GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                 SWP_NOZORDER | SWP_FRAMECHANGED);
    ShowWindow(hwnd, SW_RESTORE);
}

void DashboardHandleCommand(HWND hwnd, WPARAM wParam) {

    switch (LOWORD(wParam)) {

        case BTN_RESOLUTION:
            if (IsWindowVisible(btnRes800)) {
                // L'onglet est déjà ouvert : on le ferme
                ShowWindow(btnRes800, SW_HIDE);
                ShowWindow(btnRes1280, SW_HIDE);
                ShowWindow(btnRes1600, SW_HIDE);
                ShowWindow(btnRes1920, SW_HIDE);
                ShowWindow(btnRes2560, SW_HIDE);

                ShowWindow(btnFullscreen, SW_HIDE);
                ShowWindow(btnWindowed, SW_HIDE);
            } else {
                ShowWindow(btnRes800, SW_SHOW);
                ShowWindow(btnRes1280, SW_SHOW);
                ShowWindow(btnRes1600, SW_SHOW);
                ShowWindow(btnRes1920, SW_SHOW);
                ShowWindow(btnRes2560, SW_SHOW);

                ShowWindow(btnFullscreen, SW_SHOW);
                ShowWindow(btnWindowed, SW_SHOW);
            }
            break;

        case BTN_OPEN_GAME_PAGE:
            DashboardShow(0);
            DashboardGameShow(1);
            break;

        case BTN_EXIT_APP:
            PostQuitMessage(0);
            break;

        case BTN_SAVE:
            SettingsSave(hwnd);
            break;

        case BTN_RESET:
            SettingsReset(hwnd);
            break;

        case BTN_FULLSCREEN:
            displayMode = 1;
            SendMessage(btnFullscreen, BM_SETCHECK, BST_CHECKED, 0);
            SendMessage(btnWindowed, BM_SETCHECK, BST_UNCHECKED, 0);
            if (hasResolution)
                ApplyFullscreen(hwnd);
            break;

        case BTN_WINDOWED:
            displayMode = 0;
            SendMessage(btnWindowed, BM_SETCHECK, BST_CHECKED, 0);
            SendMessage(btnFullscreen, BM_SETCHECK, BST_UNCHECKED, 0);
            if (hasResolution)
                ApplyWindowed(hwnd, lastW, lastH);
            break;

        // 800x600 : toujours fenêtré
        case BTN_RES_800:
            lastW = 800; lastH = 600; hasResolution = 1;
            ApplyWindowed(hwnd, lastW, lastH);
            break;

        // ≥ 1280x720 : grand écran fenêtré ou plein écran sans bordure selon le mode
        case BTN_RES_1280:
        case BTN_RES_1600:
        case BTN_RES_1920:
        case BTN_RES_2560:
        {
            int w = 1280, h = 720;
            if (LOWORD(wParam) == BTN_RES_1600) { w = 1600; h = 900; }
            if (LOWORD(wParam) == BTN_RES_1920) { w = 1920; h = 1080; }
            if (LOWORD(wParam) == BTN_RES_2560) { w = 2560; h = 1440; }

            lastW = w; lastH = h; hasResolution = 1;

            if (displayMode == 1)
                ApplyFullscreen(hwnd);
            else
                ApplyWindowed(hwnd, lastW, lastH);
        }
        break;
    }
}
