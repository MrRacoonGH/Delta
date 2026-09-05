#define _CRT_SECURE_NO_WARNINGS

#include "musicPage.h"
#include "musicPlayer.h"
#include "playlist.h"
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>

static HWND g_hwnd = NULL;
static int g_visible = 0;
static int g_clientW = 900, g_clientH = 600;

static int g_selectedTrack = -1;
static int g_scrollOffset = 0;
static int g_hoveredTrack = -1;
static int g_backHovered = 0;

static int g_viewPlaylist = -1;
static int g_plTabScroll = 0;
static int g_plTabHover = -1;
static int g_plTabCount = 0;
static int g_plTabIndices[MUSIC_PL_TAB_MAX];

static int g_filteredIndices[512];
static int g_filteredCount = 0;

/* Inline "new playlist" row */
static int g_newplActive = 0;
static int g_newplMode = 0; /* 0=create, 1=rename */
static HWND g_newplEdit = NULL;
static HWND g_newplOk = NULL;
static HWND g_newplCancel = NULL;

/* Transport control rects (custom drawn) for hit testing */
static RECT g_trPrev = {0}, g_trPlay = {0}, g_trNext = {0};

#define ROW_H         30
#define SIDEBAR_W     190
#define SIDEBAR_X     20
#define SIDEBAR_Y     50
#define LIST_X        225
#define LIST_Y        50
#define RIGHT_X_OFF   400

#define BACK_BTN_X  20
#define BACK_BTN_Y  8
#define BACK_BTN_W  110
#define BACK_BTN_H  30

#define CLR_BG        RGB(25, 25, 30)
#define CLR_SIDEBAR   RGB(30, 30, 36)
#define CLR_LIST_BG   RGB(35, 35, 42)
#define CLR_LIST_SEL  RGB(55, 65, 80)
#define CLR_LIST_PLAY RGB(45, 55, 75)
#define CLR_LIST_HVR  RGB(50, 55, 65)
#define CLR_TEXT       RGB(240, 240, 240)
#define CLR_ACCENT     RGB(100, 160, 240)
#define CLR_DIM        RGB(140, 140, 140)
#define CLR_DIVIDER    RGB(60, 60, 68)
#define CLR_PL_ACTIVE  RGB(40, 50, 65)
#define CLR_PL_ADD     RGB(50, 120, 60)
#define CLR_PL_REM     RGB(160, 50, 50)

static void FillRectC(HDC hdc, int x, int y, int w, int h, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c);
    RECT rc = {x, y, x + w, y + h};
    FillRect(hdc, &rc, br);
    DeleteObject(br);
}

static void DrawRoundRectC(HDC hdc, int x, int y, int w, int h, int radius, COLORREF c) {
    HRGN rgn = CreateRoundRectRgn(x, y, x + w, y + h, radius, radius);
    HBRUSH br = CreateSolidBrush(c);
    FillRgn(hdc, rgn, br);
    DeleteObject(br);
    DeleteObject(rgn);
}

static void DrawLine(HDC hdc, int x1, int y, int x2, COLORREF c) {
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ old = SelectObject(hdc, pen);
    MoveToEx(hdc, x1, y, NULL);
    LineTo(hdc, x2, y);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static int ListAreaBottom(void) { return g_clientH - 20; }
static int ListH(void) { int b = ListAreaBottom(); return b > LIST_Y ? b - LIST_Y : 0; }
static int VisRows(void) { int h = ListH(); return h > 0 ? h / ROW_H : 0; }
static int ListW(void) { return g_clientW - RIGHT_X_OFF - LIST_X - 10; }
static int RightPanelX(void) { return g_clientW - RIGHT_X_OFF; }
static int RightPanelW(void) { return RIGHT_X_OFF - 30; }

static void ClampScroll(void) {
    int mx = g_filteredCount - VisRows();
    if (mx < 0) mx = 0;
    if (g_scrollOffset < 0) g_scrollOffset = 0;
    if (g_scrollOffset > mx) g_scrollOffset = mx;
}

static int TrackAtY(int y) {
    int lY = LIST_Y + 26;
    if (y < lY || y >= ListAreaBottom()) return -1;
    int idx = (y - lY) / ROW_H + g_scrollOffset;
    return (idx < g_filteredCount) ? idx : -1;
}

static int GetSourceCount(void) {
    return g_viewPlaylist >= 0 ? PlaylistGet(g_viewPlaylist)->trackCount : MusicPlayerGetTrackCount();
}

static const char *GetSourceFilename(int idx) {
    if (g_viewPlaylist >= 0) {
        Playlist *pl = PlaylistGet(g_viewPlaylist);
        if (pl && idx >= 0 && idx < pl->trackCount) return pl->tracks[idx];
        return NULL;
    }
    /* "Toutes" view: derive the base filename from the music player track path */
    if (idx < 0 || idx >= MusicPlayerGetTrackCount()) return NULL;
    const char *path = MusicPlayerGetTrackPath(idx);
    if (!path) return NULL;
    const char *base = strrchr(path, '\\');
    base = base ? base + 1 : path;
    static char nameBuf[MAX_PL_FILENAME];
    snprintf(nameBuf, sizeof(nameBuf), "%s", base);
    return nameBuf;
}

static int GetMusicTrackIndex(int sourceIdx) {
    if (g_viewPlaylist >= 0) {
        const char *fn = GetSourceFilename(sourceIdx);
        if (!fn) return -1;
        for (int i = 0; i < MusicPlayerGetTrackCount(); i++) {
            const char *path = MusicPlayerGetTrackPath(i);
            if (path) {
                const char *base = strrchr(path, '\\');
                base = base ? base + 1 : path;
                if (_stricmp(base, fn) == 0) return i;
            }
        }
        return -1;
    }
    return sourceIdx;
}

static void RebuildFilter(void) {
    g_filteredCount = 0;
    int total = GetSourceCount();
    for (int i = 0; i < total && g_filteredCount < 512; i++) {
        g_filteredIndices[g_filteredCount++] = i;
    }
    ClampScroll();
}

static void RebuildPlTabs(void) {
    g_plTabCount = PlaylistCount();
    if (g_plTabCount > MUSIC_PL_TAB_MAX) g_plTabCount = MUSIC_PL_TAB_MAX;
    for (int i = 0; i < g_plTabCount; i++) g_plTabIndices[i] = i;
}

static HFONT MakeFont(int size, int bold) {
    return CreateFont(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                      FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
}

static int PlTabStartY(void) { return SIDEBAR_Y + (g_newplActive ? 90 : 56); }

static void NewPlPopupActivate(void) {
    g_newplActive = 1;
    g_newplMode = 0;
    ShowWindow(g_newplEdit, SW_SHOW);
    ShowWindow(g_newplOk, SW_SHOW);
    ShowWindow(g_newplCancel, SW_SHOW);
    SetWindowText(g_newplEdit, "");
    SetFocus(g_newplEdit);
    SendMessage(g_newplEdit, EM_SETSEL, 0, -1);
}

void MusicPageCancelNewPl(void) {
    if (!g_newplActive) return;
    g_newplActive = 0;
    ShowWindow(g_newplEdit, SW_HIDE);
    ShowWindow(g_newplOk, SW_HIDE);
    ShowWindow(g_newplCancel, SW_HIDE);
    if (g_hwnd) {
        SetFocus(g_hwnd);
        InvalidateRect(g_hwnd, NULL, TRUE);
    }
}

void MusicPageResetPlaylists(void) {
    g_viewPlaylist = -1;
    g_selectedTrack = -1;
    RebuildPlTabs();
    RebuildFilter();
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
}

static void NewPlCancel(void) { MusicPageCancelNewPl(); }

static void NewPlCommit(void);

static WNDPROC g_origEditProc = NULL;

static LRESULT CALLBACK NewPlEditProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) { NewPlCommit(); return 0; }
        if (wp == VK_ESCAPE) { NewPlCancel(); return 0; }
    }
    return CallWindowProc(g_origEditProc, hw, msg, wp, lp);
}

static void NewPlCommit(void) {
    char buf[MAX_PL_NAME] = "";
    if (g_newplEdit) GetWindowText(g_newplEdit, buf, sizeof(buf));
    NewPlCancel();
    /* trim */
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t')) buf[--len] = '\0';
    int i = 0;
    while (buf[i] == ' ' || buf[i] == '\t') i++;
    if (i > 0) memmove(buf, buf + i, strlen(buf + i) + 1);
    if (!buf[0]) return;
    if (g_newplMode == 1 && g_viewPlaylist >= 0) {
        PlaylistRename(g_viewPlaylist, buf);
        RebuildPlTabs();
    } else {
        PlaylistCreate(buf);
        RebuildPlTabs();
        g_viewPlaylist = PlaylistCount() - 1;
    }
    RebuildFilter();
    g_selectedTrack = -1;
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
}

void MusicPageInit(HWND hwnd) {
    g_hwnd = hwnd;
    HFONT df = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    int editY = SIDEBAR_Y + 34;
    g_newplEdit = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
        SIDEBAR_X + 4, editY, SIDEBAR_W - 8, 24, hwnd, (HMENU)1,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessage(g_newplEdit, WM_SETFONT, (WPARAM)df, TRUE);
    g_origEditProc = (WNDPROC)SetWindowLongPtr(g_newplEdit, GWLP_WNDPROC, (LONG_PTR)NewPlEditProc);

    g_newplOk = CreateWindow("BUTTON", "OK", WS_CHILD | BS_DEFPUSHBUTTON,
        SIDEBAR_X + 4, editY + 28, 60, 24, hwnd, (HMENU)BTN_MUSIC_NEWPL_OK,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessage(g_newplOk, WM_SETFONT, (WPARAM)df, TRUE);
    g_newplCancel = CreateWindow("BUTTON", "Annuler", WS_CHILD,
        SIDEBAR_X + 68, editY + 28, 90, 24, hwnd, (HMENU)BTN_MUSIC_NEWPL_CANCEL,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessage(g_newplCancel, WM_SETFONT, (WPARAM)df, TRUE);

    PlaylistInit();
    RebuildPlTabs();
    RebuildFilter();
}

void MusicPageShow(int show) {
    g_visible = show;
    if (show) {
        RebuildPlTabs();
        RebuildFilter();
    }
    ShowWindow(g_newplEdit, (show && g_newplActive) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_newplOk, (show && g_newplActive) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_newplCancel, (show && g_newplActive) ? SW_SHOW : SW_HIDE);
    InvalidateRect(g_hwnd, NULL, TRUE);
}

void MusicPageHide(void) {
    ShowWindow(g_newplEdit, SW_HIDE);
    ShowWindow(g_newplOk, SW_HIDE);
    ShowWindow(g_newplCancel, SW_HIDE);
}

static void DrawHScrollGlyph(HDC hdc, int x, int y) {
    HPEN pen = CreatePen(PS_SOLID, 2, CLR_TEXT);
    HGDIOBJ old = SelectObject(hdc, pen);
    MoveToEx(hdc, x, y - 2, NULL);
    LineTo(hdc, x - 8, y + 6);
    LineTo(hdc, x + 8, y + 6);
    LineTo(hdc, x, y - 2);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

void MusicPageDraw(HDC hdc) {
    if (!g_visible) return;

    FillRectC(hdc, 0, 0, g_clientW, g_clientH, CLR_BG);

    int curTrack = MusicPlayerGetCurrentTrack();
    int playing  = MusicPlayerIsPlaying();

    HFONT df = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, df);
    SetBkMode(hdc, TRANSPARENT);

    /* ── Back button ── */
    DrawRoundRectC(hdc, BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, 6,
                   g_backHovered ? RGB(50, 55, 65) : RGB(35, 35, 42));
    SetTextColor(hdc, CLR_TEXT);
    RECT backR = {BACK_BTN_X, BACK_BTN_Y, BACK_BTN_X + BACK_BTN_W, BACK_BTN_Y + BACK_BTN_H};
    DrawTextW(hdc, L"\x2190  Retour", -1, &backR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* ── Title ── */
    SetTextColor(hdc, CLR_ACCENT);
    RECT tR = {0, 10, g_clientW, 42};
    DrawText(hdc, "Musique", -1, &tR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, CLR_DIM);
    char cBuf[64];
    snprintf(cBuf, sizeof(cBuf), "%d piste%s", g_filteredCount,
             g_filteredCount != 1 ? "s" : "");
    RECT cR = {g_clientW - 180, 10, g_clientW - 20, 42};
    DrawText(hdc, cBuf, -1, &cR, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    DrawLine(hdc, 20, 46, g_clientW - 20, CLR_DIVIDER);

    /* ── Left sidebar ── */
    FillRectC(hdc, SIDEBAR_X, SIDEBAR_Y, SIDEBAR_W, ListH(), CLR_SIDEBAR);

    /* "Toutes" */
    int allBg = (g_viewPlaylist < 0) ? CLR_PL_ACTIVE : CLR_SIDEBAR;
    FillRectC(hdc, SIDEBAR_X + 4, SIDEBAR_Y + 2, SIDEBAR_W - 8, 28, allBg);
    SetTextColor(hdc, (g_viewPlaylist < 0) ? CLR_ACCENT : CLR_TEXT);
    RECT allR = {SIDEBAR_X + 12, SIDEBAR_Y + 2, SIDEBAR_X + SIDEBAR_W - 12, SIDEBAR_Y + 30};
    DrawText(hdc, "Toutes les pistes", -1, &allR, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* Inline new-playlist control row placement */
    if (g_newplActive) {
        SetWindowPos(g_newplEdit,   NULL, SIDEBAR_X + 4,  SIDEBAR_Y + 34, SIDEBAR_W - 8, 24, SWP_NOZORDER);
        SetWindowPos(g_newplOk,     NULL, SIDEBAR_X + 4,  SIDEBAR_Y + 62, 60, 24, SWP_NOZORDER);
        SetWindowPos(g_newplCancel, NULL, SIDEBAR_X + 68, SIDEBAR_Y + 62, 90, 24, SWP_NOZORDER);
    }

    /* Playlist header */
    if (!g_newplActive) {
        SetTextColor(hdc, CLR_DIM);
        RECT hdrR = {SIDEBAR_X + 12, PlTabStartY() - 22, SIDEBAR_X + SIDEBAR_W - 10, PlTabStartY() - 2};
        DrawText(hdc, "Playlists", -1, &hdrR, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    /* Playlist tabs */
    int tabH = 30;
    int listBot = ListAreaBottom();
    int tabAreaH = listBot - PlTabStartY() - 40;
    int visTabs = tabAreaH > 0 ? tabAreaH / tabH : 0;

    for (int i = 0; i < visTabs && (g_plTabScroll + i) < g_plTabCount; i++) {
        int ti = g_plTabScroll + i;
        Playlist *pl = PlaylistGet(g_plTabIndices[ti]);
        if (!pl) continue;
        int ty = PlTabStartY() + i * tabH;
        int isActive = (g_viewPlaylist == g_plTabIndices[ti]);
        int isHover  = (ti == g_plTabHover);

        COLORREF bg = CLR_SIDEBAR;
        if (isActive) bg = CLR_PL_ACTIVE;
        else if (isHover) bg = RGB(38, 38, 45);
        FillRectC(hdc, SIDEBAR_X + 4, ty, SIDEBAR_W - 8, tabH - 2, bg);

        if (isActive)
            FillRectC(hdc, SIDEBAR_X + 4, ty, 3, tabH - 2, CLR_ACCENT);

        int thumbSize = 22;
        int thumbX = SIDEBAR_X + 12;
        int thumbY = ty + (tabH - 2 - thumbSize) / 2;
        if (pl->cover[0]) {
            SetTextColor(hdc, CLR_DIM);
            RECT thR = {thumbX, thumbY, thumbX + thumbSize, thumbY + thumbSize};
            DrawText(hdc, "\n~", -1, &thR, DT_CENTER | DT_VCENTER);
        } else {
            FillRectC(hdc, thumbX, thumbY, thumbSize, thumbSize, RGB(50, 50, 58));
            SetTextColor(hdc, CLR_DIM);
            RECT thR = {thumbX, thumbY, thumbX + thumbSize, thumbY + thumbSize};
            DrawText(hdc, "\n~", -1, &thR, DT_CENTER | DT_VCENTER);
        }

        SetTextColor(hdc, isActive ? CLR_ACCENT : CLR_TEXT);
        RECT nR = {thumbX + thumbSize + 6, ty, SIDEBAR_X + SIDEBAR_W - 12, ty + tabH - 2};
        DrawText(hdc, pl->name, -1, &nR, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS | DT_SINGLELINE);
    }

    /* Tab scrollbar */
    if (g_plTabCount > visTabs && visTabs > 0) {
        int barX = SIDEBAR_X + SIDEBAR_W - 8;
        int barH = tabAreaH;
        int thumbH = (visTabs * barH) / g_plTabCount;
        if (thumbH < 16) thumbH = 16;
        int maxS = g_plTabCount - visTabs;
        int thumbY = (maxS > 0) ? (g_plTabScroll * (barH - thumbH) / maxS) : 0;
        FillRectC(hdc, barX, PlTabStartY(), 4, barH, RGB(45, 45, 52));
        FillRectC(hdc, barX, PlTabStartY() + thumbY, 4, thumbH, RGB(80, 80, 90));
    }

    /* Bottom buttons: New / Rename / Delete */
    if (!g_newplActive) {
        int btnY = listBot - 35;
        FillRectC(hdc, SIDEBAR_X + 4, btnY, 50, 26, CLR_PL_ADD);
        SetTextColor(hdc, CLR_TEXT);
        RECT nR = {SIDEBAR_X + 4, btnY, SIDEBAR_X + 54, btnY + 26};
        DrawText(hdc, "+", -1, &nR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if (g_viewPlaylist >= 0) {
            FillRectC(hdc, SIDEBAR_X + 58, btnY, 56, 26, RGB(60, 60, 68));
            RECT rR = {SIDEBAR_X + 58, btnY, SIDEBAR_X + 114, btnY + 26};
            DrawText(hdc, "Renom.", -1, &rR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            FillRectC(hdc, SIDEBAR_X + 118, btnY, 56, 26, CLR_PL_REM);
            RECT dR = {SIDEBAR_X + 118, btnY, SIDEBAR_X + 174, btnY + 26};
            DrawText(hdc, "Suppr.", -1, &dR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    /* ── Track header + list ── */
    int lW = ListW();
    int lBot = ListAreaBottom();
    int lY = LIST_Y + 26;
    int lH2 = lBot - lY;

    SetTextColor(hdc, CLR_DIM);
    RECT hR = {LIST_X, LIST_Y, LIST_X + lW, LIST_Y + 22};
    DrawText(hdc, g_viewPlaylist >= 0 ? "Piste de la playlist" : "Toutes les pistes",
             -1, &hR, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (lW > 0 && lH2 > 0) {
        FillRectC(hdc, LIST_X, lY, lW, lH2, CLR_LIST_BG);
        int vr = VisRows();
        ClampScroll();

        for (int i = 0; i < vr && (g_scrollOffset + i) < g_filteredCount; i++) {
            int fi = g_scrollOffset + i;
            int sourceIdx = g_filteredIndices[fi];
            int mi = GetMusicTrackIndex(sourceIdx);
            int ry = lY + i * ROW_H;
            int isCur = (mi >= 0 && mi == curTrack);
            int isSel = (fi == g_selectedTrack);
            int isHov = (fi == g_hoveredTrack);

            COLORREF bg = CLR_LIST_BG;
            if (isCur && playing) bg = CLR_LIST_PLAY;
            else if (isSel) bg = CLR_LIST_SEL;
            else if (isHov) bg = CLR_LIST_HVR;
            FillRectC(hdc, LIST_X, ry, lW, ROW_H, bg);

            if (isCur && playing)
                FillRectC(hdc, LIST_X, ry, 3, ROW_H, CLR_ACCENT);

            SetTextColor(hdc, CLR_DIM);
            char idxB[8]; snprintf(idxB, sizeof(idxB), "%d", sourceIdx + 1);
            RECT iR = {LIST_X + 6, ry, LIST_X + 38, ry + ROW_H};
            DrawText(hdc, idxB, -1, &iR, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SetTextColor(hdc, isCur ? CLR_ACCENT : CLR_TEXT);
            RECT tR2 = {LIST_X + 40, ry, LIST_X + lW - 190, ry + ROW_H};
            const char *ti = (mi >= 0) ? MusicPlayerGetTrackTitle(mi) : "?";
            DrawText(hdc, ti, -1, &tR2, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS | DT_SINGLELINE);

            SetTextColor(hdc, CLR_DIM);
            RECT aR = {LIST_X + lW - 185, ry, LIST_X + lW - 45, ry + ROW_H};
            const char *ar = (mi >= 0) ? MusicPlayerGetTrackArtist(mi) : "?";
            DrawText(hdc, ar, -1, &aR, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS | DT_SINGLELINE);

            /* "+" add-to-playlist button */
            int bx = LIST_X + lW - 32;
            int addHas = 0;
            if (g_viewPlaylist >= 0) {
                const char *fn = GetSourceFilename(sourceIdx);
                addHas = fn ? PlaylistHasTrack(g_viewPlaylist, fn) : 0;
            }
            COLORREF btnC = (g_viewPlaylist >= 0 && addHas) ? CLR_PL_REM : CLR_PL_ADD;
            FillRectC(hdc, bx, ry + 4, 24, ROW_H - 8, btnC);
            SetTextColor(hdc, CLR_TEXT);
            RECT bR = {bx, ry, bx + 24, ry + ROW_H};
            DrawText(hdc, addHas ? "-" : "+", -1, &bR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        /* Scrollbar */
        if (g_filteredCount > vr) {
            int bx = LIST_X + lW - 5;
            int bH = lH2;
            int tH = (vr * bH) / g_filteredCount;
            if (tH < 20) tH = 20;
            int mx = g_filteredCount - vr;
            int tY = (mx > 0) ? (g_scrollOffset * (bH - tH) / mx) : 0;
            FillRectC(hdc, bx, lY, 4, bH, RGB(45, 45, 52));
            FillRectC(hdc, bx, lY + tY, 4, tH, RGB(90, 90, 100));
        }
    }

    /* ── Right panel: Now Playing ── */
    int rpX = RightPanelX();
    int rpW = RightPanelW();
    int rpY = LIST_Y;
    int rpH = ListH();

    if (rpW > 0 && rpH > 0) {
        int rpCX = rpX + rpW / 2;

        if (MusicPlayerGetTrackCount() == 0) {
            SetTextColor(hdc, CLR_DIM);
            RECT eR = {rpX, rpY + rpH/2 - 30, rpX + rpW, rpY + rpH/2 + 10};
            DrawText(hdc, "Aucun fichier audio", -1, &eR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            FillRectC(hdc, rpCX - 50, rpY + 20, 100, 100, RGB(40, 40, 48));
            SetTextColor(hdc, CLR_DIM);
            RECT icR = {rpCX - 50, rpY + 20, rpCX + 50, rpY + 120};
            DrawText(hdc, "\n~", -1, &icR, DT_CENTER | DT_VCENTER);

            HFONT bigF = MakeFont(26, 1);
            SelectObject(hdc, bigF);
            SetTextColor(hdc, CLR_TEXT);
            RECT nR = {rpX + 10, rpY + 130, rpX + rpW - 10, rpY + 160};
            const char *name = (curTrack >= 0) ? MusicPlayerGetTrackTitle(curTrack) : "---";
            DrawText(hdc, name, -1, &nR, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_SINGLELINE);
            DeleteObject(bigF);

            HFONT medF = MakeFont(18, 0);
            SelectObject(hdc, medF);
            SetTextColor(hdc, CLR_DIM);
            RECT aR = {rpX + 10, rpY + 163, rpX + rpW - 10, rpY + 188};
            const char *art = (curTrack >= 0) ? MusicPlayerGetTrackArtist(curTrack) : "---";
            DrawText(hdc, art, -1, &aR, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_SINGLELINE);
            DeleteObject(medF);

            /* Transport controls (custom drawn) */
            int btnY2 = rpY + rpH - 90;
            int bW = 80, bH = 40, gap = 20;
            int totBW = bW * 3 + gap * 2;
            int bx0 = rpCX - totBW / 2;

            g_trPrev = (RECT){bx0, btnY2, bx0 + bW, btnY2 + bH};
            g_trPlay = (RECT){bx0 + bW + gap, btnY2, bx0 + bW + gap + bW, btnY2 + bH};
            g_trNext = (RECT){bx0 + 2*(bW+gap), btnY2, bx0 + 2*(bW+gap) + bW, btnY2 + bH};

            DrawRoundRectC(hdc, g_trPrev.left,  g_trPrev.top,  bW, bH, 8, RGB(40, 40, 48));
            DrawRoundRectC(hdc, g_trPlay.left,  g_trPlay.top,  bW, bH, 8, RGB(50, 60, 75));
            DrawRoundRectC(hdc, g_trNext.left,  g_trNext.top,  bW, bH, 8, RGB(40, 40, 48));

            SetTextColor(hdc, CLR_TEXT);
            RECT pR = {g_trPrev.left, btnY2, g_trPrev.right, btnY2 + bH};
            DrawTextW(hdc, L"\x25C0", -1, &pR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            RECT pR2 = {g_trPlay.left, btnY2, g_trPlay.right, btnY2 + bH};
            DrawTextW(hdc, playing ? L"\x23F8" : L"\x25B6", -1, &pR2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            RECT nR2 = {g_trNext.left, btnY2, g_trNext.right, btnY2 + bH};
            DrawTextW(hdc, L"\x25B6\x25B6", -1, &nR2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            RECT prR = {rpCX - 120, rpY + rpH - 32, rpCX + 120, rpY + rpH - 8};
            DrawLine(hdc, prR.left, rpY + rpH - 20, prR.right, RGB(60, 60, 68));
        }
    }

    SelectObject(hdc, oldFont);
}

int MusicPageHandleCommand(HWND hwnd, WPARAM wParam) {
    int id = LOWORD(wParam);

    if (id == BTN_MUSIC_BACK) {
        MusicPageShow(0);
        MusicPlayerShow(0);
        return MUSIC_RET_BACK;
    }

    if (id == BTN_MUSIC_NEWPL_OK) {
        NewPlCommit();
        return MUSIC_RET_NONE;
    }
    if (id == BTN_MUSIC_NEWPL_CANCEL) {
        NewPlCancel();
        return MUSIC_RET_NONE;
    }

    /* Add-to-playlist popup items (id == MUSIC_ADD_MENU_BASE + plIdx) */
    if (id >= MUSIC_ADD_MENU_BASE && id < MUSIC_ADD_MENU_BASE + MAX_PLAYLISTS) {
        int plIdx = id - MUSIC_ADD_MENU_BASE;
        if (g_selectedTrack >= 0 && g_selectedTrack < g_filteredCount) {
            int sourceIdx = g_filteredIndices[g_selectedTrack];
            const char *fn = GetSourceFilename(sourceIdx);
            if (fn) {
                if (PlaylistHasTrack(plIdx, fn))
                    PlaylistRemoveTrack(plIdx, PlaylistTrackIndex(plIdx, fn));
                else
                    PlaylistAddTrack(plIdx, fn);
                RebuildFilter();
                if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
            }
        }
        return MUSIC_RET_NONE;
    }
    if (id == MUSIC_ADD_MENU_NEW) {
        NewPlPopupActivate();
        return MUSIC_RET_NONE;
    }

    /* Legacy right-click context menu items (id == MUSIC_MENU_ADDTOTRACK + plIdx) */
    if (id >= MUSIC_MENU_ADDTOTRACK && id < MUSIC_MENU_ADDTOTRACK + MAX_PLAYLISTS) {
        int plIdx = id - MUSIC_MENU_ADDTOTRACK;
        if (g_selectedTrack >= 0 && g_selectedTrack < g_filteredCount) {
            int sourceIdx = g_filteredIndices[g_selectedTrack];
            const char *fn = GetSourceFilename(sourceIdx);
            if (fn) {
                if (PlaylistHasTrack(plIdx, fn))
                    PlaylistRemoveTrack(plIdx, PlaylistTrackIndex(plIdx, fn));
                else
                    PlaylistAddTrack(plIdx, fn);
                RebuildFilter();
                if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
            }
        }
        return MUSIC_RET_NONE;
    }

    if (id == BTN_MUSIC_ALL) {
        g_viewPlaylist = -1;
        g_selectedTrack = -1;
        RebuildFilter();
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
        return MUSIC_RET_NONE;
    }

    if (id >= MUSIC_PL_TAB_BASE && id < MUSIC_PL_TAB_BASE + MUSIC_PL_TAB_MAX) {
        int tabIdx = id - MUSIC_PL_TAB_BASE;
        if (tabIdx < g_plTabCount) {
            g_viewPlaylist = g_plTabIndices[tabIdx];
            g_selectedTrack = -1;
            RebuildFilter();
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
        }
        return MUSIC_RET_NONE;
    }

    if (id == BTN_MUSIC_NEWPL) {
        NewPlPopupActivate();
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
        return MUSIC_RET_NONE;
    }

    if (id == BTN_MUSIC_RENAME && g_viewPlaylist >= 0) {
        Playlist *pl = PlaylistGet(g_viewPlaylist);
        if (!pl) return MUSIC_RET_NONE;
        g_newplActive = 1;
        g_newplMode = 1;
        ShowWindow(g_newplEdit, SW_SHOW);
        ShowWindow(g_newplOk, SW_SHOW);
        ShowWindow(g_newplCancel, SW_SHOW);
        SetWindowText(g_newplEdit, pl->name);
        SendMessage(g_newplEdit, EM_SETSEL, 0, -1);
        SetFocus(g_newplEdit);
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
        return MUSIC_RET_NONE;
    }

    if (id == BTN_MUSIC_DELETE && g_viewPlaylist >= 0) {
        PlaylistDelete(g_viewPlaylist);
        g_viewPlaylist = -1;
        RebuildPlTabs();
        RebuildFilter();
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
        return MUSIC_RET_NONE;
    }

    return MUSIC_RET_NONE;
}

static void ToggleTrackInPlaylist(int sourceIdx) {
    if (g_viewPlaylist < 0) return;
    const char *fn = GetSourceFilename(sourceIdx);
    if (!fn) return;
    if (PlaylistHasTrack(g_viewPlaylist, fn))
        PlaylistRemoveTrack(g_viewPlaylist, PlaylistTrackIndex(g_viewPlaylist, fn));
    else
        PlaylistAddTrack(g_viewPlaylist, fn);
}

static void ShowAddMenu(HWND hwnd, int x, int y, int sourceIdx) {
    const char *fn = GetSourceFilename(sourceIdx);
    if (!fn) return;

    HMENU hMenu = CreatePopupMenu();
    int pls = PlaylistCount();
    if (pls > 0) {
        for (int i = 0; i < pls; i++) {
            Playlist *pl = PlaylistGet(i);
            if (!pl) continue;
            int has = PlaylistHasTrack(i, fn);
            char label[160];
            snprintf(label, sizeof(label), "%s %s", has ? "* " : "  ", pl->name);
            AppendMenu(hMenu, MF_STRING, MUSIC_ADD_MENU_BASE + i, label);
        }
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    } else {
        AppendMenu(hMenu, MF_STRING | MF_GRAYED, MUSIC_ADD_MENU_BASE, "Aucune playlist");
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    }
    AppendMenu(hMenu, MF_STRING, MUSIC_ADD_MENU_NEW, "+ Nouvelle playlist");

    /* We need to add/remove for this track; no global selection needed */

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, hwnd, NULL);
    DestroyMenu(hMenu);

    if (cmd >= MUSIC_ADD_MENU_BASE && cmd < MUSIC_ADD_MENU_BASE + MAX_PLAYLISTS) {
        int plIdx = cmd - MUSIC_ADD_MENU_BASE;
        const char *f = GetSourceFilename(sourceIdx);
        if (f) {
            if (PlaylistHasTrack(plIdx, f))
                PlaylistRemoveTrack(plIdx, PlaylistTrackIndex(plIdx, f));
            else
                PlaylistAddTrack(plIdx, f);
            RebuildFilter();
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
        }
    } else if (cmd == MUSIC_ADD_MENU_NEW) {
        NewPlPopupActivate();
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
    }
}

void MusicPageOnLButtonDown(int x, int y) {
    if (!g_visible) return;

    /* Back button */
    if (x >= BACK_BTN_X && x <= BACK_BTN_X + BACK_BTN_W &&
        y >= BACK_BTN_Y && y <= BACK_BTN_Y + BACK_BTN_H) {
        SendMessage(g_hwnd, WM_COMMAND, MAKEWPARAM(BTN_MUSIC_BACK, 0), 0);
        return;
    }

    /* Transport controls */
    POINT ptMouse = {x, y};
    if (PtInRect(&g_trPrev, ptMouse)) { MusicPlayerHandleCommand(g_hwnd, MAKEWPARAM(BTN_MUSIC_PREV, 0)); return; }
    if (PtInRect(&g_trPlay, ptMouse)) { MusicPlayerHandleCommand(g_hwnd, MAKEWPARAM(BTN_MUSIC_PLAY, 0)); return; }
    if (PtInRect(&g_trNext, ptMouse)) { MusicPlayerHandleCommand(g_hwnd, MAKEWPARAM(BTN_MUSIC_NEXT, 0)); return; }

    /* Sidebar: "Toutes" */
    if (x >= SIDEBAR_X + 4 && x <= SIDEBAR_X + SIDEBAR_W - 4 &&
        y >= SIDEBAR_Y + 2 && y <= SIDEBAR_Y + 30) {
        g_viewPlaylist = -1;
        g_selectedTrack = -1;
        RebuildFilter();
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
        return;
    }

    /* Sidebar: playlist tabs */
    int tabH = 30;
    if (x >= SIDEBAR_X + 4 && x <= SIDEBAR_X + SIDEBAR_W - 4 &&
        y >= PlTabStartY() && y < ListAreaBottom() - 40) {
        int ti = (y - PlTabStartY()) / tabH + g_plTabScroll;
        if (ti >= 0 && ti < g_plTabCount) {
            g_viewPlaylist = g_plTabIndices[ti];
            g_selectedTrack = -1;
            RebuildFilter();
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
        }
        return;
    }

    /* Sidebar bottom buttons */
    int btnY = ListAreaBottom() - 35;
    if (y >= btnY && y <= btnY + 26 && x >= SIDEBAR_X + 4) {
        if (x <= SIDEBAR_X + 54) {
            SendMessage(g_hwnd, WM_COMMAND, MAKEWPARAM(BTN_MUSIC_NEWPL, 0), 0);
            return;
        }
        if (g_viewPlaylist >= 0) {
            if (x <= SIDEBAR_X + 114) {
                SendMessage(g_hwnd, WM_COMMAND, MAKEWPARAM(BTN_MUSIC_RENAME, 0), 0);
                return;
            }
            if (x <= SIDEBAR_X + 174) {
                SendMessage(g_hwnd, WM_COMMAND, MAKEWPARAM(BTN_MUSIC_DELETE, 0), 0);
                return;
            }
        }
    }

    /* Track list */
    if (x >= LIST_X && x <= LIST_X + ListW() && y >= LIST_Y + 30 && y < ListAreaBottom()) {
        int lW = ListW();
        int bx = LIST_X + lW - 32;
        if (x >= bx && x <= bx + 24) {
            int idx = TrackAtY(y);
            if (idx >= 0 && idx < g_filteredCount) {
                int sourceIdx = g_filteredIndices[idx];
                if (g_viewPlaylist >= 0) {
                    ToggleTrackInPlaylist(sourceIdx);
                    RebuildFilter();
                    if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
                } else {
                    POINT pt = {x, y};
                    ClientToScreen(g_hwnd, &pt);
                    ShowAddMenu(g_hwnd, pt.x, pt.y, sourceIdx);
                }
                return;
            }
        }
        int idx = TrackAtY(y);
        if (idx >= 0 && idx < g_filteredCount) {
            int sourceIdx = g_filteredIndices[idx];
            g_selectedTrack = idx;
            int mi = GetMusicTrackIndex(sourceIdx);
            if (mi >= 0) MusicPlayerPlayTrack(mi);
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
        }
    }
}

void MusicPageShowContextMenu(HWND hwnd, int x, int y, int trackIndex) {
    if (trackIndex < 0 || trackIndex >= g_filteredCount) return;
    int sourceIdx = g_filteredIndices[trackIndex];
    ShowAddMenu(hwnd, x, y, sourceIdx);
}

void MusicPageOnRButtonDown(int x, int y) {
    if (!g_visible) return;

    int tabH = 30;
    if (x >= SIDEBAR_X + 4 && x <= SIDEBAR_X + SIDEBAR_W - 4 &&
        y >= PlTabStartY() && y < ListAreaBottom() - 40) {
        int ti = (y - PlTabStartY()) / tabH + g_plTabScroll;
        if (ti >= 0 && ti < g_plTabCount) {
            int plIdx = g_plTabIndices[ti];
            POINT pt = {x, y};
            ClientToScreen(g_hwnd, &pt);

            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, 7000, "Changer la photo");
            AppendMenu(hMenu, MF_STRING, 7001, "Renommer");
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, 7002, "Supprimer");

            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                     pt.x, pt.y, 0, g_hwnd, NULL);
            DestroyMenu(hMenu);

            switch (cmd) {
                case 7000: MusicPageSetCover(plIdx); break;
                case 7001: SendMessage(g_hwnd, WM_COMMAND, MAKEWPARAM(BTN_MUSIC_RENAME, 0), 0); break;
                case 7002: SendMessage(g_hwnd, WM_COMMAND, MAKEWPARAM(BTN_MUSIC_DELETE, 0), 0); break;
            }
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
        }
        return;
    }

    if (x >= LIST_X && x <= LIST_X + ListW() && y >= LIST_Y + 30 && y < ListAreaBottom()) {
        int idx = TrackAtY(y);
        if (idx >= 0) {
            g_selectedTrack = idx;
            POINT pt = {x, y};
            ClientToScreen(g_hwnd, &pt);
            MusicPageShowContextMenu(g_hwnd, pt.x, pt.y, idx);
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
        }
    }
}

void MusicPageSetCover(int plIndex) {
    char file[1024] = "";
    OPENFILENAME ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = "Images (*.jpg;*.jpeg;*.png;*.bmp;*.gif)\0*.jpg;*.jpeg;*.png;*.bmp;*.gif\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileName(&ofn)) {
        PlaylistSetCover(plIndex, file);
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
    }
}

void MusicPageOnMouseWheel(int delta) {
    if (!g_visible) return;
    int lines = delta / WHEEL_DELTA;
    g_scrollOffset -= lines * 3;
    ClampScroll();
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
}

void MusicPageOnMouseMove(int x, int y) {
    if (!g_visible) return;
    int wasHover = g_backHovered;
    g_backHovered = (x >= BACK_BTN_X && x <= BACK_BTN_X + BACK_BTN_W &&
                     y >= BACK_BTN_Y && y <= BACK_BTN_Y + BACK_BTN_H);
    if (wasHover != g_backHovered && g_hwnd)
        InvalidateRect(g_hwnd, NULL, TRUE);
}

int MusicPageHandleKey(WPARAM wParam) {
    if (!g_visible || !g_newplActive) return 0;
    if (wParam == VK_RETURN) {
        NewPlCommit();
        return 1;
    }
    if (wParam == VK_ESCAPE) {
        NewPlCancel();
        return 1;
    }
    return 0;
}

void MusicPageOnTimer(void) {
    if (!g_visible) return;
    if (MusicPlayerIsPlaying() && g_hwnd)
        InvalidateRect(g_hwnd, NULL, TRUE);
}
