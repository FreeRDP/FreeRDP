# cmake
# make package_source
# rpmbuild -ta freerdp-<...>.tar.gz

Summary: Remote Desktop Protocol functionality
Name: freerdp
Version: 1.0.0
Release: 1%{?dist}
License: Apache License 2.0
Group: Applications/Communications
URL: http://www.freerdp.com/
Source: http://downloads.sourceforge.net/%{name}/%{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:  openssl-devel
BuildRequires:	libX11-devel, libXcursor-devel, libXext-devel, libXinerama-devel, libXdamage-devel, libXv-devel, libxkbfile-devel
BuildRequires:	cups-devel
BuildRequires:	alsa-lib-devel
BuildRequires:	pcsc-lite-devel

%description
FreeRDP is a free implementation of the Remote Desktop Protocol (RDP)
according to the Microsoft Open Specifications.

%package -n xfreerdp
Summary: Remote Desktop Protocol client
Group: Applications/Communications
Requires: %{name}-libs = %{version}-%{release}, %{name}-plugins-standard = %{version}-%{release}
%description -n xfreerdp
FreeRDP is a free implementation of the Remote Desktop Protocol (RDP)
according to the Microsoft Open Specifications.

%package libs
Summary: Core libraries implementing the RDP protocol
Group: Applications/Communications
%description libs
libfreerdp-core can be embedded in applications.

libfreerdp-channels and libfreerdp-kbd might be convenient to use in X
applications together with libfreerdp-core.

libfreerdp-core can be extended with plugins handling RDP channels.

%package plugins-standard
Summary: Plugins for handling the standard RDP channels
Group: Applications/Communications
Requires: %{name}-libs = %{version}-%{release}
%description plugins-standard
A set of plugins to the channel manager implementing the standard virtual
channels extending RDP core functionality. For instance, sounds, clipboard
sync, disk/printer redirection, etc.

%package devel
Summary: Libraries and header files for embedding and extending freerdp
Group: Applications/Communications
Requires: %{name}-libs = %{version}-%{release}
Requires: pkgconfig
%description devel
Header files and unversioned libraries for libfreerdp-core, libfreerdp-channels,
libfreerdp-kbd, libfreerdp-cache, libfreerdp-codec, libfreerdp-rail,
libfreerdp-gdi and libfreerdp-utils.

%prep
%setup -q

%build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DWITH_PCSC=ON . 

make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
rm -f $RPM_BUILD_ROOT%{_libdir}/{freerdp/,lib}*.{a,la} # FIXME: They shouldn't be installed in the first place

%post libs -p /sbin/ldconfig

%postun libs -p /sbin/ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files -n xfreerdp
%defattr(-,root,root)
%{_bindir}/xfreerdp
%{_mandir}/*/*

%files libs
%defattr(-,root,root)
%doc LICENSE README
%{_libdir}/lib*.so.*
%dir %{_libdir}/freerdp
%{_datadir}/freerdp/

%files plugins-standard
%defattr(-,root,root)
%{_libdir}/freerdp/*.so

%files devel
%defattr(-,root,root)
%{_includedir}/freerdp/
%{_libdir}/lib*.so
%{_libdir}/pkgconfig/*

%changelog

* Tue Mar 16 2010 Mads Kiilerich <mads@kiilerich.com> - 0.0.1-1
- Initial "upstream" freerdp spec - made and tested for Fedora 12
