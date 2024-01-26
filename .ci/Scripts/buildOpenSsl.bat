call %~dp0\getvars.bat
pushd .
cd %freeRdpDir%\..\OpenSSL
git clean -xdff

perl Configure VC-WIN64A no-asm no-shared no-module --prefix=%~dp0\..\..\..\OpenSSL-VC-64
nmake
nmake install_dev
popd