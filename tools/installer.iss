#ifndef AppVersion
  #error AppVersion must be supplied by the release workflow
#endif

[Setup]
AppId={{B2DF0E57-4BD9-4AA1-A675-EEDE52965569}
AppName=FlintAutoClicker
AppVersion={#AppVersion}
AppPublisher=Atharva More
AppPublisherURL=https://github.com/AtharvaMore1/Flint-Autoclicker
DefaultDirName={localappdata}\Programs\FlintAutoClicker
DefaultGroupName=FlintAutoClicker
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
DisableProgramGroupPage=yes
WizardStyle=modern
SetupIconFile=..\assets\app.ico
UninstallDisplayIcon={app}\FlintAutoClicker.exe
OutputDir=..\installer
OutputBaseFilename=FlintAutoClicker-v{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\FlintAutoClicker"; Filename: "{app}\FlintAutoClicker.exe"
Name: "{autodesktop}\FlintAutoClicker"; Filename: "{app}\FlintAutoClicker.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\FlintAutoClicker.exe"; Description: "Launch FlintAutoClicker"; Flags: nowait postinstall skipifsilent
