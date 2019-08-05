Name:		headless-server
Version:	0.0.1
Release:	0
Summary:	Display server for headless profile
License:	MIT
Group:		Graphics & UI Framework/Wayland Window System

Source:		%{name}-%{version}.tar.xz
source1001:     %name.manifest

BuildRequires:	autoconf > 2.64
BuildRequires:	automake >= 1.11
BuildRequires:	libtool >= 2.2
BuildRequires:  pkgconfig(pepper)
BuildRequires:  pkgconfig(pepper-inotify)
BuildRequires:  pkgconfig(pepper-keyrouter)
BuildRequires:  pkgconfig(pepper-xkb)
BuildRequires:  pkgconfig(pepper-devicemgr)
BuildRequires:  pkgconfig(pepper-evdev)
BuildRequires:  pkgconfig(xkbcommon)
BuildRequires:  pkgconfig(wayland-tbm-server)
BuildRequires:  pkgconfig(tizen-extension-server)
BuildRequires:  pkgconfig(capi-system-peripheral-io)
BuildRequires:	pkgconfig(xdg-shell-unstable-v6-server)
BuildRequires:	pkgconfig(tizen-extension-server)

Requires: pepper pepper-keyrouter pepper-devicemgr pepper-evdev
Requires: pepper-xkb xkeyboard-config xkb-tizen-data
Requires: libtbm
Requires: capi-system-peripheral-io
Conflicts: pepper-doctor

%description
Headless server is a display server for headless profile.

%package devel
Summary: Development module for headless-server package
Requires: %{name} = %{version}-%{release}

%description devel
This package includes developer files common to all packages.

%prep
%setup -q
cp %{SOURCE1001} .

%build
%autogen

make %{?_smp_mflags}

%install
%make_install

%define display_user display
%define display_group display

# install system session services
%__mkdir_p %{buildroot}%{_unitdir}
install -m 644 data/units/display-manager.service %{buildroot}%{_unitdir}
install -m 550 data/scripts/* %{buildroot}%{_bindir}
install -m 644 data/units/display-manager-ready.path %{buildroot}%{_unitdir}
install -m 644 data/units/display-manager-ready.service %{buildroot}%{_unitdir}

# install user session service
%__mkdir_p %{buildroot}%{_unitdir_user}
install -m 644 data/units/display-user.service %{buildroot}%{_unitdir_user}

# install env file and scripts for service
%__mkdir_p %{buildroot}%{_sysconfdir}/sysconfig
install -m 0644 data/units/display-manager.env %{buildroot}%{_sysconfdir}/sysconfig
%__mkdir_p %{buildroot}%{_sysconfdir}/profile.d
install -m 0644 data/units/display_env.sh %{buildroot}%{_sysconfdir}/profile.d

%post -n %{name} -p /sbin/ldconfig
%postun -n %{name} -p /sbin/ldconfig

%pre
# create groups 'display'
getent group %{display_group} >/dev/null || %{_sbindir}/groupadd -r -o %{display_group}
# create user 'display'
getent passwd %{display_user} >/dev/null || %{_sbindir}/useradd -r -g %{display_group} -d /run/display -s /bin/false -c "Display" %{display_user}

# create links within systemd's target(s)
%__mkdir_p %{_unitdir}/graphical.target.wants/
%__mkdir_p %{_unitdir_user}/basic.target.wants/
ln -sf ../display-manager.service %{_unitdir}/graphical.target.wants/
ln -sf ../display-manager-ready.service %{_unitdir}/graphical.target.wants/
ln -sf ../display-user.service %{_unitdir_user}/basic.target.wants/

%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%license COPYING
%{_bindir}/headless*
%{_bindir}/winfo
%{_unitdir}/display-manager-ready.path
%{_unitdir}/display-manager-ready.service
%{_unitdir}/display-manager.service
%{_unitdir_user}/display-user.service
%config %{_sysconfdir}/sysconfig/display-manager.env
%config %{_sysconfdir}/profile.d/display_env.sh

%files devel
%manifest %{name}.manifest
%defattr(-,root,root,-)

