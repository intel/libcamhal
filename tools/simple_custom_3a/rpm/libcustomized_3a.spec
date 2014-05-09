Summary: Linux libcustomized_3a
Name: libcustomized_3a
Version: %{version}
Release: %{release}
License: Apache
Group: Development/Tools
Prefix: /usr
BuildRoot: %(mktemp -ud %{_builddir}/%{name}-%{version}-%{release}-XXXXXX)
%description
Customized 3a library

%prep
%build

%install
rm -rf %{buildroot}

mkdir -p %{buildroot}/usr/lib
mkdir -p %{buildroot}/usr/include/customized_3a

cp -v  %{_srcdir}/.libs/*.so* %{buildroot}/usr/lib/
cp -v  %{_srcdir}/.libs/*.a   %{buildroot}/usr/lib/
cp -vr %{_srcdir}/*.h   %{buildroot}/usr/include/customized_3a/

%files
/usr/lib/
/usr/include/
