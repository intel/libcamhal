Summary: Linux libcustomized_aic
Name: libcustomized_aic
Version: %{version}
Release: %{release}
License: Apache
Group: Development/Tools
Prefix: /usr
BuildRoot: %(mktemp -ud %{_builddir}/%{name}-%{version}-%{release}-XXXXXX)
%description
Customized aic library

%prep
%build

%install
rm -rf %{buildroot}

mkdir -p %{buildroot}/usr/lib
mkdir -p %{buildroot}/usr/include/customized_aic

cp -v  %{_srcdir}/.libs/*.so* %{buildroot}/usr/lib/
cp -v  %{_srcdir}/.libs/*.a   %{buildroot}/usr/lib/
cp -vr %{_srcdir}/*.h   %{buildroot}/usr/include/customized_aic/

%files
/usr/lib/
/usr/include/
