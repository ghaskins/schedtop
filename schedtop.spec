%define rpmrel _RPM_RELEASE

BuildRequires: boost-devel ncurses-devel gcc-c++

Summary: schedtop: displays scheduler statistics
Name: schedtop
Version: _RPM_VERSION
License: GPL
Release: %{rpmrel}
Requires: boost ncurses
Group: System
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
This utility will process statistics from /proc/schedstat such that the
busiest stats will bubble up to the top.  It can alternately be sorted by
the largest stat, or by name.  Stats can be included or excluded based on
reg-ex pattern matching.

Authors
--------------------------
  Gregory Haskins <ghaskins@novell.com>

%debug_package
%prep
%setup

%build
make

%install
make install PREFIX=$RPM_BUILD_ROOT

%clean
make clean

%files
%defattr(-,root,root)
/bin/schedtop

%changelog
