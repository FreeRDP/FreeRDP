@echo off
if defined freeRdpDir (exit /b)
@echo on

set openSSLTag=openssl-3.0.12
set freeRdpDir=%~dp0\..\..
set buildDir=%freeRdpDir%\Build
set scriptsDir=%~dp0
set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

for /f "usebackq tokens=*" %%i in (`%vswhere% -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
  set msbuild="%%i"
)

for /f "usebackq delims=" %%i in (`%vswhere% -prerelease -latest -property installationPath`) do (
  set vsDir=%%i
)

set vc_ver_short=14.44
set vc_ver=14.44.17.14
set __call_SetupBuildEnvironment=(call "%vsdir%\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=%vc_ver_short%)
%__call_SetupBuildEnvironment%

set __check_VCToolsVersion_short=_
if defined VCToolsVersion (set __check_VCToolsVersion_short=%VCToolsVersion:~0,5%)
if [%ERRORLEVEL%][%__check_VCToolsVersion_short%]==[0][%vc_ver_short%] (
  goto end
)

if [%1]==[install-msvc] (goto install-msvc)

echo =========================== ERROR: ==============================
echo please install 'msvc visual c++ build tools %vc_ver_short%' first
echo run `install_msvc.bat` as admin with vs studio stopped
echo =================================================================
exit /b 111

:install-msvc
echo installing msvc
set vs_installer="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vs_installer.exe"
%vs_installer% modify --installPath "%vsdir%" --add Microsoft.VisualStudio.Component.VC.%vc_ver%.x86.x64 Microsoft.VisualStudio.Component.VC.%vc_ver%.ATL --includeRecommended --quiet --installWhileDownloading

if %ERRORLEVEL% neq 0 (
  echo =========================== ERROR: ==============================
  echo installing msvc failed with exit code %ERRORLEVEL%
  echo you need to run as admin and VisualStudio to be closed
  echo =================================================================
  exit %ERRORLEVEL%
)

echo installed msvc
%__call_SetupBuildEnvironment%

:end
set vs
set vc

