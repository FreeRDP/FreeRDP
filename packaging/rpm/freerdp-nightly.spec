#
# spec file for package freerdp-nightly
#
# Copyright (c) 2015 Bernhard Miklautz <bernhard.miklautz@shacknet.at>
#
# Bugs and comments https://github.com/FreeRDP/FreeRDP/issues

%if 0%{?rhel} && 0%{?rhel} <= 8
%global __cmake_builddir .
%else
%global __cmake_builddir 1
%endif

%define _build_id_links none
%define   INSTALL_PREFIX /opt/freerdp-nightly/

# this block is for turning on certain features based on the distribution and version
%if 0%{?fedora} || 0%{?suse_version} || 0%{?rhel} > 9
%bcond_without av1
%else
%bcond_with av1
%endif

%if 0%{?fedora}
%bcond_without webview
%else
%bcond_with webview
%endif

%if 0%{?fedora} || 0%{?rhel} > 8
%bcond_without openh264
%else
%bcond_with openh264
%endif

%if 0%{?fedora} || 0%{?suse_version} >= 1600 || 0%{?rhel} > 9
%bcond_without rdpewa
%else
%bcond_with rdpewa
%endif

# RHEL 10 EPEL doesn't have libyuv for some reason
%if 0%{?fedora} || 0%{?suse_version} || (0%{?rhel} && 0%{?rhel} < 10)
%bcond_without yuv
%else
%bcond_with yuv
%endif

# do not add provides for libs provided by this package
# or it could possibly mess with system provided packages
# which depend on freerdp libs
%global __provides_exclude_from ^%{INSTALL_PREFIX}.*$

# do not require our own libs
%global __requires_exclude ^(libfreerdp.*|libwinpr.*|librdtk.*|libuwac.*).*$

# no debug package
%global debug_package %{nil}

Name:           freerdp-nightly
Version:        3.0
Release:        0
License:        ASL 2.0
Summary:        Free implementation of the Remote Desktop Protocol (RDP)
Url:            http://www.freerdp.com
Group:          Productivity/Networking/Other
Source0:        %{name}-%{version}.tar.bz2
Source1:        source_version
BuildRequires: clang
BuildRequires: cmake >= 3.13.0
BuildRequires: libxkbfile-devel
BuildRequires: libX11-devel
BuildRequires: libXrandr-devel
BuildRequires: libXi-devel
BuildRequires: libXrender-devel
BuildRequires: libXext-devel
BuildRequires: libXinerama-devel
BuildRequires: libXfixes-devel
BuildRequires: libXcursor-devel
BuildRequires: libXv-devel
BuildRequires: libXdamage-devel
BuildRequires: libXtst-devel
BuildRequires: cups-devel
BuildRequires: cairo-devel
BuildRequires: pcsc-lite-devel
BuildRequires: zlib-devel
BuildRequires: krb5-devel
BuildRequires: uriparser-devel
BuildRequires: libpng-devel
BuildRequires: libwebp-devel
BuildRequires: fuse3-devel
BuildRequires: pam-devel
BuildRequires: libicu-devel
BuildRequires: libv4l-devel
BuildRequires: pkgconfig(soxr)
BuildRequires: pkgconfig(gstreamer-1.0)
BuildRequires: pkgconfig(gstreamer-base-1.0)
BuildRequires: pkgconfig(gstreamer-video-1.0)
BuildRequires: pkgconfig(gstreamer-app-1.0)

%if %{with yuv}
%if %{defined suse_version}
BuildRequires: pkgconfig(libyuv)
%else
BuildRequires: libyuv-devel
%endif
%endif

%if %{with av1}
BuildRequires: pkgconfig(aom) >= 2.0
%endif

%if %{with rdpewa}
BuildRequires: libcbor-devel >= 0.10.2
BuildRequires: libfido2-devel >= 1.14.0
%endif

%if 0%{?fedora} || 0%{?suse_version} || 0%{?rhel} > 8
BuildRequires: ocl-icd-devel
BuildRequires: opencl-headers
%endif

%if %{with webview}
BuildRequires: (webkitgtk6.0-devel or webkit2gtk4.1-devel or webkit2gtk4.0-devel)
%endif

# (Open)Suse
%if %{defined suse_version}
BuildRequires: pkg-config
BuildRequires: libswscale-devel
BuildRequires: cJSON-devel
BuildRequires: uuid-devel
%if 0%{?suse_version} >= 1600
BuildRequires: SDL3-devel
BuildRequires: SDL3_ttf-devel
BuildRequires: SDL3_image-devel
%else
BuildRequires: SDL2-devel
BuildRequires: SDL2_ttf-devel
BuildRequires: SDL2_image-devel
%endif
BuildRequires: libopenssl-devel
BuildRequires: alsa-devel
BuildRequires: libpulse-devel
BuildRequires: libusb-1_0-devel
BuildRequires: libudev-devel
BuildRequires: dbus-1-glib-devel
BuildRequires: wayland-devel
BuildRequires: libavutil-devel
BuildRequires: libavcodec-devel
BuildRequires: libavformat-devel
BuildRequires: libswresample-devel
BuildRequires: libopus-devel
BuildRequires: libjpeg62-devel
%endif

# fedora and rhel
%if 0%{?fedora} || 0%{defined rhel}
BuildRequires: pkgconfig
BuildRequires: cjson-devel
BuildRequires: uuid-devel
BuildRequires: opus-devel
BuildRequires: openssl-devel
BuildRequires: alsa-lib-devel
BuildRequires: pulseaudio-libs-devel
BuildRequires: libusbx-devel
BuildRequires: systemd-devel
BuildRequires: dbus-glib-devel
BuildRequires: libjpeg-turbo-devel
BuildRequires: libasan
BuildRequires: compiler-rt
BuildRequires: libjpeg-turbo-devel
BuildRequires: wayland-devel
%endif

%if 0%{?fedora} || 0%{?rhel} >= 9
BuildRequires: (ffmpeg-free-devel or ffmpeg-devel)
%endif

# RHEL 8 does not have SDL 2 or 3
%if 0%{?fedora} || 0%{?rhel} > 8
BuildRequires: ((SDL3-devel and SDL3_ttf-devel and SDL3_image-devel) or (SDL2-devel and SDL2_ttf-devel and SDL2_image-devel))
BuildRequires: (fdk-aac-devel or fdk-aac-free-devel)
%if %{with openh264}
BuildRequires: (noopenh264-devel or openh264-devel)
%endif
%endif

BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
FreeRDP is a open and free implementation of the Remote Desktop Protocol (RDP).
This package provides nightly master builds of all components.

%package devel
Summary:        Development Files for %{name}
Group:          Development/Libraries/C and C++
Requires:       %{name} = %{version}

%description devel
This package contains development files necessary for developing applications
based on freerdp and winpr.

%prep
%setup -q
mkdir -p %{_builddir}
cd %{_builddir}
cp %{_sourcedir}/source_version freerdp-nightly-%{version}/.source_version

%build

%cmake \
    -DCMAKE_VERBOSE_MAKEFILE=ON \
    -DCMAKE_SKIP_RPATH=FALSE \
    -DCMAKE_SKIP_INSTALL_RPATH=FALSE \
    -DWITH_FREERDP_DEPRECATED_COMMANDLINE=ON \
    -DWITH_ALSA=ON \
    -DWITH_PULSE=ON \
    -DWITH_CHANNELS=ON \
    -DWITH_AAD=ON \
    -DWITH_CUPS=ON \
    -DWITH_PCSC=ON \
    -DWITH_JPEG=ON \
%if %{with yuv}
    -DWITH_YUV=ON \
%endif
    -DWITH_OPUS=ON \
%if %{with openh264}
    -DWITH_OPENH264=ON \
    -DWITH_OPENH264_LOADING=ON \
%endif
%if %{with av1}
    -DWITH_GFX_AV1=ON \
%endif
    -DWITH_INTERNAL_RC4=ON \
    -DWITH_INTERNAL_MD4=ON \
    -DWITH_INTERNAL_MD5=ON \
    -DWITH_KEYBOARD_LAYOUT_FROM_FILE=ON \
    -DWITH_TIMEZONE_FROM_FILE=ON \
    -DSDL_USE_COMPILED_RESOURCES=OFF \
    -DWITH_SDL_IMAGE_DIALOGS=ON \
    -DWITH_BINARY_VERSIONING=ON \
    -DWITH_RESOURCE_VERSIONING=ON \
    -DWINPR_USE_VENDOR_PRODUCT_CONFIG_DIR=ON \
    -DFREERDP_USE_VENDOR_PRODUCT_CONFIG_DIR=ON \
    -DSAMPLE_USE_VENDOR_PRODUCT_CONFIG_DIR=ON \
    -DSDL_USE_VENDOR_PRODUCT_CONFIG_DIR=ON \
    -DWINPR_USE_LEGACY_RESOURCE_DIR=OFF \
    -DRDTK_FORCE_STATIC_BUILD=ON \
    -DUWAC_FORCE_STATIC_BUILD=ON \
%if 0%{?fedora} || 0%{?rhel} > 8
    -DWITH_FDK_AAC=ON \
%endif
%if 0%{?fedora} || 0%{?rhel} >= 9 || 0%{?suse_version}
    -DWITH_FFMPEG=ON \
    -DWITH_VIDEO_FFMPEG=ON \
    -DWITH_DSP_FFMPEG=ON \
    -DWITH_SWSCALE=ON \
    -DWITH_VAAPI=ON \
    -DWITH_SOXR=ON \
    -DWITH_GSTREAMER_1_0=ON \
%else
    -DWITH_FFMPEG=OFF \
    -DWITH_VIDEO_FFMPEG=OFF \
    -DWITH_DSP_FFMPEG=OFF \
    -DWITH_SWSCALE=OFF \
    -DWITH_VAAPI=OFF \
%endif
%if 0%{?fedora} || 0%{?suse_version} || 0%{?rhel} > 8
    -DWITH_OPENCL=ON \
%endif
%if 0%{?rhel} && 0%{?rhel} <= 8
    -DALLOW_IN_SOURCE_BUILD=ON \
%endif
%if %{with webview}
    -DWITH_WEBVIEW=ON \
%else
    -DWITH_WEBVIEW=OFF \
%endif
%if 0%{?fedora} == 41 || (0%{?suse_version} && 0%{?suse_version} < 1600)
    -DWITH_CLIENT_SDL3=OFF \
%endif
%if 0%{?fedora} > 41 || 0%{?suse_version} >= 1600 || 0%{?rhel} > 9
    -DWITH_CLIENT_SDL2=OFF \
%endif
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DWITH_SANITIZE_ADDRESS=ON \
    -DWITH_KRB5=ON \
    -DWITH_CLIENT_INTERFACE=ON \
    -DCLIENT_INTERFACE_SHARED=ON \
    -DWITH_SMARTCARD_INSPECT=ON \
    -DWITH_WINPR_TOOLS=ON \
    -DWITH_SAMPLE=ON \
    -DCHANNEL_URBDRC=ON \
    -DCHANNEL_URBDRC_CLIENT=ON \
    -DCHANNEL_GFXREDIR=ON \
    -DCHANNEL_GFXREDIR_SERVER=ON \
    -DCHANNEL_RDP2TCP=ON \
    -DCHANNEL_RDP2TCP_CLIENT=ON \
    -DCHANNEL_RDPECAM=ON \
    -DCHANNEL_RDPECAM_CLIENT=ON \
    -DCHANNEL_RDPEAR=ON \
    -DCHANNEL_RDPEAR_CLIENT=ON \
%if %{with rdpewa}
    -DCHANNEL_RDPEWA=ON \
    -DCHANNEL_RDPEWA_CLIENT=ON \
%endif
    -DCHANNEL_SSHAGENT=ON \
    -DCHANNEL_SSHAGENT_CLIENT=ON \
    -DCHANNEL_TSMF=ON \
    -DCHANNEL_TSMF_CLIENT=ON \
    -DWITH_SERVER=ON \
    -DWITH_CAIRO=ON \
    -DBUILD_TESTING=ON \
    -DBUILD_TESTING_NO_H264=ON \
    -DCMAKE_CTEST_ARGUMENTS="-DExperimentalTest;--output-on-failure;--no-compress-output" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-O1" \
    -DCMAKE_CXX_FLAGS="-O1" \
    -DCMAKE_INSTALL_PREFIX=%{INSTALL_PREFIX} \
    -DCMAKE_INSTALL_LIBDIR=%{_lib}

%cmake_build

%if 0%{?suse_version}
cd ..
%endif

%ctest

%cmake_install

export NO_BRP_CHECK_RPATH true

%files
%defattr(-,root,root)
%dir %{INSTALL_PREFIX}
%dir %{INSTALL_PREFIX}/%{_lib}
%dir %{INSTALL_PREFIX}/bin
%dir %{INSTALL_PREFIX}/share/
%dir %{INSTALL_PREFIX}/share/man/
%dir %{INSTALL_PREFIX}/share/man/man1
%dir %{INSTALL_PREFIX}/share/man/man7
%dir %{INSTALL_PREFIX}/share/FreeRDP/FreeRDP3
%dir %{INSTALL_PREFIX}/%{_lib}/freerdp3/proxy/
%{INSTALL_PREFIX}/%{_lib}/*.so.*
%{INSTALL_PREFIX}/%{_lib}/freerdp3/proxy/*.so
%{INSTALL_PREFIX}/bin/*
%{INSTALL_PREFIX}/share/man/man1/*
%{INSTALL_PREFIX}/share/man/man7/*
%{INSTALL_PREFIX}/share/FreeRDP/FreeRDP3/*

%files devel
%defattr(-,root,root)
%{INSTALL_PREFIX}/%{_lib}/*.so
%{INSTALL_PREFIX}/include/
%{INSTALL_PREFIX}/%{_lib}/pkgconfig/
%{INSTALL_PREFIX}/%{_lib}/cmake/

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%changelog
* Thu May 15 2025 FreeRDP Team <team@freerdp.com> - 3.15.0-1
- update dependencies, fix build paths
* Thu Oct 10 2024 FreeRDP Team <team@freerdp.com> - 3.10.0-1
- update resource locations, utilize new settings
* Wed Apr 10 2024 FreeRDP Team <team@freerdp.com> - 3.0.0-5
- Fix exclusion of libuwac and librdtk
* Fri Feb 09 2024 FreeRDP Team <team@freerdp.com> - 3.0.0-4
- Deactivate ASAN due to issues with clang
* Fri Feb 09 2024 FreeRDP Team <team@freerdp.com> - 3.0.0-3
- Fix dependencies for alma, suse and rhel
* Thu Dec 21 2023 FreeRDP Team <team@freerdp.com> - 3.0.0-2
- Add new manpages
- Use new CMake options
* Wed Dec 20 2023 FreeRDP Team <team@freerdp.com> - 3.0.0-2
- Exclude libuwac and librdtk
- Allow ffmpeg-devel or ffmpeg-free-devel as dependency
* Tue Dec 19 2023 FreeRDP Team <team@freerdp.com> - 3.0.0-1
- Disable build-id
- Update build dependencies
* Wed Nov 15 2023 FreeRDP Team <team@freerdp.com> - 3.0.0-0
- Update build dependencies
* Wed Feb 7 2018 FreeRDP Team <team@freerdp.com> - 2.0.0-0
- Update version information and support for OpenSuse 42.1
* Tue Feb 03 2015 FreeRDP Team <team@freerdp.com> - 1.2.1-0
- Update version information
* Fri Jan 23 2015 Bernhard Miklautz <bmiklautz+freerdp@shacknet.at> - 1.2.0-0
- Initial version
