@echo off
rem Builds a standalone Windows version of the game into the Packaged folder.
rem Close Unreal Editor first. The first run takes a while (it compiles the game and prepares its content).

setlocal
set "UE_DIR=C:\Program Files\Epic Games\UE_5.7"
if not exist "%UE_DIR%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo Could not find Unreal Engine 5.7 at "%UE_DIR%".
    echo Edit UE_DIR at the top of this file to point at your install.
    exit /b 1
)

call "%UE_DIR%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
    -project="%~dp0ColdCase.uproject" ^
    -noP4 -platform=Win64 -clientconfig=Development ^
    -build -cook -stage -pak -archive ^
    -archivedirectory="%~dp0Packaged" ^
    -utf8output -unattended
if errorlevel 1 (
    echo.
    echo Packaging FAILED. Scroll up for the first error.
    exit /b 1
)

echo.
echo Done. The game is in "%~dp0Packaged\Windows". Run ColdCase.exe there.
endlocal
