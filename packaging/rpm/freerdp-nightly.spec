#
# spec file for package freerdp-nightly
#
# Copyright (c) 2015 Bernhard Miklautz <bernhard.miklautz@shacknet.at>
#
# Bugs and comments https://github.com/FreeRDP/FreeRDP/issues


%define   INSTALL_PREFIX /opt/freerdp-nightly/
Name:           freerdp-nightly
Version:        2.0
Release:        0
License:        ASL 2.0
Summary:        Free implementation of the Remote Desktop Protocol (RDP)
Url:            http://www.freerdp.com
Group:          Productivity/Networking/Other
Source0:        %{name}-%{version}.tar.bz2
#Source1:        %{name}-rpmlintrc
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
BuildRequires: pcsc-lite-devel
BuildRequires: uuid-devel
BuildRequires: libxml2-devel
BuildRequires: zlib-devel

# (Open)Suse
%if %{defined suse_version}
BuildRequires: docbook-xsl-stylesheets
BuildRequires: libxslt-tools
BuildRequires: pkg-config
BuildRequires: libopenssl-devel
BuildRequires: alsa-devel
BuildRequires: libpulse-devel
BuildRequires: libgsm-devel
BuildRequires: libusb-1_0-devel
BuildRequires: libudev-devel
BuildRequires: dbus-1-glib-devel
BuildRequires: gstreamer-devel
BuildRequires: gstreamer-plugins-base-devel
BuildRequires: wayland-devel
BuildRequires: libjpeg-devel
BuildRequires: libavutil-devel
%endif
# fedora 21+
%if 0%{?fedora} >= 21
BuildRequires: docbook-style-xsl
BuildRequires: libxslt
BuildRequires: pkgconfig
BuildRequires: openssl-devel
BuildRequires: alsa-lib-devel
BuildRequires: pulseaudio-libs-devel
BuildRequires: gsm-devel
BuildRequires: libusbx-devel
BuildRequires: systemd-devel
BuildRequires: dbus-glib-devel
BuildRequires: gstreamer1-devel
BuildRequires: gstreamer1-plugins-base-devel
BuildRequires: libwayland-client-devel
BuildRequires: libjpeg-turbo-devel
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

%build
%cmake  -DCMAKE_SKIP_RPATH=FALSE \
        -DCMAKE_SKIP_INSTALL_RPATH=FALSE \
        -DWITH_PULSE=ON \
        -DWITH_CHANNELS=ON \
        -DSTATIC_CHANNELS=ON \
        -DWITH_CUPS=ON \
        -DWITH_PCSC=ON \
        -DWITH_JPEG=ON \
        -DWITH_GSTREAMER_0_10=ON \
        -DWITH_GSM=ON \
        -DCHANNEL_URBDRC=ON \
        -DCHANNEL_URBDRC_CLIENT=ON \
        -DWITH_SERVER=ON \
        -DBUILD_TESTING=OFF \
        -DCMAKE_BUILD_TYPE=RELWITHDEBINFO \
        -DCMAKE_INSTALL_PREFIX=%{INSTALL_PREFIX} \
%if %{defined suse_version}
	-DCMAKE_NO_BUILTIN_CHRPATH=ON \
%endif
        -DCMAKE_INSTALL_LIBDIR=%{_lib}

make %{?_smp_mflags}

%install
%if %{defined suse_version}
%cmake_install
%endif

%if %{defined fedora}
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
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
%{INSTALL_PREFIX}/%{_lib}/*.so.*
%{INSTALL_PREFIX}/bin/
%{INSTALL_PREFIX}/share/man/man1/xfreerdp.1*

%files devel
%defattr(-,root,root)
%{INSTALL_PREFIX}/%{_lib}/*.so
%{INSTALL_PREFIX}/include/
%{INSTALL_PREFIX}/%{_lib}/pkgconfig/
%{INSTALL_PREFIX}/%{_lib}/cmake/

%post -p /sbin/ldconfig


%postun -p /sbin/ldconfig


%changelog
* Tue Nov 16 2015 FreeRDP Team <team@freerdp.com> - 2.0.0-0
- Update version information and support for OpenSuse 42.1
* Tue Feb 03 2015 FreeRDP Team <team@freerdp.com> - 1.2.1-0
- Update version information
* Fri Jan 23 2015 Bernhard Miklautz <bmiklautz+freerdp@shacknet.at> - 1.2.0-0
- Initial version
