#
# spec file for package familiar on fedora, rhel, opensuse leap 15.x
#

# fedora >= 30, rhel >= 7
%define is_rhel_or_fedora (0%{?fedora} && 0%{?fedora} >= 30) || (0%{?rhel} && 0%{?rhel} >= 7)
# openSUSE Leap >= 15.2
%define is_suse_leap (0%{?is_opensuse} && 0%{?sle_version} >= 150200)

Name: familiar
# Placeholder - Linux-pack.yml's rpm-pack job overwrites this with the
# real $VERSION (from CMakeLists.txt) and appends the commit hash to
# Release before building, so it never actually goes stale for a CI
# build. Kept roughly current here for anyone building this spec
# standalone.
Version: 0.0.16
%if %{is_rhel_or_fedora}
Release: 1%{?dist}
%endif
%if %{is_suse_leap}
Release: 1
%endif
License: GPL-3.0-or-later
Summary: Reference board for 2D/3D artists
URL: https://github.com/try-hard-factory/familiar
Source0: %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires: cmake >= 3.16.0
BuildRequires: gcc-c++ >= 9
BuildRequires: desktop-file-utils
BuildRequires: cmake(Qt6Core) >= 6.2.0
BuildRequires: cmake(Qt6Gui) >= 6.2.0
BuildRequires: cmake(Qt6Network) >= 6.2.0
BuildRequires: cmake(Qt6Widgets) >= 6.2.0

Requires: hicolor-icon-theme
%if %{is_rhel_or_fedora}
Requires: qt6-qtbase >= 6.2.0
%endif
%if %{is_suse_leap}
Requires: libQt6Core6 >= 6.2.0
Requires: libQt6Widgets6 >= 6.2.0
%endif

%description
Familiar is a reference board for 2D/3D artists - a canvas for
collecting, arranging, and annotating reference images.

%prep
%autosetup -p1

%build
%if %{is_suse_leap}
%cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DRUN_IN_PLACE=OFF
%endif
%if %{is_rhel_or_fedora}
%cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DRUN_IN_PLACE=OFF
%endif
%cmake_build

%install
%cmake_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/*.desktop

%files
%doc README.md
%license LICENSE
%{_bindir}/%{name}
%{_datadir}/applications/org.tryhardfactory.Familiar.desktop
%{_datadir}/mime/packages/org.tryhardfactory.Familiar.xml
%{_datadir}/icons/hicolor/*/apps/*.png
%{_datadir}/icons/hicolor/*/apps/*.svg
%{_datadir}/icons/hicolor/*/mimetypes/*.png
%{_datadir}/icons/hicolor/*/mimetypes/*.svg

%changelog
* Wed Aug 19 2026 try-hard-factory <mpano91@gmail.com> - 0.0.15-1
- Initial RPM packaging for familiar.
