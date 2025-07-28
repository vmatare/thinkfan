Name: thinkfan
Version: 1.2.1
Release: 0.0.4%{?dist}
Summary: A simple, lightweight fan control program 
License: GPLv3+
Source: https://codeload.github.com/vmatare/thinkfan/tar.gz/%{version}
BuildArch: x86_64
BuildRequires: cmake gcc systemd yaml-cpp yaml-cpp-devel
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
Thinkfan is a simple, lightweight fan control program.

%changelog
* Wed Aug 5 2020 Trae Santiago <tsantiago@us.ibm.com> - 1.2.1-0.0.1
    - Initial specfile
* Wed Aug 5 2020 Trae Santiago <tsantiago@us.ibm.com> - 1.2.1-0.0.2
    - Updated to remove unneeded comments and test without disabling debug package
* Wed Aug 5 2020 Trae Santiago <tsantiago@us.ibm.com> - 1.2.1-0.0.3
    - Fixed service files and config location
* Wed Aug 5 2020 Trae Santiago <tsantiago@us.ibm.com> - 1.2.1-0.0.4
    - Fixed service files (norlly)

%prep
%autosetup -D -n .

%build
cmake "%{_builddir}/%{name}-%{version}"
make

%install
# Make directory tree
mkdir -p %{buildroot}{%{_bindir},%{_datadir}/{doc,licenses}/%{name},%{_mandir}/{man1,man5},%{_sysconfdir}/%{name},%{_unitdir}/%{name}}
# Copy main program
cp "%{_builddir}/%{name}" "%{buildroot}%{_bindir}/"
# Copy systemd services
cp %{_builddir}/{rcscripts/systemd/%{name}.service,%{name}-%{version}/rcscripts/systemd/%{name}{-sleep,-wakeup}.service} "%{buildroot}%{_unitdir}/"
# Copy example and readme
cp %{_builddir}/%{name}-%{version}/{README.md,examples/thinkfan.yaml} "%{buildroot}%{_datadir}/doc/%{name}/"
# Copy man pages
cp "%{_builddir}/%{name}-%{version}/src/thinkfan.1" "%{buildroot}%{_mandir}/man1/"
cp "%{_builddir}/%{name}-%{version}/src/thinkfan.conf.5" "%{buildroot}%{_mandir}/man5/"
# Copy license
cp "%{_builddir}/%{name}-%{version}/COPYING" "%{buildroot}%{_datadir}/licenses/%{name}/"

%files
%{_bindir}/%{name}
%config %ghost %{_sysconfdir}/%{name}.conf
%doc %{_datadir}/doc/%{name}
%license COPYING
%{_mandir}/man1/thinkfan.1.gz
%{_mandir}/man5/thinkfan.conf.5.gz
%{_unitdir}/%{name}.service
%{_unitdir}/%{name}-sleep.service
%{_unitdir}/%{name}-wakeup.service

%clean
rm -rf %{buildroot} %{_builddir}/*

%preun
if [ "${1}" = 0 ]; then
    systemctl daemon-reload
    rm "/usr/local/sbin/%{name}"
fi

%post
if ! [ -e "/usr/local/sbin/%{name}" ]; then
    ln -sf "%{_bindir}/%{name}" "/usr/local/sbin/%{name}"
fi
systemctl daemon-reload
