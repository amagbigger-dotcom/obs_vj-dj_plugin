@echo off
chcp 65001 >nul
echo ========================================================
echo OBS VJ/DJ Plugin Telepito (Build #309 / Run #44779958565)
echo ========================================================
echo.
:CHECK_OBS
tasklist /FI "IMAGENAME eq obs64.exe" 2>nul | findstr /i "obs64.exe" >nul
if %ERRORLEVEL% EQU 0 (
    echo [INFO] Az OBS Studio jelenleg fut. Kíméletes bezárás kezdeményezése...
    taskkill /IM obs64.exe >nul 2>&1
    timeout /t 2 /nobreak >nul
    tasklist /FI "IMAGENAME eq obs64.exe" 2>nul | findstr /i "obs64.exe" >nul
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo [FIGYELEM] Az OBS Studio még nyitva van!
        echo Kérlek mentsd el az állapotot és zárd be az OBS-t a telepítés befejezéséhez!
        echo Várakozás az OBS bezárására (vagy nyomj Ctrl+C-t a megszakításhoz)...
        timeout /t 4 >nul
        goto CHECK_OBS
    )
)

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
