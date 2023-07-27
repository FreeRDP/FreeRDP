## UiPath fork of FreeRDP

This repo forks the FreeRDP repo, thus allowing us to make changes that fit our needs and augment the codebase with other components.
From time to time there is a need to merge the changes from the original repo into this one.

### ❗When updating the FreeRDP library from the official repo, please update the following in this file (README.md):

|**Original repository tag/branch used:**| `2.5.0` |
| --- | --- |
|**Original corresponding commit hash:**| `d50aef95520df4216c638495a6049125c00742cb` |


### Build instructions
* Visual Studio 2022 installed in `C:\Program Files` required.  

#### 
* Install [StrawberryPerl](http://strawberryperl.com).  Make sure the `perl` command is in PATH.
  You may try on newer Windows 10:
```
winget install -e --id StrawberryPerl.StrawberryPerl
```

#### Build FreeRDP and Build OpenSSL (dependency for FreeRDP)

* Steps
** Clone [OpenSSL](https://github.com/openssl/openssl) to `..\openssl` && Checkout tag `OpenSSL_1_0_2u` (getOpenSsl)
** Generate OpenSSL build to `..\OpenSSL-VC-64`.  (buildOpenSsl)
** Use CMake to generate and then build Visual Studio 2022 solutions.  (BuildFreeRDP)
** The freerdp solution is generated in `.\Build\x64\` directories.

* Scripts
** Simple
```
cd .ci/Scripts
.\PrepareFreeRdpDev
```
** or detailed
```
cd .ci/Scripts
.\getOpenSsl
.\buildOpenSsl
.\buildFreeRDP Debug
```

### Work on the FreeRdpClient
* Open [UiPath.FreeRdpClient/UiPath.FreeRdpClient.sln](file://UiPath.FreeRdpClient/UiPath.FreeRdpClient.sln)
* To test with a nugetRef instead of projectRef edit the [UiPath.FreeRdp.Tests.csproj](file://UiPath.FreeRdpClient/UiPath.FreeRdpClient.Tests/UiPath.FreeRdp.Tests.csproj)
search for: `<When Condition="'$(UseNugetRef)'!=''">`

### Our current baseline from the original FreeRDP repo

Check this [wiki page](./UiPath.FreeRdpClient/README.md)