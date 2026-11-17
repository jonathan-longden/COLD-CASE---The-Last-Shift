@echo off
rem Makes the installer for players: Packaged\Installer\ColdCase_Setup.exe
rem It first builds the release version of the game (Package.bat release), then wraps it with Inno Setup.
rem Needs Inno Setup 6 (free): https://jrsoftware.org/isinfo.php
rem   MakeInstaller.bat          build the release game, then the installer
rem   MakeInstaller.bat nobuild  only remake the installer from the existing release build

setlocal
set "ISCC="
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not defined ISCC (
    echo Could not find Inno Setup 6. Install it from https://jrsoftware.org/isinfo.php and run this again.
    exit /b 1
)

if /i not "%~1"=="nobuild" (
    call "%~dp0Package.bat" release
    if errorlevel 1 exit /b 1
)

if not exist "%~dp0Packaged\Release\Windows\ColdCase.exe" (
    echo The release build is missing. Run Package.bat release first.
    exit /b 1
)

"%ISCC%" "%~dp0Installer\ColdCase.iss"
if errorlevel 1 (
    echo.
    echo Making the installer failed. See the messages above.
    exit /b 1
)

echo.
echo Done. The installer is "%~dp0Packaged\Installer\ColdCase_Setup.exe".
endlocal
