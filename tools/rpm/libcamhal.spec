Summary: Linux libcamhal
Name: libcamhal
Version: %{version}
Release: %{release}
License: Apache
Group: Development/Tools
Prefix: /usr
Prefix: /etc
BuildRoot: %(mktemp -ud %{_builddir}/%{name}-%{version}-%{release}-XXXXXX)
%description
Linux camera HAL.

%prep
%build

%install
rm -rf %{buildroot}

mkdir -p %{buildroot}/usr/lib
mkdir -p %{buildroot}/usr/include/libcamhal
mkdir -p %{buildroot}/etc/camera/
mkdir -p %{buildroot}/usr/share/defaults/etc/camera/

cp -v  %{_srcdir}/.libs/*.so* %{buildroot}/usr/lib/
cp -v  %{_srcdir}/.libs/*.a   %{buildroot}/usr/lib/
cp -vr %{_srcdir}/include/*   %{buildroot}/usr/include/libcamhal/
cp -vr %{_srcdir}/config/*    %{buildroot}/etc/camera/
cp -vr %{_srcdir}/config/*    %{buildroot}/usr/share/defaults/etc/camera/

%files
/usr/lib/
/usr/include/
/etc/
/usr/share/
