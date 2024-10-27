Name: hsi2s-test
Version: 1.0
Release: r0_sa8775
Summary: HSI2S application
License: GPL 2.0
Source0: %{name}-%{version}.tar.gz

BuildRequires: autoconf automake libtool gcc-g++ libcutils

%description
hsi2s test  application

%prep
%autosetup -n %{name}-%{version}

%build
cp ../driver/hsi2s_common.h .
%make_build


%install
mkdir -p %{buildroot}/%{_bindir}
install -m 0755 hsi2s_test %{buildroot}/%{_bindir}

%files
%{_bindir}/hsi2s_test
