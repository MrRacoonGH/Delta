#define _CRT_SECURE_NO_WARNINGS

#include "statsPanel.h"
#include "dashboardGame.h"
#include "musicPlayer.h"
#include "dashboard.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define TOP_N 3

typedef struct {
    char name[128];
    int  value;
    char display[64];
} StatEntry;

static StatEntry topGames[TOP_N];
static int       topGameCount = 0;

static StatEntry topMusic[TOP_N];
static int       topMusicCount = 0;

static void BuildTopGames(void) {
    int count = GameCount();
    topGameCount = 0;

    for (int i = 0; i < count && topGameCount < TOP_N; i++) {
        int pt = GameGetPlaytime(i);
        if (pt <= 0) continue;

        int pos = topGameCount;
        while (pos > 0 && pt > topGames[pos - 1].value) {
            topGames[pos] = topGames[pos - 1];
            pos--;
        }

        strncpy(topGames[pos].name, GameGetName(i), sizeof(topGames[pos].name) - 1);
        topGames[pos].name[sizeof(topGames[pos].name) - 1] = '\0';
        topGames[pos].value = pt;

        if (pt >= 60)
            snprintf(topGames[pos].display, sizeof(topGames[pos].display),
                     "%dh %02dmin", pt / 60, pt % 60);
        else
            snprintf(topGames[pos].display, sizeof(topGames[pos].display),
                     "%d min", pt);

        if (pos == topGameCount) topGameCount++;
    }
}

static void BuildTopMusic(void) {
    int count = MusicPlayerGetTrackCount();
    topMusicCount = 0;

    for (int i = 0; i < count && topMusicCount < TOP_N; i++) {
        int pc = MusicPlayerGetPlayCount(i);
        if (pc <= 0) continue;

        int pos = topMusicCount;
        while (pos > 0 && pc > topMusic[pos - 1].value) {
            topMusic[pos] = topMusic[pos - 1];
            pos--;
        }

        const char *title = MusicPlayerGetTrackTitle(i);
        strncpy(topMusic[pos].name, title, sizeof(topMusic[pos].name) - 1);
        topMusic[pos].name[sizeof(topMusic[pos].name) - 1] = '\0';
        topMusic[pos].value = pc;
        snprintf(topMusic[pos].display, sizeof(topMusic[pos].display),
                 "%d ecoutes", pc);

        if (pos == topMusicCount) topMusicCount++;
    }
}

void StatsPanelRefresh(void) {
    BuildTopGames();
    BuildTopMusic();
}

static void DrawSectionTitle(HDC hdc, int x, int y, int w, const char *title) {
    RECT rc = { x, y, x + w, y + 16 };
    int dark = BackgroundIsDark();
    SetTextColor(hdc, dark ? RGB(150, 200, 255) : RGB(10, 10, 10));
    HFONT hFont = CreateFont(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, hFont);
    DrawText(hdc, title, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
    DeleteObject(hFont);
}

static void DrawEntry(HDC hdc, int x, int y, int w,
                       int rank, const char *name, const char *value) {
    char rankStr[8];
    snprintf(rankStr, sizeof(rankStr), "%d.", rank);
    int dark = BackgroundIsDark();
    RECT rankRc = { x, y, x + 18, y + 14 };
    SetTextColor(hdc, dark ? RGB(150, 200, 255) : RGB(10, 10, 10));
    HFONT hBold = CreateFont(11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, hBold);
    DrawText(hdc, rankStr, -1, &rankRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT nameRc = { x + 18, y, x + w - 62, y + 14 };
    SetTextColor(hdc, dark ? RGB(255, 255, 255) : RGB(0, 0, 0));
    HFONT hReg = CreateFont(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    SelectObject(hdc, hReg);
    DrawText(hdc, name, -1, &nameRc, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

    RECT valRc = { x + w - 62, y, x + w, y + 14 };
    SetTextColor(hdc, dark ? RGB(180, 180, 180) : RGB(80, 80, 80));
    DrawText(hdc, value, -1, &valRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(hBold);
    DeleteObject(hReg);
}

void StatsPanelDraw(HDC hdc, int width, int height) {
    int panelW = width / 10;
    if (panelW < 160) panelW = 160;
    int panelH = height / 10;
    if (panelH < 180) panelH = 180;

    int panelX = width - panelW - 16;
    int panelY = 16;

    int dark = BackgroundIsDark();
    COLORREF panelBg = dark ? RGB(20, 20, 20) : RGB(245, 245, 245);
    COLORREF panelBorder = dark ? RGB(255, 255, 255) : RGB(0, 0, 0);
    COLORREF titleColor = dark ? RGB(255, 255, 255) : RGB(0, 0, 0);
    COLORREF dimColor = dark ? RGB(150, 150, 150) : RGB(90, 90, 90);

    /* Panel background */
    HBRUSH bgBrush = CreateSolidBrush(panelBg);
    HPEN bgPen = CreatePen(PS_SOLID, 1, panelBorder);
    HGDIOBJ oldBg = SelectObject(hdc, bgBrush);
    HGDIOBJ oldPen = SelectObject(hdc, bgPen);
    RoundRect(hdc, panelX, panelY, panelX + panelW, panelY + panelH, 8, 8);
    SelectObject(hdc, oldBg);
    SelectObject(hdc, oldPen);
    DeleteObject(bgPen);
    DeleteObject(bgBrush);

    int margin = 10;
    int x0 = panelX + margin;
    int x1 = panelX + panelW - margin;

    /* Title */
    RECT titleRc = { x0, panelY + 6, x1, panelY + 22 };
    SetTextColor(hdc, titleColor);
    HFONT hTitle = CreateFont(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, hTitle);
    DrawText(hdc, "Statistiques", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
    DeleteObject(hTitle);

    /* Separator */
    HPEN linePen = CreatePen(PS_SOLID, 1, panelBorder);
    SelectObject(hdc, linePen);
    MoveToEx(hdc, x0, panelY + 26, NULL);
    LineTo(hdc, x1, panelY + 26);
    SelectObject(hdc, GetStockObject(BLACK_PEN));
    DeleteObject(linePen);

    int curY = panelY + 32;
    int avail = panelY + panelH - 8 - curY;
    int half = avail / 2;

    /* --- JEUX --- */
    int gameY = curY;
    DrawSectionTitle(hdc, x0, gameY, panelW - 2 * margin, "Jeux");
    gameY += 18;

    if (topGameCount == 0) {
        RECT emptyRc = { x0 + 18, gameY, x1, gameY + 14 };
        SetTextColor(hdc, dimColor);
        HFONT hDim = CreateFont(10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        SelectObject(hdc, hDim);
        DrawText(hdc, "Aucune donnee", -1, &emptyRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        DeleteObject(hDim);
    } else {
        for (int i = 0; i < topGameCount; i++) {
            DrawEntry(hdc, x0 + 16, gameY, panelW - 2 * margin - 16,
                      i + 1, topGames[i].name, topGames[i].display);
            gameY += 15;
        }
    }

    /* --- MUSIQUE --- */
    int musicY = curY + half;
    DrawSectionTitle(hdc, x0, musicY, panelW - 2 * margin, "Musique");
    musicY += 18;

    if (topMusicCount == 0) {
        RECT emptyRc = { x0 + 18, musicY, x1, musicY + 14 };
        SetTextColor(hdc, dimColor);
        HFONT hDim = CreateFont(10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        SelectObject(hdc, hDim);
        DrawText(hdc, "Aucune donnee", -1, &emptyRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        DeleteObject(hDim);
    } else {
        for (int i = 0; i < topMusicCount; i++) {
            DrawEntry(hdc, x0 + 16, musicY, panelW - 2 * margin - 16,
                      i + 1, topMusic[i].name, topMusic[i].display);
            musicY += 15;
        }
    }
}
