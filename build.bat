@echo off
setlocal
set "PATH=C:\Users\hadri\Desktop\DeltaGame\w64devkit\bin;%PATH%"
gcc main.c dashboard.c dashboardGame.c jeux.c musicPlayer.c -o DeltaGame.exe -lwinmm -lmfplay -lmfplat -lole32 -loleaut32 -luuid -lcomdlg32 -lgdiplus -mwindows
if %errorlevel%==0 (
    echo Compilation reussie.
) else (
    echo Erreur de compilation.
)
endlocal
