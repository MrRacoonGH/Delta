#include <stdio.h>
#include "steamScanner.h"

int main(void) {
    int n = SteamScanAll();
    printf("FOUND %d steam games\n", n);
    for (int i = 0; i < n; i++) {
        printf("[%s] %s\n  -> %s\n", SteamGetAppId(i), SteamGetName(i), SteamGetPath(i));
    }
    printf("Merging into games.json...\n");
    int r = SteamMergeIntoJson();
    printf("Merge result: %d\n", r);
    return 0;
}
