@echo off
setlocal

echo Outloud TTS SAPI5 Build
echo.

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Please install Visual Studio 2022 or later.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -products * -latest -property installationPath`) do (
    set "VSINSTALLDIR=%%i\"
)

if not defined VSINSTALLDIR (
    echo ERROR: Visual Studio installation not found.
    exit /b 1
)

echo Found Visual Studio at: %VSINSTALLDIR%
echo.

echo Building x86 targets...
cmake -A Win32 -S . -B build_x86
if errorlevel 1 exit /b 1
cmake --build build_x86 --config Release
if errorlevel 1 exit /b 1

echo Building x64 targets...
cmake -A x64 -S . -B build_x64
if errorlevel 1 exit /b 1
cmake --build build_x64 --config Release
if errorlevel 1 exit /b 1

echo Staging output layout...
powershell -NoProfile -ExecutionPolicy Bypass -File installer\stage.ps1
if errorlevel 1 exit /b 1

echo Building installer...
set "ISCC=%LocalAppData%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo ERROR: Inno Setup 6 compiler not found.
    exit /b 1
)
"%ISCC%" /O"output" installer\outloud.iss
if errorlevel 1 exit /b 1

echo.
echo Build completed. Installer: output\OutloudSAPI_Setup.exe
endlocal
