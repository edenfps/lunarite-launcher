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
; Inno Setup 5.6 builds a setup that also runs on Windows XP; Inno Setup 6 setups need Vista or later.
; Make-LauncherBundle.ps1 prefers Inno Setup 5. The Inno 6 extras stay for builds without it.
#if Ver >= EncodeVer(6,0,0)
  #define Programs "{autoprograms}"
  #define Desktop "{autodesktop}"
#else
  #define Programs "{userprograms}"
  #define Desktop "{userdesktop}"
#endif

[Setup]
AppId={{D90CE85D-1199-4DF7-94F1-B0BD14A8E5A6}
AppName=Lunarite Launcher
AppVersion={#AppVersion}
AppPublisher=Lunarite
AppPublisherURL=https://huxleythedystopia.com/
; Outside Program Files: the game writes its configs and logs into its own folder.
DefaultDirName={sd}\Games\Lunarite
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
PrivilegesRequired=lowest
#if Ver >= EncodeVer(6,0,0)
PrivilegesRequiredOverridesAllowed=commandline dialog
#endif
; The game the launcher downloads, about 22 GB.
ExtraDiskSpaceRequired=23000000000
#if Ver >= EncodeVer(6,0,0)
WizardStyle=modern
#endif
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
Name: "{#Programs}\Lunarite Launcher"; Filename: "{app}\WZLauncher.exe"; WorkingDir: "{app}"; IconFilename: "{app}\LauncherImage\HuxleyTheDistopia.ico"
Name: "{#Desktop}\Lunarite Launcher"; Filename: "{app}\WZLauncher.exe"; WorkingDir: "{app}"; IconFilename: "{app}\LauncherImage\HuxleyTheDistopia.ico"; Tasks: desktopicon

[Run]
; Not under Wine/Proton: Steam ends a shortcut's session when its program (this setup) exits, which
; takes a launcher started from here down with it. The finished page says what to run instead.
; shellexec: WZLauncher.exe asks for administrator rights and this setup runs without them, so a plain
; CreateProcess fails with error 740; ShellExecute shows the UAC prompt instead.
Filename: "{app}\WZLauncher.exe"; WorkingDir: "{app}"; Description: "Start the launcher"; Flags: nowait postinstall skipifsilent shellexec; Check: not IsWine

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
Type: files; Name: "{app}\WZLauncher.crc"
Type: dirifempty; Name: "{app}"

[Code]
function IsWine: Boolean;
begin
  Result := RegKeyExists(HKLM, 'Software\Wine') or RegKeyExists(HKCU, 'Software\Wine');
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = wpFinished) and IsWine then
    WizardForm.FinishedLabel.Caption :=
      'The launcher is installed in ' + ExpandConstant('{app}') + '.' + #13#10#13#10 +
      'On Linux (Steam Proton or Wine), start it from there: add WZLauncher.exe from that folder to ' +
      'Steam as a non-Steam game (or point this shortcut at it) and launch that. Setup does not start ' +
      'it here, because Steam closes everything when Setup exits.';
end;

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
