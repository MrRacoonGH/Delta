#define _CRT_SECURE_NO_WARNINGS

#include "playlist.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Playlist g_playlists[MAX_PLAYLISTS];
static int g_plCount = 0;

static void GetPlaylistPath(char *out, size_t outSize) {
    GetModuleFileName(NULL, out, (DWORD)outSize);
    char *s = strrchr(out, '\\');
    if (s) s[1] = '\0';
    strcat(out, "playlists.json");
}

static void Trim(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' '))
        s[--len] = '\0';
    size_t i = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

void PlaylistInit(void) {
    g_plCount = 0;
    memset(g_playlists, 0, sizeof(g_playlists));
    PlaylistLoad();
}

static void JsonWriteString(FILE *f, const char *str) {
    fputc('"', f);
    for (const char *p = str; *p; p++) {
        if (*p == '"')  fputs("\\\"", f);
        else if (*p == '\\') fputs("\\\\", f);
        else if (*p == '\n') fputs("\\n", f);
        else fputc(*p, f);
    }
    fputc('"', f);
}

static int JsonReadString(const char *src, char *out, size_t outSize) {
    const char *q = strchr(src, '"');
    if (!q) return 0;
    q++;
    size_t i = 0;
    while (*q && *q != '"' && i < outSize - 1) {
        if (*q == '\\' && *(q+1)) {
            q++;
            if (*q == 'n') out[i++] = '\n';
            else out[i++] = *q;
        } else {
            out[i++] = *q;
        }
        q++;
    }
    out[i] = '\0';
    return 1;
}

void PlaylistLoad(void) {
    g_plCount = 0;
    char path[MAX_PATH];
    GetPlaylistPath(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return; }

    char *buf = (char*)malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return; }
    size_t rd = fread(buf, 1, (size_t)fsize, f);
    buf[rd] = '\0';
    fclose(f);

    char *p = strstr(buf, "\"playlists\"");
    if (!p) { free(buf); return; }
    char *arrOpen = strchr(p, '[');
    if (!arrOpen) { free(buf); return; }
    p = arrOpen + 1;

    while (*p && g_plCount < MAX_PLAYLISTS) {
        char *objOpen = strchr(p, '{');
        if (!objOpen) break;
        p = objOpen + 1;

        Playlist *pl = &g_playlists[g_plCount];
        memset(pl, 0, sizeof(*pl));

        char *nameKey = strstr(p, "\"name\"");
        char *coverKey = strstr(p, "\"cover\"");
        char *tracksKey = strstr(p, "\"tracks\"");

        if (nameKey) {
            char *val = strchr(nameKey, ':');
            if (val) { val++; while (*val == ' ') val++; JsonReadString(val, pl->name, sizeof(pl->name)); }
        }
        if (coverKey) {
            char *val = strchr(coverKey, ':');
            if (val) { val++; while (*val == ' ') val++; JsonReadString(val, pl->cover, sizeof(pl->cover)); }
        }

        pl->trackCount = 0;
        if (tracksKey) {
            char *arrS = strchr(tracksKey, '[');
            if (arrS) {
                const char *t = arrS + 1;
                while (*t && *t != ']' && pl->trackCount < MAX_PL_TRACKS) {
                    while (*t == ' ' || *t == '\n' || *t == '\r' || *t == ',') t++;
                    if (*t == '"') {
                        JsonReadString(t, pl->tracks[pl->trackCount], sizeof(pl->tracks[0]));
                        pl->trackCount++;
                        const char *end = strchr(t + 1, '"');
                        if (end) t = end + 1;
                        else break;
                    } else break;
                }
            }
        }

        g_plCount++;
        char *objClose = strchr(p, '}');
        if (objClose) p = objClose + 1;
        else break;
    }

    free(buf);
}

void PlaylistSave(void) {
    char path[MAX_PATH];
    GetPlaylistPath(path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "{\n  \"playlists\": [\n");
    for (int i = 0; i < g_plCount; i++) {
        Playlist *pl = &g_playlists[i];
        fprintf(f, "    {\n      "); JsonWriteString(f, "name"); fprintf(f, ": "); JsonWriteString(f, pl->name); fprintf(f, ",\n");
        fprintf(f, "      "); JsonWriteString(f, "cover"); fprintf(f, ": "); JsonWriteString(f, pl->cover); fprintf(f, ",\n");
        fprintf(f, "      "); JsonWriteString(f, "tracks"); fprintf(f, ": [");
        for (int j = 0; j < pl->trackCount; j++) {
            if (j > 0) fprintf(f, ", ");
            fprintf(f, "\n        "); JsonWriteString(f, pl->tracks[j]);
        }
        if (pl->trackCount > 0) fprintf(f, "\n      ");
        fprintf(f, "]\n");
        fprintf(f, "    }");
        if (i < g_plCount - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
}

int PlaylistCount(void) { return g_plCount; }

Playlist *PlaylistGet(int index) {
    if (index >= 0 && index < g_plCount) return &g_playlists[index];
    return NULL;
}

Playlist *PlaylistGetByName(const char *name) {
    for (int i = 0; i < g_plCount; i++)
        if (_stricmp(g_playlists[i].name, name) == 0) return &g_playlists[i];
    return NULL;
}

int PlaylistCreate(const char *name) {
    if (g_plCount >= MAX_PLAYLISTS) return -1;
    Playlist *pl = &g_playlists[g_plCount];
    memset(pl, 0, sizeof(*pl));
    strncpy(pl->name, name, sizeof(pl->name) - 1);
    g_plCount++;
    PlaylistSave();
    return g_plCount - 1;
}

void PlaylistRename(int index, const char *newName) {
    if (index < 0 || index >= g_plCount) return;
    strncpy(g_playlists[index].name, newName, sizeof(g_playlists[0].name) - 1);
    g_playlists[index].name[sizeof(g_playlists[0].name) - 1] = '\0';
    PlaylistSave();
}

void PlaylistDelete(int index) {
    if (index < 0 || index >= g_plCount) return;
    for (int i = index; i < g_plCount - 1; i++)
        g_playlists[i] = g_playlists[i + 1];
    g_plCount--;
    memset(&g_playlists[g_plCount], 0, sizeof(Playlist));
    PlaylistSave();
}

void PlaylistClearAll(void) {
    g_plCount = 0;
    memset(g_playlists, 0, sizeof(g_playlists));
    PlaylistSave();
}

void PlaylistSetCover(int index, const char *coverPath) {
    if (index < 0 || index >= g_plCount) return;
    strncpy(g_playlists[index].cover, coverPath, sizeof(g_playlists[0].cover) - 1);
    g_playlists[index].cover[sizeof(g_playlists[0].cover) - 1] = '\0';
    PlaylistSave();
}

int PlaylistTrackIndex(int plIndex, const char *trackFilename) {
    Playlist *pl = PlaylistGet(plIndex);
    if (!pl) return -1;
    for (int i = 0; i < pl->trackCount; i++)
        if (_stricmp(pl->tracks[i], trackFilename) == 0) return i;
    return -1;
}

void PlaylistAddTrack(int plIndex, const char *trackFilename) {
    Playlist *pl = PlaylistGet(plIndex);
    if (!pl || pl->trackCount >= MAX_PL_TRACKS) return;
    if (PlaylistTrackIndex(plIndex, trackFilename) >= 0) return;
    strncpy(pl->tracks[pl->trackCount], trackFilename, sizeof(pl->tracks[0]) - 1);
    pl->tracks[pl->trackCount][sizeof(pl->tracks[0]) - 1] = '\0';
    pl->trackCount++;
    PlaylistSave();
}

void PlaylistRemoveTrack(int plIndex, int trackIndexInPl) {
    Playlist *pl = PlaylistGet(plIndex);
    if (!pl || trackIndexInPl < 0 || trackIndexInPl >= pl->trackCount) return;
    for (int i = trackIndexInPl; i < pl->trackCount - 1; i++)
        strcpy(pl->tracks[i], pl->tracks[i + 1]);
    pl->trackCount--;
    PlaylistSave();
}

int PlaylistHasTrack(int plIndex, const char *trackFilename) {
    return PlaylistTrackIndex(plIndex, trackFilename) >= 0;
}

int PlaylistFindByTrack(const char *trackFilename, int *outPlIndices, int maxOut) {
    int count = 0;
    for (int i = 0; i < g_plCount && count < maxOut; i++)
        if (PlaylistHasTrack(i, trackFilename))
            outPlIndices[count++] = i;
    return count;
}
