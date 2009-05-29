%define rpmrel _RPM_RELEASE

%if 0%{?suse_version} == 1110
%define boostver 1_36_0
%endif

%if 0%{?suse_version} > 1110
%define boostver 1_38_0
%endif

%if 0%{?suse_version} > 1100
%define boostdeps libboost_regex%{boostver} libboost_program_options%{boostver} libboost_filesystem%{boostver}
%else
%define boostdeps boost
%endif


BuildRequires: %{boostdeps} boost-devel ncurses-devel gcc-c++

Summary: schedtop: displays scheduler statistics
Name: schedtop
Version: _RPM_VERSION
License: GPL
Release: %{rpmrel}
Requires: %{boostdeps} ncurses
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

# Install documentation  
%{__mkdir_p} %{buildroot}/%{_defaultdocdir}/schedtop  
%{__mkdir_p} %{buildroot}/%{_mandir}/man1  
%{__gzip} *.1  
%{__cp} *.1.gz %{buildroot}/%{_mandir}/man1  

%clean
make clean

%files
%defattr(-,root,root)
/usr/bin/schedtop
%{_mandir}/man1/*  
%{_defaultdocdir}/*

%changelog
