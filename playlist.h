#ifndef PLAYLIST_H
#define PLAYLIST_H

#define MAX_PLAYLISTS      64
#define MAX_PL_NAME       128
#define MAX_PL_TRACKS     512
#define MAX_PL_FILENAME   260
#define MAX_PL_COVER     1024

typedef struct {
    char name[MAX_PL_NAME];
    char cover[MAX_PL_COVER];
    char tracks[MAX_PL_TRACKS][MAX_PL_FILENAME];
    int  trackCount;
} Playlist;

void    PlaylistInit(void);
void    PlaylistLoad(void);
void    PlaylistSave(void);
int     PlaylistCount(void);
Playlist *PlaylistGet(int index);
Playlist *PlaylistGetByName(const char *name);
int     PlaylistCreate(const char *name);
void    PlaylistRename(int index, const char *newName);
void    PlaylistDelete(int index);
void    PlaylistClearAll(void);
void    PlaylistSetCover(int index, const char *coverPath);
void    PlaylistAddTrack(int plIndex, const char *trackFilename);
void    PlaylistRemoveTrack(int plIndex, int trackIndexInPl);
int     PlaylistHasTrack(int plIndex, const char *trackFilename);
int     PlaylistFindByTrack(const char *trackFilename, int *outPlIndices, int maxOut);
int     PlaylistTrackIndex(int plIndex, const char *trackFilename);

#endif
