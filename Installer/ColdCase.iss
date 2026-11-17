; Inno Setup script for COLD CASE. Builds Packaged\Installer\ColdCase_Setup.exe
; from the release build in Packaged\Release\Windows. Run MakeInstaller.bat rather than this directly.

#define AppName "COLD CASE"
#define AppVersion "0.1.0"
#define AppExe "ColdCase.exe"

[Setup]
AppId={{6C1D2B7E-4F3A-4E8B-9C1D-0A7B3E5F2C11}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Jonathan Longden
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\Packaged\Installer
OutputBaseFilename=ColdCase_Setup
Compression=lzma2/normal
SolidCompression=no
DiskSpanning=no
UninstallDisplayIcon={app}\{#AppExe}
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Files]
Source: "..\Packaged\Release\Windows\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
; Microsoft runtimes the game needs (Visual C++ and DirectX). Safe to run when they are already installed.
Filename: "{app}\Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe"; Parameters: "/quiet /norestart"; StatusMsg: "Installing Microsoft runtimes..."; Flags: waituntilterminated skipifdoesntexist
Filename: "{app}\{#AppExe}"; Description: "Play {#AppName} now"; Flags: nowait postinstall skipifsilent
