#
# spec file for package freerdp-nightly
#
# Copyright (c) 2015 Bernhard Miklautz <bernhard.miklautz@shacknet.at>
#
# Bugs and comments https://github.com/FreeRDP/FreeRDP/issues


%define   INSTALL_PREFIX /opt/freerdp-nightly/

# do not add provides for libs provided by this package
# or it could possibly mess with system provided packages
# which depend on freerdp libs
%global __provides_exclude_from ^%{INSTALL_PREFIX}.*$

# do not require our own libs
%global __requires_exclude ^(libfreerdp.*|libwinpr).*$

Name:           freerdp-nightly
Version:        3.0
Release:        0
License:        ASL 2.0
Summary:        Free implementation of the Remote Desktop Protocol (RDP)
Url:            http://www.freerdp.com
Group:          Productivity/Networking/Other
Source0:        %{name}-%{version}.tar.bz2
Source1:        source_version
BuildRequires:   gcc-c++
BuildRequires:  cmake >= 2.8.12
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
BuildRequires: uuid-devel
BuildRequires: libxml2-devel
BuildRequires: zlib-devel
BuildRequires: krb5-devel

# (Open)Suse
%if %{defined suse_version}
BuildRequires: docbook-xsl-stylesheets
BuildRequires: libxslt-tools
BuildRequires: pkg-config
BuildRequires: libopenssl-devel
BuildRequires: alsa-devel
BuildRequires: libpulse-devel
BuildRequires: libusb-1_0-devel
BuildRequires: libudev-devel
BuildRequires: dbus-1-glib-devel
BuildRequires: wayland-devel
BuildRequires: libjpeg-devel
BuildRequires: libavutil-devel
BuildRequires: libavcodec-devel
BuildRequires: libswresample-devel
%endif
# fedora 21+
%if 0%{?fedora} >= 21 || 0%{?rhel} >= 7
BuildRequires: docbook-style-xsl
BuildRequires: libxslt
BuildRequires: pkgconfig
BuildRequires: openssl-devel
BuildRequires: alsa-lib-devel
BuildRequires: pulseaudio-libs-devel
BuildRequires: libusbx-devel
BuildRequires: systemd-devel
BuildRequires: dbus-glib-devel
BuildRequires: libjpeg-turbo-devel
%endif 

%if 0%{?fedora} >= 33
BuildRequires: wayland-devel
%endif

%if 0%{?rhel} >= 8
BuildRequires: libwayland-client-devel
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
        -DWITH_PULSE=ON \
        -DWITH_CHANNELS=ON \
        -DWITH_CUPS=ON \
        -DWITH_PCSC=ON \
        -DWITH_JPEG=ON \
%if %{defined suse_version}
        -DWITH_FFMPEG=ON \
        -DWITH_DSP_FFMPEG=ON \
%endif
%if 0%{?fedora} < 21 || 0%{?rhel} < 8
        -DWITH_WAYLAND=OFF \
%endif
        -DWITH_GSSAPI=OFF \
        -DCHANNEL_URBDRC=ON \
        -DCHANNEL_URBDRC_CLIENT=ON \
        -DWITH_SERVER=ON \
        -DWITH_CAIRO=ON \
        -DBUILD_TESTING=OFF \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=%{INSTALL_PREFIX} \
%if %{defined suse_version}
        -DCMAKE_NO_BUILTIN_CHRPATH=ON \
%else
        -DWITH_SANITIZE_ADDRESS=ON \
%endif
        -DCMAKE_INSTALL_LIBDIR=%{_lib}

%if 0%{?fedora} > 32
%cmake_build
%else
make %{?_smp_mflags}
%endif

%install
%if %{defined suse_version}
%cmake_install
%endif

%if %{defined fedora} || %{defined rhel}
%if 0%{?fedora} > 32
%cmake_install
%else
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
%endif
%endif 

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
%{INSTALL_PREFIX}/%{_lib}/*.so.*
%{INSTALL_PREFIX}/bin/
%{INSTALL_PREFIX}/share/man/man1/xfreerdp.1*
%{INSTALL_PREFIX}/share/man/man1/freerdp-shadow-cli.1*
%{INSTALL_PREFIX}/share/man/man1/winpr-makecert.1*
%{INSTALL_PREFIX}/share/man/man1/winpr-hash.1*
%{INSTALL_PREFIX}/share/man/man7/wlog.7*

%files devel
%defattr(-,root,root)
%{INSTALL_PREFIX}/%{_lib}/*.so
%{INSTALL_PREFIX}/include/
%{INSTALL_PREFIX}/%{_lib}/pkgconfig/
%{INSTALL_PREFIX}/%{_lib}/cmake/

%post -p /sbin/ldconfig


%postun -p /sbin/ldconfig


%changelog
* Wed Feb 7 2018 FreeRDP Team <team@freerdp.com> - 2.0.0-0
- Update version information and support for OpenSuse 42.1
* Tue Feb 03 2015 FreeRDP Team <team@freerdp.com> - 1.2.1-0
- Update version information
* Fri Jan 23 2015 Bernhard Miklautz <bmiklautz+freerdp@shacknet.at> - 1.2.0-0
- Initial version
