#ifndef STEAMSCANNER_H
#define STEAMSCANNER_H

int SteamScanAll(void);
int SteamGetCount(void);
const char *SteamGetName(int i);
const char *SteamGetPath(int i);
const char *SteamGetAppId(int i);
int SteamMergeIntoJson(void);
long SteamGetPlaytime(const char *appid);
long long SteamGetLastPlayed(const char *appid);
const char *SteamGetAppIdForPath(const char *path);

#endif
