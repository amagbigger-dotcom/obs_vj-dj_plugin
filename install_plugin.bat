@echo off
chcp 65001 >nul
echo ========================================================
echo OBS VJ/DJ Plugin Telepito (Build #309 / Run #44779958565)
echo ========================================================
echo.
echo OBS zarasa ha fut...
taskkill /F /IM obs64.exe >nul 2>&1

echo Masolas a Program Files mappaba...
copy /Y "%~dp0obs_dj-vj_plugin.dll" "C:\Program Files\obs-studio\obs-plugins\64bit\obs_dj-vj_plugin.dll"
if %ERRORLEVEL% EQU 0 (
    echo.
    echo [OK] SIKERES TELEPITES!
    echo A legújabb obs_dj-vj_plugin.dll sikeresen bekerült az OBS-be.
) else (
    echo.
    echo [FIGYELEM] A másoláshoz rendszergazdai jogosultság szükséges!
    echo Jobb klikk a fájlra -> "Futtatás rendszergazdaként"
)
echo.
pause
