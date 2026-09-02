#include "jeux.h"
#include <windows.h>

void LancerJeu(const char* path) {
    ShellExecute(NULL, "open", path, NULL, NULL, SW_SHOW);
}
