#!/usr/bin/env bash
set -euo pipefail

# Wraps an already-built AppImage into a .deb - same approach PureRef's
# own .deb uses (verified: https://bbs.archlinux.org/viewtopic.php?id=301763),
# so installing familiar never needs a system Qt at all, only libfuse2
# (to mount the AppImage - same one dependency PureRef has). Replaced
# the old debhelper/dpkg-buildpackage-from-source approach entirely -
# no compiler/Qt build-deps needed to build this package anymore.
#
# Usage: build-deb.sh <path-to-AppImage> <version> <output.deb>

APPIMAGE="$1"
VERSION="$2"
OUT="$3"

if [ ! -f "$APPIMAGE" ]; then
  echo "AppImage not found: $APPIMAGE" >&2
  exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/DEBIAN" \
         "$STAGE/opt/familiar" \
         "$STAGE/usr/bin" \
         "$STAGE/usr/share/applications" \
         "$STAGE/usr/share/mime/packages" \
         "$STAGE/usr/share/doc/familiar" \
         "$STAGE/usr/share/icons/hicolor/scalable/apps" \
         "$STAGE/usr/share/icons/hicolor/256x256/apps" \
         "$STAGE/usr/share/icons/hicolor/512x512/apps" \
         "$STAGE/usr/share/icons/hicolor/scalable/mimetypes" \
         "$STAGE/usr/share/icons/hicolor/256x256/mimetypes"

install -m755 "$APPIMAGE" "$STAGE/opt/familiar/familiar.AppImage"
ln -s /opt/familiar/familiar.AppImage "$STAGE/usr/bin/familiar"

# Same desktop entry / mime-type / icon layout CMakeLists.txt's own
# install() rules use for a from-source build (src/CMakeLists.txt,
# `if (UNIX AND NOT APPLE)` block) - kept in sync by hand, since this
# script no longer runs that CMake code at all.
install -m644 "$REPO_ROOT/data/desktopEntry/package/org.tryhardfactory.Familiar.desktop" \
  "$STAGE/usr/share/applications/"
install -m644 "$REPO_ROOT/packaging/linux/org.tryhardfactory.Familiar.xml" \
  "$STAGE/usr/share/mime/packages/"
install -m644 "$REPO_ROOT/data/img/app/familiar.svg" \
  "$STAGE/usr/share/icons/hicolor/scalable/apps/org.tryhardfactory.Familiar.svg"
install -m644 "$REPO_ROOT/data/img/app/familiar_256.png" \
  "$STAGE/usr/share/icons/hicolor/256x256/apps/org.tryhardfactory.Familiar.png"
install -m644 "$REPO_ROOT/data/img/app/familiar_512.png" \
  "$STAGE/usr/share/icons/hicolor/512x512/apps/org.tryhardfactory.Familiar.png"
install -m644 "$REPO_ROOT/data/img/app/familiar.svg" \
  "$STAGE/usr/share/icons/hicolor/scalable/mimetypes/org.tryhardfactory.Familiar.svg"
install -m644 "$REPO_ROOT/data/img/app/familiar_256.png" \
  "$STAGE/usr/share/icons/hicolor/256x256/mimetypes/org.tryhardfactory.Familiar.png"
install -m644 "$REPO_ROOT/packaging/debian/copyright" \
  "$STAGE/usr/share/doc/familiar/copyright"

sed "s/@VERSION@/${VERSION}/" "$REPO_ROOT/packaging/debian/control" > "$STAGE/DEBIAN/control"

dpkg-deb --build --root-owner-group "$STAGE" "$OUT"
echo "Built: $OUT"
