@echo off
rem Builds WZLauncher.exe (x86, static CRT).
rem Output: out\WZLauncher.exe
setlocal
rem Finds the newest Visual Studio with the C++ tools (2019 or later) unless run from a developer prompt.
if defined VSCMD_VER goto :built_env
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio not found. & exit /b 1)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (echo Visual Studio C++ tools not found. & exit /b 1)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul || exit /b 1
:built_env

set "ROOT=%~dp0"
set "SRC=%ROOT%src"
set "OUT=%ROOT%out"
set "OBJ=%OUT%\obj"
if not exist "%OBJ%" mkdir "%OBJ%"

set SOURCES=
for /r "%SRC%" %%f in (*.cpp) do call set SOURCES=%%SOURCES%% "%%f"

rc /nologo /I "%SRC%" /I "%SRC%\App" /fo "%OBJ%\WZLauncher.res" "%SRC%\App\WZLauncher.rc" || exit /b 1

cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS ^
   /Zi /Fd"%OBJ%\vc.pdb" /I "%SRC%" /Fo"%OBJ%\\" /Fe"%OUT%\WZLauncher.exe" %SOURCES% "%OBJ%\WZLauncher.res" ^
   /link /DEBUG /OPT:REF /OPT:ICF /MAP:"%OUT%\WZLauncher.map" /SUBSYSTEM:WINDOWS user32.lib gdi32.lib advapi32.lib shell32.lib shlwapi.lib ^
   comctl32.lib comdlg32.lib ole32.lib oleaut32.lib uuid.lib wininet.lib msimg32.lib version.lib || exit /b 1

echo Built "%OUT%\WZLauncher.exe"
