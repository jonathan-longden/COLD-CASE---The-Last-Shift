@echo off
rem Builds a standalone Windows version of the game into the Packaged folder.
rem Close Unreal Editor first. The first run takes a while (it compiles the game and prepares its content).
rem   Package.bat           test build (keeps the console and log) into Packaged\Windows
rem   Package.bat release   release build for players (Shipping, no console, with the Microsoft runtime installer) into Packaged\Release\Windows

setlocal
set "CONFIG=Development"
set "EXTRA="
set "OUTDIR=%~dp0Packaged"
if /i "%~1"=="release" (
    set "CONFIG=Shipping"
    set "EXTRA=-prereqs -nodebuginfo"
    set "OUTDIR=%~dp0Packaged\Release"
)
set "UE_DIR=C:\Program Files\Epic Games\UE_5.7"
if not exist "%UE_DIR%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo Could not find Unreal Engine 5.7 at "%UE_DIR%".
    echo Edit UE_DIR at the top of this file to point at your install.
    exit /b 1
)

call "%UE_DIR%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
    -project="%~dp0ColdCase.uproject" ^
    -noP4 -platform=Win64 -clientconfig=%CONFIG% ^
    -build -cook -stage -pak -archive %EXTRA% ^
    -archivedirectory="%OUTDIR%" ^
    -utf8output -unattended
if errorlevel 1 (
    echo.
    echo Packaging FAILED. Scroll up for the first error.
    exit /b 1
)

echo.
echo Done. The game is in "%OUTDIR%\Windows". Run ColdCase.exe there.
endlocal
