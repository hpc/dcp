Name: dcp
Version: 0.1
Release: 1
Summary: distributed file copy tool
License: LANL LA-CC
Group: System Environment/Base
Source: %{name}-%{version}-%{release}.tgz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}
URL: http://github.com/hpc/dcp

%description
A file copy program that evenly distributes work across a large cluster
without centralized state.

# Don't strip binaries
%define __os_install_post /usr/lib/rpm/brp-compress
%define debug_package %{nil}

###############################################################################

%prep
%setup -n %{name}-%{version}-%{release}

%build
%configure --program-prefix=%{?_program_prefix:%{_program_prefix}}
make %{?_smp_mflags}

%install
rm -rf "$RPM_BUILD_ROOT"
DESTDIR="$RPM_BUILD_ROOT" make install

###############################################################################

%clean
rm -rf $RPM_BUILD_ROOT

###############################################################################

%files
%defattr(-,root,root,0755)
%{_bindir}/dcp
%{_mandir}/man1/dcp.1.gz
