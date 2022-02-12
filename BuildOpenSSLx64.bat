set "START_DIR=%CD%"
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
cd /D "%START_DIR%"

perl Configure VC-WIN64A no-asm --prefix=..\OpenSSL-VC-64
call ms\do_win64a
nmake -f ms\nt.mak
nmake -f ms\nt.mak install
