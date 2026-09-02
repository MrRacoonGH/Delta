#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define STEAM_MAX_LIBS 16
#define STEAM_MAX_GAMES 512
#define STEAM_MAX_NAME 256
#define STEAM_MAX_PATH 520

typedef struct {
    char name[STEAM_MAX_NAME];
    char path[STEAM_MAX_PATH];
    char appid[32];
} SteamGame;

static int g_steamCount = 0;
static SteamGame g_steamGames[STEAM_MAX_GAMES];

/* case-insensitive substring */
int stristr(const char *haystack, const char *needle) {
    if (!haystack || !needle || !needle[0]) return 0;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        if (_strnicmp(haystack, needle, nlen) == 0) return 1;
        size_t remaining = strlen(haystack);
        if (remaining <= nlen) break;
    }
    return 0;
}

static void trim(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' ' || s[len-1] == '\t')) {
        s[len-1] = '\0';
        len--;
    }
    size_t i = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

/* Extract the value of a quoted key from a VDF-style line like:
   "path"  "C:\\Program Files (x86)\\Steam" */
static int vdfValue(const char *line, char *out, size_t outSize) {
    /* find first quoted key start position after optional whitespace/braces */
    const char *q1 = strchr(line, '"');
    if (!q1) return 0;
    const char *q1e = strchr(q1 + 1, '"');
    if (!q1e) return 0;
    /* find second quoted value */
    const char *q2 = strchr(q1e + 1, '"');
    if (!q2) return 0;
    const char *q2e = strchr(q2 + 1, '"');
    if (!q2e) return 0;
    size_t len = (size_t)(q2e - (q2 + 1));
    if (len >= outSize) len = outSize - 1;
    memcpy(out, q2 + 1, len);
    out[len] = '\0';
    return 1;
}

/* Compute the Steam install directory via the registry. */
static int SteamGetInstallDir(char *out, size_t outSize) {
    HKEY key;
    char *data = out;
    char raw[1024] = {0};
    DWORD dataSize = sizeof(raw);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return 0;
    LONG r = RegQueryValueExA(key, "SteamPath", NULL, NULL, (LPBYTE)raw, &dataSize);
    RegCloseKey(key);
    if (r != ERROR_SUCCESS) return 0;
    /* normalize separators to backslashes */
    int oi = 0;
    for (int i = 0; raw[i] && oi < (int)outSize - 1; i++) {
        if (raw[i] == '/') data[oi++] = '\\';
        else data[oi++] = raw[i];
    }
    data[oi] = '\0';
    return strlen(out) > 0;
}

/* Read all library root paths (including the main one) from libraryfolders.vdf */
static void SteamReadLibraries(const char *steamDir,
                               char libs[][STEAM_MAX_PATH], int *libCount) {
    *libCount = 0;
    strncpy(libs[*libCount], steamDir, STEAM_MAX_PATH - 1);
    libs[*libCount][STEAM_MAX_PATH - 1] = '\0';
    (*libCount)++;

    char vdfPath[STEAM_MAX_PATH];
    snprintf(vdfPath, sizeof(vdfPath), "%s\\steamapps\\libraryfolders.vdf", steamDir);

    FILE *f = fopen(vdfPath, "r");
    if (!f) return;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        char key[64] = {0}, value[2048] = {0};
        if (!vdfValue(line, value, sizeof(value))) continue;
        const char *q = strchr(line, '"');
        if (!q) continue;
        const char *q2 = strchr(q + 1, '"');
        if (!q2) continue;
        size_t klen = (size_t)(q2 - (q + 1));
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, q + 1, klen);
        key[klen] = '\0';
        if (strcmp(key, "path") == 0 && *libCount < STEAM_MAX_LIBS) {
            char unesc[2048] = {0};
            int oi = 0;
            for (int i = 0; value[i] && oi < (int)sizeof(unesc) - 2; i++) {
                if (value[i] == '\\' && value[i+1] == '\\') { unesc[oi++] = '\\'; i++; }
                else unesc[oi++] = value[i];
            }
            unesc[oi] = '\0';

            /* normalize: lowercase and use backslashes for comparison */
            char norm[2048] = {0};
            int ni = 0;
            for (int i = 0; unesc[i] && ni < (int)sizeof(norm) - 2; i++) {
                char c = unesc[i];
                if (c == '/') c = '\\';
                c = (char)tolower((unsigned char)c);
                norm[ni++] = c;
            }
            norm[ni] = '\0';

            /* skip if already present */
            int dup = 0;
            for (int j = 0; j < *libCount; j++) {
                char existing[2048] = {0};
                int ei = 0;
                for (int k = 0; libs[j][k] && ei < (int)sizeof(existing) - 2; k++) {
                    char c = libs[j][k];
                    if (c == '/') c = '\\';
                    c = (char)tolower((unsigned char)c);
                    existing[ei++] = c;
                }
                existing[ei] = '\0';
                if (strcmp(existing, norm) == 0) { dup = 1; break; }
            }
            if (!dup) {
                strncpy(libs[*libCount], unesc, STEAM_MAX_PATH - 1);
                libs[*libCount][STEAM_MAX_PATH - 1] = '\0';
                (*libCount)++;
            }
        }
    }
    fclose(f);
}

/* For a game folder, pick the most likely main .exe */
static void FindMainExe(const char *gameFolder, const char *gameName,
                        const char *installdir, char *outExe, size_t outSize) {
    outExe[0] = '\0';

    char search[STEAM_MAX_PATH];
    snprintf(search, sizeof(search), "%s\\*", gameFolder);

    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    char best[STEAM_MAX_PATH] = "";
    int bestScore = -1;
    LARGE_INTEGER bestSize;
    bestSize.QuadPart = 0;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (fd.nFileSizeHigh != 0) continue;

        const char *ext = strrchr(fd.cFileName, '.');
        if (!ext || _stricmp(ext, ".exe") != 0) continue;

        /* Skip obvious non-game executables */
        if (_stricmp(fd.cFileName, "uninstall.exe") == 0) continue;
        if (_stricmp(fd.cFileName, "install.exe") == 0) continue;
        if (_stricmp(fd.cFileName, "setup.exe") == 0) continue;
        if (_stricmp(fd.cFileName, "redistributable.exe") == 0) continue;
        if (_stricmp(fd.cFileName, "UnityCrashHandler64.exe") == 0) continue;
        if (_stricmp(fd.cFileName, "UnityCrashHandler32.exe") == 0) continue;
        if (_stricmp(fd.cFileName, "steamclient.dll") == 0) continue;
        if (stristr(fd.cFileName, "crashhandler")) continue;

        /* Score: matching installdir folder name is best, then game name */
        char base[256] = "";
        strncpy(base, fd.cFileName, sizeof(base) - 1);
        char *d = strrchr(base, '.');
        if (d) *d = '\0';

        int score = 0;
        if (_stricmp(base, installdir) == 0) score += 100;
        if (gameName[0] && _stricmp(base, gameName) == 0) score += 80;
        if (gameName[0] && stristr(gameName, base)) score += 50;
        if (stristr(installdir, base)) score += 40;
        if (stristr(base, "launcher") || stristr(base, "Launcher")) score += 10;

        LARGE_INTEGER sz;
        sz.LowPart = fd.nFileSizeLow;
        sz.HighPart = fd.nFileSizeHigh;

        if (score > bestScore || (score == bestScore && sz.QuadPart > bestSize.QuadPart)) {
            bestScore = score;
            bestSize = sz;
            snprintf(best, sizeof(best), "%s", fd.cFileName);
        }
    } while (FindNextFile(h, &fd));

    FindClose(h);

    if (best[0]) {
        snprintf(outExe, outSize, "%s\\%s", gameFolder, best);
    }
}

/* Scan one appmanifest acf and add its game */
static void ScanAcf(const char *commonDir, const char *acfPath) {
    FILE *f = fopen(acfPath, "r");
    if (!f) return;

    char name[STEAM_MAX_NAME] = "", installdir[STEAM_MAX_NAME] = "", appid[32] = "";
    char line[2048];

    while (fgets(line, sizeof(line), f)) {
        char val[2048] = {0};
        if (!vdfValue(line, val, sizeof(val))) continue;
        const char *q = strchr(line, '"');
        if (!q) continue;
        const char *q2 = strchr(q + 1, '"');
        if (!q2) continue;
        size_t klen = (size_t)(q2 - (q + 1));
        char key[64] = {0};
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, q + 1, klen);
        key[klen] = '\0';

        if (strcmp(key, "name") == 0) { trim(val); strncpy(name, val, sizeof(name) - 1); }
        else if (strcmp(key, "installdir") == 0) { trim(val); strncpy(installdir, val, sizeof(installdir) - 1); }
        else if (strcmp(key, "appid") == 0) { trim(val); strncpy(appid, val, sizeof(appid) - 1); }
    }
    fclose(f);

    if (!installdir[0]) return;

    char gameFolder[STEAM_MAX_PATH];
    snprintf(gameFolder, sizeof(gameFolder), "%s\\%s", commonDir, installdir);

    if (GetFileAttributesA(gameFolder) == INVALID_FILE_ATTRIBUTES) return;

    char exe[STEAM_MAX_PATH] = "";
    FindMainExe(gameFolder, name, installdir, exe, sizeof(exe));
    if (!exe[0]) return;

    if (g_steamCount >= STEAM_MAX_GAMES) return;

    strncpy(g_steamGames[g_steamCount].name, name[0] ? name : installdir, sizeof(g_steamGames[g_steamCount].name) - 1);
    strncpy(g_steamGames[g_steamCount].path, exe, sizeof(g_steamGames[g_steamCount].path) - 1);
    strncpy(g_steamGames[g_steamCount].appid, appid, sizeof(g_steamGames[g_steamCount].appid) - 1);
    g_steamCount++;
}

/* Scan all libraries for appmanifests and add discovered games to g_steamGames */
static void SteamScanLibs(const char libs[][STEAM_MAX_PATH], int libCount) {
    for (int i = 0; i < libCount; i++) {
        char appsDir[STEAM_MAX_PATH];
        snprintf(appsDir, sizeof(appsDir), "%s\\steamapps", libs[i]);

        char search[STEAM_MAX_PATH];
        snprintf(search, sizeof(search), "%s\\appmanifest_*.acf", appsDir);

        WIN32_FIND_DATA fd;
        HANDLE h = FindFirstFile(search, &fd);
        if (h == INVALID_HANDLE_VALUE) continue;

        char commonDir[STEAM_MAX_PATH];
        snprintf(commonDir, sizeof(commonDir), "%s\\steamapps\\common", libs[i]);

        do {
            char full[STEAM_MAX_PATH];
            snprintf(full, sizeof(full), "%s\\%s", appsDir, fd.cFileName);
            ScanAcf(commonDir, full);
        } while (FindNextFile(h, &fd) && g_steamCount < STEAM_MAX_GAMES);

        FindClose(h);
    }
}

/* Public: scan Steam and fill internal list. Returns game count. */
int SteamScanAll(void) {
    g_steamCount = 0;

    char steamDir[STEAM_MAX_PATH] = "";
    if (!SteamGetInstallDir(steamDir, sizeof(steamDir)))
        return 0;

    char libs[STEAM_MAX_LIBS][STEAM_MAX_PATH];
    int libCount = 0;
    SteamReadLibraries(steamDir, libs, &libCount);

    SteamScanLibs(libs, libCount);
    return g_steamCount;
}

/* Public: accessors */
int SteamGetCount(void) { return g_steamCount; }
const char *SteamGetName(int i) { return g_steamGames[i].name; }
const char *SteamGetPath(int i) { return g_steamGames[i].path; }
const char *SteamGetAppId(int i) { return g_steamGames[i].appid; }

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

/* Check whether a file or folder exists on disk */
static int FileExists(const char *path) {
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

/* Lowercase + forward-slash to backslash normalization for path comparison */
static void NormalizePath(const char *in, char *out) {
    int oi = 0;
    for (int i = 0; in[i] && oi < STEAM_MAX_PATH - 1; i++) {
        char c = in[i];
        if (c == '/') c = '\\';
        c = (char)tolower((unsigned char)c);
        out[oi++] = c;
    }
    out[oi] = '\0';
}

/* Public: merge discovered Steam games into games.json (append, dedupe by exe path).
   Existing entries are preserved. Returns 1 on success. */
int SteamMergeIntoJson(void) {
    char appDir[STEAM_MAX_PATH];
    GetModuleFileName(NULL, appDir, sizeof(appDir));
    char *s = strrchr(appDir, '\\');
    if (!s) return 0;
    s[1] = '\0';

    char jsonPath[STEAM_MAX_PATH];
    snprintf(jsonPath, sizeof(jsonPath), "%sgames.json", appDir);

    /* Read existing content */
    FILE *old = fopen(jsonPath, "r");
    char *oldContent = NULL;
    long oldLen = 0;
    if (old) {
        fseek(old, 0, SEEK_END);
        oldLen = ftell(old);
        fseek(old, 0, SEEK_SET);
        if (oldLen > 0) {
            oldContent = (char*)malloc((size_t)oldLen + 1);
            if (oldContent) {
                size_t rd = fread(oldContent, 1, (size_t)oldLen, old);
                oldContent[rd] = '\0';
            }
        }
        fclose(old);
    }

    FILE *f = fopen(jsonPath, "w");
    if (!f) { if (oldContent) free(oldContent); return 0; }

    fprintf(f, "{\n  \"games\": [\n");

    /* Write existing entries, collect their normalized paths */
    char existingPaths[STEAM_MAX_GAMES][STEAM_MAX_PATH];
    int existingCount = 0;
    int wroteAny = 0;
    if (oldContent && strstr(oldContent, "\"name\"")) {
        char *p = oldContent;
        char *brace = strstr(p, "[");
        if (brace) p = brace + 1;

        for (;;) {
            char *nameStart = strstr(p, "\"name\"");
            if (!nameStart) break;

            char *pathStart = strstr(nameStart, "\"path\"");
            if (!pathStart) break;
            char *iconStart = strstr(pathStart, "\"icon\"");

            char *nameVal = strchr(nameStart, ':');
            char *nameOpen = nameVal ? strchr(nameVal, '"') : NULL;
            char *nameClose = nameOpen ? strchr(nameOpen + 1, '"') : NULL;
            char *pathVal = strchr(pathStart, ':');
            char *pathOpen = pathVal ? strchr(pathVal, '"') : NULL;
            char *pathClose = pathOpen ? strchr(pathOpen + 1, '"') : NULL;
            char *iconOpen = NULL, *iconClose = NULL;
            if (iconStart) {
                char *iconVal = strchr(iconStart, ':');
                iconOpen = iconVal ? strchr(iconVal, '"') : NULL;
                iconClose = iconOpen ? strchr(iconOpen + 1, '"') : NULL;
            }

            if (!nameOpen || !nameClose || !pathOpen || !pathClose) break;

            char name[512] = ""; size_t nlen = (size_t)(nameClose - nameOpen - 1);
            if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
            memcpy(name, nameOpen + 1, nlen); name[nlen] = '\0';

            char path[520] = ""; size_t plen = (size_t)(pathClose - pathOpen - 1);
            if (plen >= sizeof(path)) plen = sizeof(path) - 1;
            memcpy(path, pathOpen + 1, plen); path[plen] = '\0';

            char icon[520] = ""; size_t ilen = 0;
            if (iconOpen && iconClose) {
                ilen = (size_t)(iconClose - iconOpen - 1);
                if (ilen >= sizeof(icon)) ilen = sizeof(icon) - 1;
                memcpy(icon, iconOpen + 1, ilen); icon[ilen] = '\0';
            }

            char banner[520] = "";
            char *bannerStart = strstr(iconStart ? iconStart : pathClose, "\"banner\"");
            if (bannerStart) {
                char *bVal = strchr(bannerStart, ':');
                char *bOpen = bVal ? strchr(bVal, '"') : NULL;
                char *bClose = bOpen ? strchr(bOpen + 1, '"') : NULL;
                if (bOpen && bClose) {
                    size_t blen = (size_t)(bClose - bOpen - 1);
                    if (blen >= sizeof(banner)) blen = sizeof(banner) - 1;
                    memcpy(banner, bOpen + 1, blen); banner[blen] = '\0';
                }
            }

            long playtime = 0;
            char *playStart = strstr(iconStart ? iconStart : pathClose, "\"playtime\"");
            if (playStart) {
                char *pVal = strchr(playStart, ':');
                if (pVal) playtime = atol(pVal + 1);
            }

            JsonUnescape(path);
            JsonUnescape(icon);
            JsonUnescape(name);
            if (banner[0]) JsonUnescape(banner);

            /* advance past this entry */
            char *nameNext = strstr(playStart ? playStart : iconClose ? iconClose : pathClose, "\"name\"");
            p = (nameNext && nameNext > nameStart) ? nameNext : (iconClose ? iconClose + 1 : pathClose + 1);

            /* Drop entries whose exe no longer exists on disk (uninstalled) */
            if (!FileExists(path))
                continue;

            if (existingCount < STEAM_MAX_GAMES) {
                NormalizePath(path, existingPaths[existingCount]);
                existingCount++;
            }

            if (wroteAny) fprintf(f, ",\n");
            char en[512*2], ep[600*2], ei[600*2], eb[600*2];
            JsonEscape(name, en, sizeof(en));
            JsonEscape(path, ep, sizeof(ep));
            JsonEscape(icon, ei, sizeof(ei));
            JsonEscape(banner, eb, sizeof(eb));
            fprintf(f, "    {\n");
            fprintf(f, "      \"name\": \"%s\",\n", en);
            fprintf(f, "      \"path\": \"%s\",\n", ep);
            if (icon[0]) fprintf(f, "      \"icon\": \"%s\",\n", ei);
            if (banner[0]) fprintf(f, "      \"banner\": \"%s\",\n", eb);
            fprintf(f, "      \"playtime\": %ld\n", playtime);
            fprintf(f, "    }");
            wroteAny = 1;
        }
    }

    /* Append new Steam games, dedupe by normalized path */
    for (int i = 0; i < g_steamCount; i++) {
        const char *sp = SteamGetPath(i);

        char norm[STEAM_MAX_PATH];
        NormalizePath(sp, norm);

        int dup = 0;
        for (int j = 0; j < existingCount; j++) {
            if (strcmp(norm, existingPaths[j]) == 0) { dup = 1; break; }
        }
        if (dup) continue;

        char ename[STEAM_MAX_NAME * 2];
        char epath[STEAM_MAX_PATH * 2];
        JsonEscape(SteamGetName(i), ename, sizeof(ename));
        JsonEscape(sp, epath, sizeof(epath));

        if (existingCount < STEAM_MAX_GAMES) {
            NormalizePath(sp, existingPaths[existingCount]);
            existingCount++;
        }

        if (wroteAny) fprintf(f, ",\n");
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", ename);
        fprintf(f, "      \"path\": \"%s\",\n", epath);
        fprintf(f, "      \"playtime\": 0\n");
        fprintf(f, "    }");
        wroteAny = 1;
    }

    fprintf(f, "\n  ]\n}\n");
    fclose(f);

    if (oldContent) free(oldContent);
    return 1;
}
