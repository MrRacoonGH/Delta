#!/bin/sh
export PATH="/c/Users/hadri/Desktop/DeltaGame/w64devkit/bin:$PATH"
gcc main.c dashboard.c dashboardGame.c jeux.c musicPlayer.c -o DeltaGame.exe -lwinmm -mwindows
if [ $? -eq 0 ]; then
    echo "Compilation reussie."
else
    echo "Erreur de compilation."
fi
