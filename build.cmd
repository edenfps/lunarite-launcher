@echo off
rem Builds WZLauncher.exe (x86, static CRT).
rem Output: out\WZLauncher.exe
rem
rem Windows XP support needs the XP toolset: Visual Studio 2017 or later with the components
rem "C++ Windows XP Support for VS 2017 (v141) tools", "C++ ATL for v141 build tools" and the
rem Windows 10 SDK 10.0.10240 (the last Universal CRT that runs on XP). Without them the build
rem still works, for Vista and later only.
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo Visual Studio not found. & exit /b 1)
set "XPSDK=%ProgramFiles(x86)%\Microsoft SDKs\Windows\v7.1A"
set "XPFLAGS="
set "XPSUB="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.WinXP -property installationPath`) do set "VSXP=%%i"
if defined VSXP if exist "%XPSDK%\Include" if exist "%ProgramFiles(x86)%\Windows Kits\10\Lib\10.0.10240.0\ucrt" goto :xp_env
if defined VSCMD_VER goto :built_env
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (echo Visual Studio C++ tools not found. & exit /b 1)
echo XP toolset not found: building for Vista and later.
call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul || exit /b 1
goto :built_env

:xp_env
echo Building with the XP toolset (Windows XP and later).
call "%VSXP%\VC\Auxiliary\Build\vcvarsall.bat" x86 10.0.10240.0 -vcvars_ver=14.16 >nul || exit /b 1
set "INCLUDE=%XPSDK%\Include;%INCLUDE%"
set "LIB=%XPSDK%\Lib;%LIB%"
set "PATH=%XPSDK%\Bin;%PATH%"
set "XPFLAGS=/D_USING_V110_SDK71_ /D_WIN32_WINNT=0x0501 /DWINVER=0x0501"
set "XPSUB=,5.01"

:built_env
set "ROOT=%~dp0"
set "SRC=%ROOT%src"
set "OUT=%ROOT%out"
set "OBJ=%OUT%\obj"
if not exist "%OBJ%" mkdir "%OBJ%"

set SOURCES=
for /r "%SRC%" %%f in (*.cpp) do call set SOURCES=%%SOURCES%% "%%f"

rc /nologo /I "%SRC%" /I "%SRC%\App" /fo "%OBJ%\WZLauncher.res" "%SRC%\App\WZLauncher.rc" || exit /b 1

cl /nologo /EHsc /std:c++17 /O2 /MT /W3 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS %XPFLAGS% ^
   /Zi /Fd"%OBJ%\vc.pdb" /I "%SRC%" /Fo"%OBJ%\\" /Fe"%OUT%\WZLauncher.exe" %SOURCES% "%OBJ%\WZLauncher.res" ^
   /link /DEBUG /OPT:REF /OPT:ICF /MAP:"%OUT%\WZLauncher.map" /SUBSYSTEM:WINDOWS%XPSUB% user32.lib gdi32.lib advapi32.lib shell32.lib shlwapi.lib ^
   comctl32.lib comdlg32.lib ole32.lib oleaut32.lib uuid.lib wininet.lib msimg32.lib version.lib || exit /b 1

echo Built "%OUT%\WZLauncher.exe"
