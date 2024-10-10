#
# spec file for package freerdp-nightly
#
# Copyright (c) 2015 Bernhard Miklautz <bernhard.miklautz@shacknet.at>
#
# Bugs and comments https://github.com/FreeRDP/FreeRDP/issues

%define _build_id_links none
%define   INSTALL_PREFIX /opt/freerdp-nightly/

# do not add provides for libs provided by this package
# or it could possibly mess with system provided packages
# which depend on freerdp libs
%global __provides_exclude_from ^%{INSTALL_PREFIX}.*$

# do not require our own libs
%global __requires_exclude ^(libfreerdp.*|libwinpr.*|librdtk.*|libuwac.*).*$

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

# (Open)Suse
%if %{defined suse_version}
BuildRequires: libswscale-devel
BuildRequires: cJSON-devel
BuildRequires: uuid-devel
BuildRequires: libSDL2-devel
BuildRequires: libSDL2_ttf-devel
BuildRequires: libSDL2_image-devel
BuildRequires: pkg-config
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

%if 0%{defined rhel}
BuildRequires: cjson-devel
BuildRequires: uuid-devel
BuildRequires: opus-devel
BuildRequires: SDL2-devel
BuildRequires: SDL2_ttf-devel
BuildRequires: SDL2_image-devel
BuildRequires: pkgconfig
BuildRequires: openssl-devel
BuildRequires: alsa-lib-devel
BuildRequires: pulseaudio-libs-devel
BuildRequires: libusbx-devel
BuildRequires: systemd-devel
BuildRequires: dbus-glib-devel
BuildRequires: libjpeg-turbo-devel
BuildRequires: libasan
BuildRequires: libjpeg-turbo-devel
BuildRequires: wayland-devel
%endif

# fedora 21+
%if 0%{?fedora} >= 37
BuildRequires: cjson-devel
BuildRequires: uuid-devel
BuildRequires: opus-devel
BuildRequires: SDL2-devel
BuildRequires: SDL2_ttf-devel
BuildRequires: SDL2_image-devel
BuildRequires: pkgconfig
BuildRequires: openssl-devel
BuildRequires: alsa-lib-devel
BuildRequires: pulseaudio-libs-devel
BuildRequires: libusbx-devel
BuildRequires: systemd-devel
BuildRequires: dbus-glib-devel
BuildRequires: libjpeg-turbo-devel
BuildRequires: libasan
BuildRequires: webkit2gtk4.0-devel
BuildRequires: libjpeg-turbo-devel
BuildRequires: wayland-devel
%endif

%if 0%{?fedora} >= 36 || 0%{?rhel} >= 8
BuildRequires: (ffmpeg-free-devel or ffmpeg-devel)
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
cd %{_topdir}/BUILD
cp %{_topdir}/SOURCES/source_version freerdp-nightly-%{version}/.source_version

%build

%cmake  -DCMAKE_SKIP_RPATH=FALSE \
        -DCMAKE_SKIP_INSTALL_RPATH=FALSE \
        -DWITH_FREERDP_DEPRECATED_COMMANDLINE=ON \
        -DWITH_PULSE=ON \
        -DWITH_CHANNELS=ON \
        -DWITH_CUPS=ON \
        -DWITH_PCSC=ON \
        -DWITH_JPEG=ON \
        -DWITH_OPUS=ON \
        -DSDL_USE_COMPILED_RESOURCES=OFF \
        -DWITH_SDL_IMAGE_DIALOGS=ON \
        -DWITH_BINARY_VERSIONING=ON \
        -DRDTK_FORCE_STATIC_BUILD=ON \
        -DUWAC_FORCE_STATIC_BUILD=ON \
%if 0%{?fedora} >= 36 || 0%{?rhel} >= 9 || 0%{?suse_version}
        -DWITH_FFMPEG=ON \
        -DWITH_DSP_FFMPEG=ON \
%endif
%if 0%{?rhel} <= 8
        -DALLOW_IN_SOURCE_BUILD=ON \
%endif
%if 0%{?rhel} >= 8 || 0%{defined suse_version}
        -DWITH_WEBVIEW=OFF \
%endif
	-DCMAKE_C_COMPILER=clang \
	-DCMAKE_CXX_COMPILER=clang++ \
        -DWITH_SANITIZE_ADDRESS=OFF \
        -DWITH_KRB5=ON \
        -DCHANNEL_URBDRC=ON \
        -DCHANNEL_URBDRC_CLIENT=ON \
        -DWITH_SERVER=ON \
        -DWITH_CAIRO=ON \
        -DBUILD_TESTING=OFF \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=%{INSTALL_PREFIX} \
        -DCMAKE_INSTALL_LIBDIR=%{_lib}

%cmake_build

%install

%cmake_install

find %{buildroot} -name "*.a" -delete
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
%dir %{INSTALL_PREFIX}/share/FreeRDP3
%dir %{INSTALL_PREFIX}/share/FreeRDP3/fonts
%dir %{INSTALL_PREFIX}/share/FreeRDP3/images
%dir %{INSTALL_PREFIX}/%{_lib}/freerdp3/proxy/
%{INSTALL_PREFIX}/%{_lib}/*.so.*
%{INSTALL_PREFIX}/%{_lib}/freerdp3/proxy/*.so
%{INSTALL_PREFIX}/bin/*
%{INSTALL_PREFIX}/share/man/man1/*
%{INSTALL_PREFIX}/share/man/man7/*
%{INSTALL_PREFIX}/share/FreeRDP3/fonts/*
%{INSTALL_PREFIX}/share/FreeRDP3/images/*

%files devel
%defattr(-,root,root)
%{INSTALL_PREFIX}/%{_lib}/*.so
%{INSTALL_PREFIX}/include/
%{INSTALL_PREFIX}/%{_lib}/pkgconfig/
%{INSTALL_PREFIX}/%{_lib}/cmake/

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%changelog
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
