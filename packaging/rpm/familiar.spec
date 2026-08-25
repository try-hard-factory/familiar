#
# spec file for package familiar - wraps an already-built AppImage,
# same approach PureRef's own .deb uses (see packaging/debian/control
# for the sourced reference) - no compiling here, no per-distro Qt
# BuildRequires, just installing files into %{buildroot}. Only real
# runtime dependency is fuse-libs (Fedora's FUSE2 runtime package name -
# verified via web search), needed to mount the AppImage.
#

Name: familiar
# Placeholder - Linux-pack.yml's rpm-pack job overwrites this with the
# real $VERSION (from CMakeLists.txt) before building, same as
# packaging/debian/control's @VERSION@ placeholder gets sed'd. Kept
# roughly current here for anyone building this spec standalone.
Version: 0.0.16
Release: 1%{?dist}
License: GPL-3.0-or-later
Summary: Reference board for 2D/3D artists
URL: https://github.com/try-hard-factory/familiar

# All pre-built by the CI job before rpmbuild runs (or copied by hand
# for a standalone build) - dropped straight into %{_sourcedir}, no
# %prep/%build steps touch them.
Source0: familiar-%{version}-x86_64.AppImage
Source1: org.tryhardfactory.Familiar.desktop
Source2: org.tryhardfactory.Familiar.xml
Source3: familiar.svg
Source4: familiar_256.png
Source5: familiar_512.png

Requires: fuse-libs

# The AppImage in Source0 is one big ELF+embedded-squashfs blob, not
# something rpm's automatic dependency scanner or its post-install
# strip/debuginfo-extraction scripts (find-debuginfo.sh) know how to
# handle safely - letting them run against it risks either a build
# failure or, worse, a corrupted/stripped AppImage that silently no
# longer runs. [Непроверено - based on documented rpm behavior around
# these scripts, not confirmed yet against a real CI run of this exact
# spec] Disabling both is the standard fix for "package wraps a
# prebuilt/foreign binary" specs.
%global debug_package %{nil}
%global __os_install_post %{nil}
%global _build_id_links none
# Same reason - the automatic Requires: scanner would otherwise try to
# ldd-style-inspect the AppImage and the desktop/mime files, and could
# emit bogus auto-detected dependencies for a binary it doesn't
# actually understand.
AutoReqProv: no

%description
Familiar is a canvas for collecting, arranging, and annotating
reference images - built for 2D/3D artists who need a fast, always-
on-top board of source material next to their main work surface.

Wraps a self-contained AppImage build (same approach PureRef's own
package uses) - no system Qt dependency, only fuse-libs to mount it.

%install
rm -rf %{buildroot}

install -Dm755 %{SOURCE0} %{buildroot}/opt/familiar/familiar.AppImage
mkdir -p %{buildroot}%{_bindir}
ln -s /opt/familiar/familiar.AppImage %{buildroot}%{_bindir}/familiar

install -Dm644 %{SOURCE1} %{buildroot}%{_datadir}/applications/org.tryhardfactory.Familiar.desktop
install -Dm644 %{SOURCE2} %{buildroot}%{_datadir}/mime/packages/org.tryhardfactory.Familiar.xml

# Same desktop entry / mime-type / icon layout CMakeLists.txt's own
# install() rules use for a from-source build (src/CMakeLists.txt,
# `if (UNIX AND NOT APPLE)` block) and packaging/debian/build-deb.sh
# mirrors on the .deb side - kept in sync by hand, since this spec no
# longer runs that CMake code at all.
install -Dm644 %{SOURCE3} %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/org.tryhardfactory.Familiar.svg
install -Dm644 %{SOURCE4} %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/org.tryhardfactory.Familiar.png
install -Dm644 %{SOURCE5} %{buildroot}%{_datadir}/icons/hicolor/512x512/apps/org.tryhardfactory.Familiar.png
install -Dm644 %{SOURCE3} %{buildroot}%{_datadir}/icons/hicolor/scalable/mimetypes/org.tryhardfactory.Familiar.svg
install -Dm644 %{SOURCE4} %{buildroot}%{_datadir}/icons/hicolor/256x256/mimetypes/org.tryhardfactory.Familiar.png

%files
/opt/familiar/familiar.AppImage
%{_bindir}/familiar
%{_datadir}/applications/org.tryhardfactory.Familiar.desktop
%{_datadir}/mime/packages/org.tryhardfactory.Familiar.xml
%{_datadir}/icons/hicolor/scalable/apps/org.tryhardfactory.Familiar.svg
%{_datadir}/icons/hicolor/256x256/apps/org.tryhardfactory.Familiar.png
%{_datadir}/icons/hicolor/512x512/apps/org.tryhardfactory.Familiar.png
%{_datadir}/icons/hicolor/scalable/mimetypes/org.tryhardfactory.Familiar.svg
%{_datadir}/icons/hicolor/256x256/mimetypes/org.tryhardfactory.Familiar.png

%changelog
* See ChangeLog / GitHub releases for details.
