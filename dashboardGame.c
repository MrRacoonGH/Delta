#define _CRT_SECURE_NO_WARNINGS

#include "dashboardGame.h"
#include "dashboard.h"
#include "jeux.h"
#include "steamScanner.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static GameEntry games[MAX_GAMES];
static int gameCount = 0;

static HICON gameIconHandles[MAX_GAMES];

static HWND btnBack;
static HWND hwndParent;
static int pageVisible = 0;
static int scrollOffset = 0;
static int clientW = 900, clientH = 600;

static void JsonUnescape(char *str) {
    char *src = str;
    char *dst = str;
    while (*src) {
        if (*src == '\\' && (src[1] == '\\' || src[1] == '"')) {
            *dst++ = src[1];
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static void JsonEscape(const char *in, char *out, size_t outSize) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o < outSize - 2; i++) {
        char c = in[i];
        if (c == '\\' || c == '"') {
            if (o + 1 < outSize - 1) out[o++] = '\\';
        }
        out[o++] = c;
    }
    out[o] = '\0';
}

void LoadGamesFromJSON()
{
    /* Scan Steam library and rewrite games.json to reflect current games */
    SteamScanAll();
    SteamMergeIntoJson();

    char jsonPath[MAX_PATH];
    GetModuleFileName(NULL, jsonPath, MAX_PATH);

    char *slash = strrchr(jsonPath, '\\');
    if (!slash)
        return;
    slash[1] = '\0';
    strcat(jsonPath, "games.json");

    FILE *f = fopen(jsonPath, "r");
    if (!f)
        return;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return; }

    char *buffer = (char*)malloc((size_t)fsize + 1);
    if (!buffer) { fclose(f); return; }

    size_t read = fread(buffer, 1, (size_t)fsize, f);
    buffer[read] = '\0';
    fclose(f);

    gameCount = 0;
    char *ptr = buffer;

    while ((ptr = strstr(ptr, "\"name\"")) != NULL && gameCount < MAX_GAMES)
    {
        // NAME
        ptr = strchr(ptr, ':');
        ptr = strchr(ptr, '"') + 1;
        char *end = strchr(ptr, '"');
        {
            size_t len = (size_t)(end - ptr);
            if (len >= sizeof(games[gameCount].name)) len = sizeof(games[gameCount].name) - 1;
            memcpy(games[gameCount].name, ptr, len);
            games[gameCount].name[len] = '\0';
            JsonUnescape(games[gameCount].name);
        }

        // PATH
        ptr = strstr(end, "\"path\"");
        ptr = strchr(ptr, ':');
        ptr = strchr(ptr, '"') + 1;
        end = strchr(ptr, '"');
        {
            size_t len = (size_t)(end - ptr);
            if (len >= sizeof(games[gameCount].path)) len = sizeof(games[gameCount].path) - 1;
            memcpy(games[gameCount].path, ptr, len);
            games[gameCount].path[len] = '\0';
            JsonUnescape(games[gameCount].path);
        }

        // ICON
        games[gameCount].icon[0] = '\0';
        char *iconStart = strstr(end, "\"icon\"");
        if (iconStart && iconStart < buffer + read)
        {
            char *iv = strchr(iconStart, ':');
            char *io = iv ? strchr(iv, '"') : NULL;
            char *ic = io ? strchr(io + 1, '"') : NULL;
            if (io && ic) {
                size_t len = (size_t)(ic - io - 1);
                if (len >= sizeof(games[gameCount].icon)) len = sizeof(games[gameCount].icon) - 1;
                memcpy(games[gameCount].icon, io + 1, len);
                games[gameCount].icon[len] = '\0';
                JsonUnescape(games[gameCount].icon);
                end = ic;
            }
        }

        // BANNER (optional)
        char *bannerStart = strstr(end, "\"banner\"");
        strcpy(games[gameCount].banner, "");
        if (bannerStart && bannerStart < buffer + read)
        {
            char *bVal = strchr(bannerStart, ':');
            char *bOpen = bVal ? strchr(bVal, '"') : NULL;
            char *bClose = bOpen ? strchr(bOpen + 1, '"') : NULL;
            if (bOpen && bClose) {
                size_t len = (size_t)(bClose - bOpen - 1);
                if (len >= sizeof(games[gameCount].banner)) len = sizeof(games[gameCount].banner) - 1;
                memcpy(games[gameCount].banner, bOpen + 1, len);
                games[gameCount].banner[len] = '\0';
                JsonUnescape(games[gameCount].banner);
            }
        }

        // PLAYTIME (optional, minutes)
        games[gameCount].playtime = 0;
        char *playStart = strstr(end, "\"playtime\"");
        if (playStart && playStart < buffer + read)
        {
            char *pVal = strchr(playStart, ':');
            if (pVal) games[gameCount].playtime = atoi(pVal + 1);
        }

        gameCount++;
    }

    free(buffer);
}

/* Layout geometry for a card index */
static void CardRect(int index, RECT *rc) {
    int gap = 20;
    int x = 20 - scrollOffset + index * (CARD_W + gap);
    rc->left = x;
    rc->top = 70;
    rc->right = x + CARD_W;
    rc->bottom = 70 + CARD_H;
}

static int TotalWidth(void) {
    if (gameCount == 0) return 0;
    return gameCount * (CARD_W + 20) + 20;
}

int DashboardGameIsVisible(void) { return pageVisible; }

int DashboardGameIndexAt(int x, int y) {
    if (!pageVisible) return -1;
    for (int i = 0; i < gameCount; i++) {
        RECT rc;
        CardRect(i, &rc);
        if (x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom)
            return i;
    }
    return -1;
}

const char *GameGetPath(int index) {
    if (index >= 0 && index < gameCount) return games[index].path;
    return NULL;
}

int GameCount(void) { return gameCount; }

const char *GameGetName(int index) {
    if (index >= 0 && index < gameCount) return games[index].name;
    return "";
}

const char *GameGetBanner(int index) {
    if (index >= 0 && index < gameCount) return games[index].banner;
    return "";
}

const char *GameGetIcon(int index) {
    if (index >= 0 && index < gameCount) return games[index].icon;
    return "";
}

int GameGetPlaytime(int index) {
    if (index >= 0 && index < gameCount) return games[index].playtime;
    return 0;
}

void GameSetPlaytime(int index, int minutes) {
    if (index >= 0 && index < gameCount) games[index].playtime = minutes;
}

void FormatPlaytime(int minutes, char *out, size_t outSize) {
    if (minutes >= 60) {
        int h = minutes / 60;
        int m = minutes % 60;
        snprintf(out, outSize, "Temps de jeu : %d h %02d min", h, m);
    } else {
        snprintf(out, outSize, "Temps de jeu : %d min", minutes);
    }
}

/* Write the current in-memory game list back to games.json */
void SaveGamesJson(void) {
    char jsonPath[MAX_PATH];
    GetModuleFileName(NULL, jsonPath, MAX_PATH);
    char *slash = strrchr(jsonPath, '\\');
    if (!slash) return;
    slash[1] = '\0';
    strcat(jsonPath, "games.json");

    FILE *f = fopen(jsonPath, "w");
    if (!f) return;

    fprintf(f, "{\n  \"games\": [\n");
    for (int i = 0; i < gameCount; i++) {
        char en[512*2], ep[600*2], ei[600*2], eb[600*2];
        JsonEscape(games[i].name, en, sizeof(en));
        JsonEscape(games[i].path, ep, sizeof(ep));
        JsonEscape(games[i].icon, ei, sizeof(ei));
        JsonEscape(games[i].banner, eb, sizeof(eb));
        if (i > 0) fprintf(f, ",\n");
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", en);
        fprintf(f, "      \"path\": \"%s\",\n", ep);
        if (games[i].icon[0]) fprintf(f, "      \"icon\": \"%s\",\n", ei);
        if (games[i].banner[0]) fprintf(f, "      \"banner\": \"%s\",\n", eb);
        fprintf(f, "      \"playtime\": %d\n", games[i].playtime);
        fprintf(f, "    }");
    }
    fprintf(f, "\n  ]\n}\n");
    fclose(f);
}

void DashboardGameWheel(HWND hwnd, int wheelDelta) {
    int total = TotalWidth();
    if (total <= clientW) return;

    int maxOffset = total - clientW;
    if (maxOffset < 0) maxOffset = 0;

    scrollOffset -= wheelDelta;  /* wheel up -> scroll left */
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset > maxOffset) scrollOffset = maxOffset;

    InvalidateRect(hwnd, NULL, TRUE);
}

/* Returns 1 if a card was clicked (opens detail) */
int DashboardGameHitTest(HWND hwnd, int x, int y) {
    (void)hwnd;
    if (!pageVisible) return 0;

    for (int i = 0; i < gameCount; i++) {
        RECT rc;
        CardRect(i, &rc);
        if (x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom)
            return 1;
    }
    return 0;
}

void DashboardGameHandleCommand(HWND hwnd, WPARAM wParam)
{
    int id = LOWORD(wParam);

    // Lancer jeu
    if (id >= BTN_GAME_LAUNCH_BASE && id < BTN_GAME_LAUNCH_BASE + gameCount)
    {
        int index = id - BTN_GAME_LAUNCH_BASE;
        LancerJeu(games[index].path);
        return;
    }

    // Ouvrir dossier
    if (id >= BTN_GAME_FOLDER_BASE && id < BTN_GAME_FOLDER_BASE + gameCount)
    {
        int index = id - BTN_GAME_FOLDER_BASE;

        char folder[260];
        strcpy(folder, games[index].path);

        for (int i = (int)strlen(folder) - 1; i >= 0; i--)
        {
            if (folder[i] == '\\' || folder[i] == '/')
            {
                folder[i] = '\0';
                break;
            }
        }

        ShellExecute(NULL, "open", folder, NULL, NULL, SW_SHOW);
        return;
    }

    // Retour
    if (id == BTN_BACK)
    {
        DashboardGameShow(0);
        DashboardShow(1);
        return;
    }
}

void DashboardGameDraw(HDC hdc)
{
    if (!pageVisible) return;

    RECT clip = {0, 0, clientW, clientH};
    HRGN rgn = CreateRectRgn(0, 0, clientW, clientH);
    SelectClipRgn(hdc, rgn);

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

    for (int i = 0; i < gameCount; i++) {
        RECT rc;
        CardRect(i, &rc);

        /* Skip cards fully outside */
        if (rc.right < 0 || rc.left > clientW) continue;

        /* card background */
        HBRUSH cardBrush = CreateSolidBrush(RGB(40, 40, 40));
        HPEN cardPen = CreatePen(PS_SOLID, 1, RGB(90, 90, 90));
        HGDIOBJ oldBrush = SelectObject(hdc, cardBrush);
        HGDIOBJ oldPen = SelectObject(hdc, cardPen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(cardPen);
        DeleteObject(cardBrush);

        /* icon */
        HICON icon = gameIconHandles[i];
        if (!icon) {
            /* default icon */
            icon = LoadIcon(NULL, IDI_APPLICATION);
            DrawIconEx(hdc, rc.left + 10, rc.top + 10, icon, 48, 48, 0, NULL, DI_NORMAL);
        } else {
            DrawIconEx(hdc, rc.left + 10, rc.top + 10, icon, 48, 48, 0, NULL, DI_NORMAL);
        }

        /* name (truncated) */
        RECT nameRc;
        nameRc.left = rc.left + 68;
        nameRc.top = rc.top + 12;
        nameRc.right = rc.right - 10;
        nameRc.bottom = rc.top + 44;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        DrawText(hdc, games[i].name, -1, &nameRc, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

        /* playtime */
        RECT ptRc;
        ptRc.left = rc.left + 68;
        ptRc.top = rc.top + 46;
        ptRc.right = rc.right - 10;
        ptRc.bottom = rc.top + 70;
        SetTextColor(hdc, RGB(170, 170, 170));
        char ptbuf[64];
        FormatPlaytime(games[i].playtime, ptbuf, sizeof(ptbuf));
        DrawText(hdc, ptbuf, -1, &ptRc, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

        /* "open detail" hint bar */
        int btnH = 28;
        int btnY = rc.bottom - btnH - 12;
        int m = 12;

        RECT detailRc = { rc.left + m, btnY, rc.right - m, btnY + btnH };
        HBRUSH btnBrush = CreateSolidBrush(RGB(60, 70, 80));
        HGDIOBJ ob = SelectObject(hdc, btnBrush);
        HPEN btnPen = CreatePen(PS_SOLID, 1, RGB(60, 70, 80));
        HGDIOBJ op = SelectObject(hdc, btnPen);
        RoundRect(hdc, detailRc.left, detailRc.top, detailRc.right, detailRc.bottom, 6, 6);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(btnPen);
        DeleteObject(btnBrush);

        SetTextColor(hdc, RGB(220, 220, 220));
        DrawText(hdc, "Voir détails  >", -1, &detailRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* hint text if scrollable */
    if (TotalWidth() > clientW) {
        char hint[64];
        snprintf(hint, sizeof(hint), "Molette souris pour faire défiler (%d jeux)", gameCount);
        SetTextColor(hdc, RGB(150, 150, 150));
        RECT hr = {20, clientH - 30, clientW - 20, clientH - 8};
        DrawText(hdc, hint, -1, &hr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, oldFont);
    SelectClipRgn(hdc, NULL);
    DeleteObject(rgn);
}

void DashboardGameShow(int show)
{
    pageVisible = show;
    ShowWindow(btnBack, show ? SW_SHOW : SW_HIDE);
    scrollOffset = 0;
    if (hwndParent) InvalidateRect(hwndParent, NULL, TRUE);
}

void DashboardGameResize(HWND hwnd, int width, int height)
{
    clientW = width;
    clientH = height;

    SetWindowPos(btnBack, NULL, 20, 20, 150, 30, SWP_NOZORDER);
    InvalidateRect(hwnd, NULL, TRUE);
}

void DashboardGameInit(HWND hwnd)
{
    hwndParent = hwnd;

    LoadGamesFromJSON();

    for (int i = 0; i < gameCount; i++)
    {
        if (strlen(games[i].icon) > 0)
        {
            HICON icon = (HICON)LoadImage(NULL, games[i].icon, IMAGE_ICON, 48, 48, LR_LOADFROMFILE);
            if (icon)
                gameIconHandles[i] = icon;
        }
    }

    btnBack = CreateWindow(
        "BUTTON", "Retour",
        WS_CHILD | BS_DEFPUSHBUTTON,
        20, 20, 150, 30,
        hwnd, (HMENU)BTN_BACK,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);
}

void DashboardGameDestroy(void)
{
    for (int i = 0; i < MAX_GAMES; i++)
    {
        if (gameIconHandles[i])
        {
            DestroyIcon(gameIconHandles[i]);
            gameIconHandles[i] = NULL;
        }
    }
}
