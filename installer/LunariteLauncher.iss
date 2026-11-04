; Lunarite launcher setup. Installs the launcher bundle (out\bundle) into a game folder and adds
; shortcuts; the launcher then downloads the whole game into that folder. Uninstalling removes the
; launcher and the game files it downloaded.
; Built by Make-LauncherBundle.ps1, which passes /DAppVersion=<version> /DBundleDir=<bundle folder>.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BundleDir
  #define BundleDir "..\out\bundle"
#endif

[Setup]
AppId={{D90CE85D-1199-4DF7-94F1-B0BD14A8E5A6}
AppName=Lunarite Launcher
AppVersion={#AppVersion}
AppPublisher=Lunarite
AppPublisherURL=https://lunarite-website.vercel.app/
; Outside Program Files: the game writes its configs and logs into its own folder.
DefaultDirName={sd}\Games\Lunarite
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline dialog
; The game the launcher downloads, about 22 GB.
ExtraDiskSpaceRequired=23000000000
WizardStyle=modern
SetupIconFile={#BundleDir}\LauncherImage\HuxleyTheDistopia.ico
UninstallDisplayIcon={app}\WZLauncher.exe
UninstallDisplayName=Lunarite Launcher
OutputDir=..\out
OutputBaseFilename=LunariteLauncher-win-Setup
Compression=lzma2
SolidCompression=yes

[Messages]
SelectDirLabel3=Setup will install the Lunarite launcher into this folder. The launcher then downloads Huxley (about 22 GB) into the same folder.

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Files]
Source: "{#BundleDir}\*"; DestDir: "{app}"; Excludes: "README.txt"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{autoprograms}\Lunarite Launcher"; Filename: "{app}\WZLauncher.exe"; WorkingDir: "{app}"; IconFilename: "{app}\LauncherImage\HuxleyTheDistopia.ico"
Name: "{autodesktop}\Lunarite Launcher"; Filename: "{app}\WZLauncher.exe"; WorkingDir: "{app}"; IconFilename: "{app}\LauncherImage\HuxleyTheDistopia.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\WZLauncher.exe"; WorkingDir: "{app}"; Description: "Start the launcher"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Binaries"
Type: filesandordirs; Name: "{app}\Engine"
Type: filesandordirs; Name: "{app}\Huxley"
Type: filesandordirs; Name: "{app}\HuxleyGame"
Type: filesandordirs; Name: "{app}\HuxleyMMOGame"
Type: filesandordirs; Name: "{app}\DownloadTemp"
Type: filesandordirs; Name: "{app}\LauncherTemp"
Type: files; Name: "{app}\Version_2.txt"
Type: files; Name: "{app}\WZLauncher.Log"
Type: dirifempty; Name: "{app}"

[Code]
// Game paths reach 110 characters under the folder, and the launcher works within 260.
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectDir) and (Length(WizardDirValue) > 140) then
  begin
    MsgBox('This folder path is too long for the game. Pick a shorter one, for example C:\Games\Lunarite.',
           mbError, MB_OK);
    Result := False;
  end;
end;
