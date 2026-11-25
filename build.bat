@echo off
setlocal
set "CONFIG=%~1"
set "PLATFORM=%~2"
if not defined CONFIG set "CONFIG=Release"
if not defined PLATFORM set "PLATFORM=x64"
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" exit /b 2
if /I not "%PLATFORM%"=="x64" if /I not "%PLATFORM%"=="Win32" exit /b 2
set "MSBUILD="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.v141.x86.x64 -find MSBuild\**\Bin\MSBuild.exe`) do if not defined MSBUILD set "MSBUILD=%%I"
)
if not defined MSBUILD set "MSBUILD=%ProgramFiles(x86)%\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin\MSBuild.exe"
if not exist "%MSBUILD%" set "MSBUILD=%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe"
if not exist "%MSBUILD%" (
  echo Visual Studio 2017 with the v141 C++ toolset is required.
  exit /b 1
)
rem Build product projects.
rem The x64 launcher also needs the matching Win32 helper and DLL.
if /I "%PLATFORM%"=="x64" (
  call :products Win32
  if errorlevel 1 exit /b 1
)
call :products %PLATFORM%
exit /b %ERRORLEVEL%

:products
for %%P in (client launcher accounts catalog server) do (
  "%MSBUILD%" "%~dp0%%P\%%P.vcxproj" /t:Build /m:2 /nologo /verbosity:minimal /p:Configuration=%CONFIG% /p:Platform=%~1
  if errorlevel 1 exit /b 1
)
if /I "%~1"=="Win32" (
  "%MSBUILD%" "%~dp0inject32\inject32.vcxproj" /t:Build /m:2 /nologo /verbosity:minimal /p:Configuration=%CONFIG% /p:Platform=Win32
  if errorlevel 1 exit /b 1
)
exit /b 0
