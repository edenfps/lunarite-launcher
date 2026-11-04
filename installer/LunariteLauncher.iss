; Lunarite launcher setup. Installs the launcher bundle (out\bundle) into the player's Huxley folder,
; the one with Binaries, and adds shortcuts. Uninstalling removes only the launcher's own files.
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
DefaultDirName={code:DefaultHuxleyDir}
AppendDefaultDirName=no
DirExistsWarning=no
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline
WizardStyle=modern
SetupIconFile={#BundleDir}\LauncherImage\HuxleyTheDistopia.ico
UninstallDisplayIcon={app}\WZLauncher.exe
UninstallDisplayName=Lunarite Launcher
OutputDir=..\out
OutputBaseFilename=LunariteLauncher-win-Setup
Compression=lzma2
SolidCompression=yes

[Messages]
SelectDirLabel3=Setup will install the Lunarite launcher into your Huxley folder.
SelectDirBrowseLabel=Pick the Huxley folder (the one that contains Binaries, HuxleyGame and HuxleyMMOGame), then click Next.

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
Type: files; Name: "{app}\WZLauncher.Log"

[Code]
// The launcher records the game folder in HKCU\Software\Webzen\HuxleyTheDystopia\PATH.
function DefaultHuxleyDir(Param: String): String;
begin
  if not RegQueryStringValue(HKCU, 'Software\Webzen\HuxleyTheDystopia', 'PATH', Result) then
    if not RegQueryStringValue(HKLM32, 'Software\Webzen\HuxleyTheDystopia', 'PATH', Result) then
      Result := ExpandConstant('{autopf32}\Webzen\Huxley');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectDir) and
     not FileExists(AddBackslash(WizardDirValue) + 'Binaries\HuxleyMMOGame.exe') then
  begin
    MsgBox('This folder has no Binaries\HuxleyMMOGame.exe.' + #13#10#13#10 +
           'Pick your Huxley folder (the clean US Huxley client), the one that contains Binaries.',
           mbError, MB_OK);
    Result := False;
  end;
end;
