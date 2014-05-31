Name:       libnet-client
Summary:    Network Client library (Shared library)
Version:    0.1.77_48
Release:    1
Group:      System/Network
License:    Flora License
Source0:    %{name}-%{version}.tar.gz

%if %{_repository}=="wearable"
BuildRequires:	cmake
BuildRequires:	pkgconfig(dlog)
BuildRequires:	pkgconfig(vconf)
BuildRequires:	pkgconfig(gio-2.0)
BuildRequires:	pkgconfig(glib-2.0)
BuildRequires:	pkgconfig(gthread-2.0)
%else
BuildRequires:  cmake
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:	pkgconfig(gthread-2.0)
BuildRequires:	pkgconfig(dbus-glib-1)
%endif

%description
Network Client library (Shared library)

%package devel
Summary:    Network Client library (Development)
Group:      Development/Library
Requires:   %{name} = %{version}-%{release}

%description devel
Network Client library (Development)

%prep
%setup -q

%build
%if %{_repository}=="wearable"
cd wearable
./autogen.sh
./configure --prefix=/usr
%else
cd mobile
./autogen.sh
%configure
%endif

make %{?_smp_mflags}


%install
%if %{_repository}=="wearable"
cd wearable
%else
cd mobile
%endif
%make_install

#License
mkdir -p %{buildroot}%{_datadir}/license
%if %{_repository}=="wearable"
cp LICENSE %{buildroot}%{_datadir}/license/libnet-client
%else
cp LICENSE.Flora %{buildroot}%{_datadir}/license/libnet-client
%endif

%if %{_repository}=="wearable"
%else
#Make test app
cd test
cp -rf %{buildroot}/usr/lib/ ./
./build.sh
cd ..
%endif

%if %{_repository}=="wearable"
%files
%manifest wearable/libnet-client.manifest
%{_libdir}/libnetwork.so
%{_libdir}/libnetwork.so.0
%attr(644,-,-) %{_libdir}/libnetwork.so.0.0.0
%{_datadir}/license/libnet-client

%files devel
%{_includedir}/network/*.h
%{_libdir}/pkgconfig/network.pc
%else
%files
%manifest mobile/libnet-client.manifest
%{_libdir}/libnetwork.so
%{_libdir}/libnetwork.so.0
%attr(644,-,-) %{_libdir}/libnetwork.so.0.0.0
%{_datadir}/license/libnet-client

%files devel
%{_includedir}/network/*.h
%{_libdir}/pkgconfig/network.pc
%endif
