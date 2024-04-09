# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion %(uname -r)}

%define kmod_name hsi2s-kernel

%define debug_package %{nil}

Summary: HS-I2S driver
Name: hsi2s-kernel
Version: 1.0
Release: r0_sa8775
License: GPL-2.0
URL: https://git.codelinaro.org/
Source0:%{name}-%{version}.tar.gz
BuildRequires:  kernel-automotive-devel-uname-r = %{kversion} make libtool gcc-g++ systemd-rpm-macros
Requires: kernel-automotive-core-uname-r = %{kversion}

%description
hsi2s kernel module

%package -n kernel-module-hsi2s-kernel-5.14.0-rt21
Summary: hsi2s-kernel kernel module
License: GPL-2.0

%description -n kernel-module-hsi2s-kernel-5.14.0-rt21
hsi2s-kernel kernel module

%prep
%setup -qn %{name}

%build
#%make_build
KSRC=%{_usrsrc}/kernels/%{kversion}
make KERNEL_SRC=${KSRC} modules

%post
depmod %{kversion}

%postun
depmod %{kversion}

%install
KSRC=%{_usrsrc}/kernels/%{kversion}
make KERNEL_SRC=${KSRC} INSTALL_MOD_PATH=$RPM_BUILD_ROOT  modules_install
rm -rf "$RPM_BUILD_ROOT/lib/modules/%{kversion}/modules."*

%clean
rm -rf $RPM_BUILD_ROOT

%files
/lib/modules/%{kversion}/extra/driver/hsi2s.ko
