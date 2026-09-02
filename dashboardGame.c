#define _CRT_SECURE_NO_WARNINGS

#include "dashboardGame.h"
#include "dashboard.h"
#include "jeux.h"
#include <stdio.h>
#include <string.h>

static GameEntry games[MAX_GAMES];
static int gameCount = 0;

static HWND gameIcons[MAX_GAMES];
static HWND gameNames[MAX_GAMES];
static HWND gameLaunch[MAX_GAMES];
static HWND gameFolder[MAX_GAMES];
static HICON gameIconHandles[MAX_GAMES];

static HWND btnBack;

void DashboardGameDraw(HDC hdc)
{
    // Rien à dessiner pour l'instant
}

void LoadGamesFromJSON()
{
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

    char buffer[8192];
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[read] = '\0';
    fclose(f);

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
            strncpy(games[gameCount].name, ptr, len);
            games[gameCount].name[len] = '\0';
        }

        // PATH
        ptr = strstr(end, "\"path\"");
        ptr = strchr(ptr, ':');
        ptr = strchr(ptr, '"') + 1;
        end = strchr(ptr, '"');
        {
            size_t len = (size_t)(end - ptr);
            if (len >= sizeof(games[gameCount].path)) len = sizeof(games[gameCount].path) - 1;
            strncpy(games[gameCount].path, ptr, len);
            games[gameCount].path[len] = '\0';
        }

        // ICON
        ptr = strstr(end, "\"icon\"");
        if (ptr)
        {
            ptr = strchr(ptr, ':');
            ptr = strchr(ptr, '"') + 1;
            end = strchr(ptr, '"');
            size_t len = (size_t)(end - ptr);
            if (len >= sizeof(games[gameCount].icon)) len = sizeof(games[gameCount].icon) - 1;
            strncpy(games[gameCount].icon, ptr, len);
            games[gameCount].icon[len] = '\0';
        }
        else
        {
            strcpy(games[gameCount].icon, "");
        }

        gameCount++;
    }
}

void DashboardGameInit(HWND hwnd)
{
    LoadGamesFromJSON();

    int y = 20;

    for (int i = 0; i < gameCount; i++)
    {
        // Icône du jeu (.ico)
        gameIcons[i] = CreateWindow(
            "STATIC", NULL,
            WS_CHILD | SS_ICON,
            20, y, 32, 32,
            hwnd, NULL,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);

        if (strlen(games[i].icon) > 0)
        {
            HICON icon = (HICON)LoadImage(
                NULL,
                games[i].icon,
                IMAGE_ICON,
                32, 32,
                LR_LOADFROMFILE
            );

            if (icon)
            {
                SendMessage(gameIcons[i], STM_SETIMAGE, IMAGE_ICON, (LPARAM)icon);
                gameIconHandles[i] = icon;
            }
        }

        // Nom du jeu
        gameNames[i] = CreateWindow(
            "STATIC",
            games[i].name,
            WS_CHILD,
            70, y, 200, 30,
            hwnd,
            NULL,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);

        // Bouton Lancer
        gameLaunch[i] = CreateWindow(
            "BUTTON", "Lancer",
            WS_CHILD | BS_DEFPUSHBUTTON,
            300, y, 100, 30,
            hwnd,
            (HMENU)(UINT_PTR)(BTN_GAME_LAUNCH_BASE + i),
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);

        // Bouton Ouvrir dossier
        gameFolder[i] = CreateWindow(
            "BUTTON", "Ouvrir dossier",
            WS_CHILD | BS_DEFPUSHBUTTON,
            410, y, 120, 30,
            hwnd,
            (HMENU)(UINT_PTR)(BTN_GAME_FOLDER_BASE + i),
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);

        y += 50;
    }

    btnBack = CreateWindow(
        "BUTTON", "Retour",
        WS_CHILD | BS_DEFPUSHBUTTON,
        20, y + 20, 200, 30,
        hwnd, (HMENU)BTN_BACK,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);
}

void DashboardGameShow(int show)
{
    for (int i = 0; i < gameCount; i++)
    {
        ShowWindow(gameIcons[i], show ? SW_SHOW : SW_HIDE);
        ShowWindow(gameNames[i], show ? SW_SHOW : SW_HIDE);
        ShowWindow(gameLaunch[i], show ? SW_SHOW : SW_HIDE);
        ShowWindow(gameFolder[i], show ? SW_SHOW : SW_HIDE);
    }

    ShowWindow(btnBack, show ? SW_SHOW : SW_HIDE);
}

void DashboardGameResize(HWND hwnd, int width, int height)
{
    int y = 20;

    for (int i = 0; i < gameCount; i++)
    {
        SetWindowPos(gameIcons[i], NULL, 20, y, 32, 32, SWP_NOZORDER);
        SetWindowPos(gameNames[i], NULL, 70, y, 200, 30, SWP_NOZORDER);
        SetWindowPos(gameLaunch[i], NULL, 300, y, 100, 30, SWP_NOZORDER);
        SetWindowPos(gameFolder[i], NULL, 410, y, 120, 30, SWP_NOZORDER);

        y += 50;
    }

    SetWindowPos(btnBack, NULL, 20, y + 20, 200, 30, SWP_NOZORDER);
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

        // retirer le .exe
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
