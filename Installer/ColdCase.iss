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
UninstallDisplayIcon={app}\ColdCase.ico
SetupIconFile=..\Build\Windows\Application.ico
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Files]
Source: "..\Packaged\Release\Windows\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\Build\Windows\Application.ico"; DestDir: "{app}"; DestName: "ColdCase.ico"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"; IconFilename: "{app}\ColdCase.ico"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; IconFilename: "{app}\ColdCase.ico"; Tasks: desktopicon

[Run]
; Microsoft Visual C++ runtime the game needs (UE 5.7 ships vc_redist instead of UEPrereqSetup). Safe to run when it is already installed.
Filename: "{app}\Engine\Extras\Redist\en-us\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing Microsoft Visual C++ runtime..."; Flags: waituntilterminated skipifdoesntexist
Filename: "{app}\{#AppExe}"; Description: "Play {#AppName} now"; Flags: nowait postinstall skipifsilent
