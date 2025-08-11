## UiPath fork of FreeRDP

This repo forks the FreeRDP repo, thus allowing us to make changes that fit our needs and augment the codebase with other components.
From time to time there is a need to merge the changes from the original repo into this one.

### ❗When updating the FreeRDP library from the official repo, please update the following in this file (README.md):

|**Original repository tag/branch used:**| `3.16.0` |
| --- | --- |
|**Original corresponding commit hash:**| `fcdf4c6c7e7ffda9203889114a2ee7ba8d5d1976` |


### Build instructions
* Visual Studio 2022 installed in `C:\Program Files` required.

* Install [StrawberryPerl](http://strawberryperl.com).  Make sure the `perl` command is in PATH.
  You may try on newer Windows 10:
```
    winget install -e --id StrawberryPerl.StrawberryPerl
```

#### Build FreeRDP and Build OpenSSL (dependency for FreeRDP)

> Use a developer console for VS 2022 instead of normal PowerShell or CMD, the commands require `nmake`

* Steps
  * Clone [OpenSSL](https://github.com/openssl/openssl) to `..\openssl` && Checkout tag `OpenSSL_1_0_2u` (getOpenSsl)
  * Generate OpenSSL build to `..\OpenSSL-VC-64`.  (buildOpenSsl)
  * Use CMake to generate and then build Visual Studio 2022 solutions.  (BuildFreeRDP)
  * The freerdp solution is generated in `.\Build\x64\` directories.

* Scripts
  * Simple
    ```
    cd .ci/Scripts
    .\PrepareFreeRdpDev
    ```
  * or detailed
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

#### Running unit tests

* Run Visual Studio as Admin
* Make sure RDP is enabled on local machine - `View advanced system settings > Remote tab > Allow remote connections to this computer`
