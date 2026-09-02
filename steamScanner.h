#ifndef STEAMSCANNER_H
#define STEAMSCANNER_H

int SteamScanAll(void);
int SteamGetCount(void);
const char *SteamGetName(int i);
const char *SteamGetPath(int i);
const char *SteamGetAppId(int i);
int SteamMergeIntoJson(void);

#endif
