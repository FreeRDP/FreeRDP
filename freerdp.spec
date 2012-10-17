# rpmbuild -ta freerdp-<...>.tar.gz

Name:           freerdp
Version:        1.0.1
Release:        1%{?dist}
Summary:        Remote Desktop Protocol functionality

Group:          Applications/Communications
License:        Apache License 2.0
URL:            http://www.freerdp.com/
Source0:        https://github.com/downloads/FreeRDP/FreeRDP/%{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  cmake
BuildRequires:  xmlto
BuildRequires:  openssl-devel
BuildRequires:  libX11-devel
BuildRequires:  libXext-devel
BuildRequires:  libXinerama-devel
BuildRequires:  libXcursor-devel
BuildRequires:  libXdamage-devel
BuildRequires:  libXv-devel
BuildRequires:  libxkbfile-devel
BuildRequires:  pulseaudio-libs-devel
BuildRequires:  cups-devel
BuildRequires:  alsa-lib-devel
BuildRequires:  pcsc-lite-devel
BuildRequires:  desktop-file-utils

%description
FreeRDP is a free implementation of the Remote Desktop Protocol (RDP)
according to the Microsoft Open Specifications.


%package        -n xfreerdp
Summary:        Remote Desktop Protocol client
Group:          Applications/Communications
Requires:       %{name}-libs = %{version}-%{release}
Requires:       %{name}-plugins-standard = %{version}-%{release}
%description    -n xfreerdp
FreeRDP is a free implementation of the Remote Desktop Protocol (RDP)
according to the Microsoft Open Specifications.


%package        libs
Summary:        Core libraries implementing the RDP protocol
Group:          Applications/Communications
%description    libs
libfreerdp-core can be embedded in applications.

libfreerdp-channels and libfreerdp-kbd might be convenient to use in X
applications together with libfreerdp-core.

libfreerdp-core can be extended with plugins handling RDP channels.

%package        plugins-standard
Summary:        Plugins for handling the standard RDP channels
Group:          Applications/Communications
Requires:       %{name}-libs = %{version}-%{release}
%description    plugins-standard
A set of plugins to the channel manager implementing the standard virtual
channels extending RDP core functionality. For instance, sounds, clipboard
sync, disk/printer redirection, etc.


%package        devel
Summary:        Libraries and header files for embedding and extending freerdp
Group:          Applications/Communications
Requires:       %{name}-libs = %{version}-%{release}
Requires:       pkgconfig
%description    devel
Header files and unversioned libraries for libfreerdp-core, libfreerdp-channels,
libfreerdp-locale, libfreerdp-cache, libfreerdp-codec, libfreerdp-rail,
libfreerdp-gdi and libfreerdp-utils.

%prep

%setup -q

cat << EOF > xfreerdp.desktop 
[Desktop Entry]
Type=Application
Name=X FreeRDP
NoDisplay=true
Comment=Connect to RDP server and display remote desktop
Icon=%{name}
Exec=/usr/bin/xfreerdp
Terminal=false
Categories=Network;RemoteAccess;
EOF


%build

cmake \
        -DCMAKE_INSTALL_PREFIX:PATH=/usr \
        -DWITH_CUPS:BOOL=ON \
        -DWITH_PCSC:BOOL=ON \
        -DWITH_PULSE:BOOL=ON \
		-DWITH_MACAUDIO:BOOL=ON \
        -DWITH_X11:BOOL=ON \
        -DWITH_XCURSOR:BOOL=ON \
        -DWITH_XEXT:BOOL=ON \
        -DWITH_XINERAMA:BOOL=ON \
        -DWITH_XKBFILE:BOOL=ON \
        -DWITH_XV:BOOL=ON \
        -DWITH_ALSA:BOOL=ON \
        -DWITH_CUNIT:BOOL=OFF \
        -DWITH_DIRECTFB:BOOL=OFF \
        -DWITH_FFMPEG:BOOL=OFF \
        -DWITH_SSE2:BOOL=OFF \
        .

make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT

make install DESTDIR=$RPM_BUILD_ROOT INSTALL='install -p'

desktop-file-install --dir=$RPM_BUILD_ROOT%{_datadir}/applications xfreerdp.desktop
install -p -D resources/FreeRDP_Icon_256px.png $RPM_BUILD_ROOT%{_datadir}/icons/hicolor/256x256/apps/%{name}.png


%clean
rm -rf $RPM_BUILD_ROOT


%post
# This is no gtk application, but try to integrate nicely with GNOME if it is available
gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :


%post libs -p /sbin/ldconfig


%postun libs -p /sbin/ldconfig


%files -n xfreerdp
%defattr(-,root,root,-)
%{_bindir}/xfreerdp
%{_mandir}/man1/xfreerdp.*
%{_datadir}/applications/xfreerdp.desktop
%{_datadir}/icons/hicolor/256x256/apps/%{name}.png

%files libs
%defattr(-,root,root,-)
%doc LICENSE README ChangeLog
%{_libdir}/lib%{name}-*.so.*
%dir %{_libdir}/%{name}/

%files plugins-standard
%defattr(-,root,root,-)
%{_libdir}/%{name}/*

%files devel
%defattr(-,root,root,-)
%{_includedir}/%{name}/
%{_libdir}/lib%{name}-*.so
%{_libdir}/pkgconfig/%{name}.pc


%changelog
