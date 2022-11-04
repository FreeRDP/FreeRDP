@echo off
set openSSLTag=OpenSSL_1_0_2u

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

call "%vsdir%\Common7\Tools\vsdevcmd.bat"
