# FreeRDP: A Remote Desktop Protocol Implementation

FreeRDP is a free implementation of the Remote Desktop Protocol (RDP), released under the Apache license.
Enjoy the freedom of using your software wherever you want, the way you want it, in a world where
interoperability can finally liberate your computing experience.

## Datiscum VAAPI/XShm Optimization Notes (new111)

This fork contains an optimized X11 AVC420/VAAPI output path and audio synchronization changes.
The graphics path keeps FreeRDP in control of the image flow while avoiding the expensive old
hardware-download and color-conversion path where possible.

The optimized graphics path is:

```text
H.264/AVC420 -> VAAPI Decode -> VAAPI VPP -> USER_PTR RGB32 -> GDI/XShm -> XShmPutImage
```

This is not a direct `vaPutSurface`, EGL, or DMABUF render path into the X11 window. A fully
GPU-direct presenter would theoretically be another possible optimization, but it only partially
fits the current FreeRDP architecture. FreeRDP normally controls the image flow through
`SurfaceCommand`, dirty regions, `OutputUpdate`, SmartSizing, RemoteApp, and X11 window handling.
The current approach deliberately remains inside the normal FreeRDP output model and is therefore
more compatible with the existing graphics pipeline.

### Test System

The following measurements were taken on this system:

```text
lscpu
Architecture:                x86_64
CPU op-mode(s):              32-bit, 64-bit
Address sizes:               39 bits physical, 48 bits virtual
Byte Order:                  Little Endian
CPU(s):                      4
On-line CPU(s) list:         0-3
Vendor ID:                   GenuineIntel
Model name:                  Intel(R) N100
```

The resolution used during the test was `3440x1600` pixels.

### Recommended Command Line

```sh
xfreerdp /f +clipboard +grab-keyboard +async-channels +compression /compression-level:5 /multitransport /bpp:32 /gfx:avc420 /gdi:hw /sound:sys:pulse,format:0x704F,rate:48000
```

Recommended graphics options:

```text
/gfx:avc420 /gdi:hw /bpp:32
```

These options are relevant for the optimized AVC420/VAAPI/X11 path.

Recommended audio option:

```text
/sound:sys:pulse,format:0x704F,rate:48000
```

`0x704F` is OPUS. For this mode, xrdp had to be patched because the OPUS codec otherwise works
incorrectly there.

Alternative audio option:

```text
/sound:sys:pulse,format:0xA106
```

`0xA106` is AAC_MS. This works here without xrdp modifications.

### Required Build Option

`xfreerdp` must be built with XShm support enabled:

```sh
cmake -DWITH_XSHM=ON ...
```

### How To Verify The Optimized Graphics Path

If the following log lines appear, the optimized graphics acceleration should be active:

```text
[xf_shm_create_surface_image]: XShm runtime: enabled version=1.2 sharedPixmaps=yes image=3440x1600 bpl=13760 size=22016000
[com.freerdp.client.x11] - [xf_vaapi_vpp_prepare]: VAAPI VPP v41: ready source NV12/VAAPI -> USER_PTR RGB32
```

The first line confirms that MIT-SHM/XShm is active for X11 output. The second line confirms that
the optimized VAAPI-VPP path is active.

The following features must not be enabled for this optimized path:

```text
SmartSizing
MultiTouchGestures
RemoteApp
```

If SmartSizing or MultiTouchGestures are active, the client deliberately falls back to the normal
FreeRDP path. In that case, display output still works, but not through the optimized VAAPI/XShm
path.

If messages such as `not suitable for custom path`, `falling back`, or
`USER_PTR RGB32 target not available` appear, the optimized path is not running.

### CPU Usage Comparison

Note: The following `pidstat` output was generated on a system using a German locale.
`Durchschn.` means `average`.

Optimized build, `xfreerdp_111`:

```text
pidstat -u -t -p 5334 1 30
Durchschn.:   999      5334         -   28,76    5,26    0,00    0,00   34,02     -  xfreerdp_111
Durchschn.:   999         -      5334    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp_111
Durchschn.:   999         -      5335    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp_111
Durchschn.:   999         -      5336    4,60    3,27    0,00    0,23    7,86     -  |__xfreerdp_111
Durchschn.:   999         -      5337    1,30    0,23    0,00    0,07    1,53     -  |__xfreerdp_111
Durchschn.:   999         -      5338    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp_111
Durchschn.:   999         -      5342    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp_111
Durchschn.:   999         -      5343    0,27    0,60    0,00    0,13    0,87     -  |__threaded-ml
Durchschn.:   999         -      5344    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp_111
Durchschn.:   999         -      5345    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp_111
Durchschn.:   999         -      5346   22,36    1,43    0,00    0,13   23,79     -  |__xfreerdp_111
```

Unoptimized comparison build, `xfreerdp`:

```text
pidstat -u -t -p 6463 1 30
Durchschn.:   999      6463         -  101,63   36,05    0,00    0,00  137,69     -  xfreerdp
Durchschn.:   999         -      6463    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp
Durchschn.:   999         -      6464    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp
Durchschn.:   999         -      6465    3,10    1,73    0,00    1,77    4,83     -  |__xfreerdp
Durchschn.:   999         -      6466    0,93    0,13    0,00    0,37    1,07     -  |__xfreerdp
Durchschn.:   999         -      6467    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp
Durchschn.:   999         -      6471    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp
Durchschn.:   999         -      6472    0,20    0,33    0,00    0,70    0,53     -  |__threaded-ml
Durchschn.:   999         -      6473    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp
Durchschn.:   999         -      6474    0,00    0,00    0,00    0,00    0,00     -  |__xfreerdp
Durchschn.:   999         -      6475   19,49   21,39    0,00    6,13   40,89     -  |__xfreerdp
Durchschn.:   999         -      6476   19,29    3,20    0,00    5,26   22,49     -  |__xfreerdp
Durchschn.:   999         -      6477   19,46    3,20    0,00    5,16   22,66     -  |__xfreerdp
Durchschn.:   999         -      6478   19,23    3,30    0,00    5,26   22,53     -  |__xfreerdp
Durchschn.:   999         -      6479   19,46    3,23    0,00    5,06   22,69     -  |__xfreerdp
```

Summary based on `pidstat`:

| Build | CPU usage |
| --- | ---: |
| Before optimization | 137.69% |
| After optimization | 34.02% |

The optimization reduced CPU usage from `137.69%` to `34.02%`. This corresponds to a reduction of
approximately `75%`, or roughly `4x` lower CPU usage. In practice, this frees up around three
quarters of the CPU capacity that was previously required for the same workload.

### Audio Sync Optimization

Audio playback has been optimized to prevent gradual drift between audio and video.
Synchronization adjustments are now applied dynamically during quiet periods or pauses in the audio
stream, helping maintain stable A/V sync without introducing unnecessary artifacts.

### Static Test Build

Debian 13 static test build download:

[Download xfreerdp_111](https://datiscum.de/MyPubDownload/xfreerdp_111)

If all dependencies are installed, it can be tested directly.

### Background

VA-API hardware encoding work for xrdp/xorg-xrdp was started about one year before this test build.
After that worked reasonably well, the remaining client-side CPU load in `xfreerdp` became visible
despite hardware decoding. The optimized VAAPI/XShm path was developed incrementally over the last
few months and has been used continuously during daily work sessions of up to 14 hours on the test
system above.

## Code Quality Status

[![abi-checker](https://github.com/FreeRDP/FreeRDP/actions/workflows/abi-checker.yml/badge.svg)](https://github.com/FreeRDP/FreeRDP/actions/workflows/abi-checker.yml)
[![clang-tidy-review](https://github.com/FreeRDP/FreeRDP/actions/workflows/clang-tidy.yml/badge.svg?event=pull_request_target)](https://github.com/FreeRDP/FreeRDP/actions/workflows/clang-tidy.yml)
[![CodeQL](https://github.com/FreeRDP/FreeRDP/actions/workflows/codeql-analysis.yml/badge.svg?branch=master)](https://github.com/FreeRDP/FreeRDP/actions/workflows/codeql-analysis.yml)
[![mingw-builder](https://github.com/FreeRDP/FreeRDP/actions/workflows/mingw.yml/badge.svg)](https://github.com/FreeRDP/FreeRDP/actions/workflows/mingw.yml)
[![macos-builder](https://github.com/FreeRDP/FreeRDP/actions/workflows/macos.yml/badge.svg)](https://github.com/FreeRDP/FreeRDP/actions/workflows/macos.yml)
[![[arm,ppc,ricsv] architecture builds](https://github.com/FreeRDP/FreeRDP/actions/workflows/alt-architectures.yml/badge.svg)](https://github.com/FreeRDP/FreeRDP/actions/workflows/alt-architectures.yml)
[![[freebsd] architecture builds](https://github.com/FreeRDP/FreeRDP/actions/workflows/freebsd.yml/badge.svg)](https://github.com/FreeRDP/FreeRDP/actions/workflows/freebsd.yml)
[![coverity](https://scan.coverity.com/projects/616/badge.svg)](https://scan.coverity.com/projects/freerdp)

## Resources

Project website: https://www.freerdp.com/

Issue tracker: https://github.com/FreeRDP/FreeRDP/issues

Sources: https://github.com/FreeRDP/FreeRDP/

Downloads: https://pub.freerdp.com/releases/

Wiki: https://github.com/FreeRDP/FreeRDP/wiki

API documentation: https://pub.freerdp.com/api/

Security policy: https://github.com/FreeRDP/FreeRDP/security/policy

FAQ: https://github.com/FreeRDP/FreeRDP/wiki/FAQ

### Contact

* Matrix room : `#FreeRDP:matrix.org` (main)
  * IRC channel : `#freerdp @ irc.oftc.net` (bridged)
* Mailing list: https://lists.sourceforge.net/lists/listinfo/freerdp-devel

## Microsoft Open Specifications

Information regarding the Microsoft Open Specifications can be found at:
https://www.microsoft.com/openspecifications/

A list of reference documentation is maintained here:
https://github.com/FreeRDP/FreeRDP/wiki/Reference-Documentation

## Compilation

Instructions on how to get started compiling FreeRDP can be found on the wiki:
https://github.com/FreeRDP/FreeRDP/wiki/Compilation
