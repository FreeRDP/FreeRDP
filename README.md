## UiPath fork of FreeRDP

[UiPath/Driver](https://github.com/UiPath/Driver) has this fork as a static library dependency.

### Build instructions

#### Build OpenSSL (dependency for FreeRDP)
* Visual Studio 2022 installed in `C:\Program Files` required.  For different VS versions change the path in the build scripts.
* Install [CMake](https://cmake.org/download).  Make sure the `cmake` command is in PATH.
* Clone [OpenSSL](https://github.com/openssl/openssl) to `..\openssl`.
```
cd ..
git clone https://github.com/openssl/openssl
```
* Checkout tag `OpenSSL_1_0_2u`.
```
git checkout OpenSSL_1_0_2u
```
* Run x86 build script.  It should generate the build to `..\OpenSSL-VC-32`.
```
..\FreeRDP\BuildOpenSSLx86.bat
```
* The build system is messy and leaves intermediary x86 build files in the repo directory.  Clean these before building x64.
```
git clean -xdff
```
* Run x64 build script.  It should generate the build to `..\OpenSSL-VC-64`.
```
..\FreeRDP\BuildOpenSSLx64.bat
```

#### Build FreeRDP.
* Install [StrawberryPerl](http://strawberryperl.com).  Make sure the `perl` command is in PATH.
* Use CMake to generate Visual Studio 2022 solutions.  The cmake invocation is in the `BuildFreeRDP*.bat` files.
```
.\BuildFreeRDPx86.bat
.\BuildFreeRDPx64.bat
```
* The solutions are generated in `.\Build\x86\` and `.\Build\x64\` directories.  Build these for target Release.
