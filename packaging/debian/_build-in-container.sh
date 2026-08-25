#!/usr/bin/env bash
set -euo pipefail

# Runs INSIDE the Docker container - see build-local.sh, which is the
# actual entry point. Already root here (containers default to root),
# no sudo.
#
# Builds an AppImage first (same recipe as Linux-pack.yml's
# appimage-pack job, just against apt's qt6-base-dev instead of a
# pinned install-qt-action Qt - fine for a local check, doesn't need to
# match CI's exact Qt point release), then wraps it into a .deb via
# build-deb.sh - the .deb no longer compiles anything itself.

cd /work

VERSION="$(grep 'set.*(.*FAMILIAR_VERSION' CMakeLists.txt | sed 's/[^0-9.]*//' | sed 's/)//g')"
echo "Building familiar ${VERSION} .deb (AppImage-in-deb) in $(cat /etc/os-release | grep ^PRETTY_NAME)..."

apt-get -y -qq update
apt-get -y --no-install-recommends install \
  build-essential cmake qt6-base-dev python3 fuse libfuse2 file wget git ca-certificates

# Same reason Linux-pack.yml's deb-pack/rpm-pack jobs need this - /work
# is bind-mounted from the host, owned by the host user's UID, not
# root (who we are in here). Without it, FetchContent's own git clone
# for googletest fails ("dubious ownership").
git config --global --add safe.directory '*'

rm -rf appimage-build AppDir
mkdir -p appimage-build
cd appimage-build

wget -c -nv "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
wget -c -nv "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod +x linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage

cmake -S /work -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr -DRUN_IN_PLACE=OFF
cmake --build build --parallel "$(nproc)"

mkdir -p AppDir
DESTDIR="$(pwd)/AppDir" cmake --install build

# apt's Qt6 names the binary qmake6 (Debian/Ubuntu's own convention),
# not qmake - see appimage-pack's own comment on why this differs from
# the pinned install-qt-action path.
export QMAKE="$(which qmake6 2>/dev/null || which qmake)"
# FUSE-mounting inside a container is unreliable - same reasoning
# appimage-pack's CI job already documents.
export APPIMAGE_EXTRACT_AND_RUN=1
./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt --output appimage

APPIMAGE_FILE=$(find . -maxdepth 1 -iname "*.AppImage" ! -iname "linuxdeploy*")
mv "$APPIMAGE_FILE" "familiar-${VERSION}-x86_64.AppImage"

cd /work
mkdir -p build-artifacts
/work/packaging/debian/build-deb.sh \
  "appimage-build/familiar-${VERSION}-x86_64.AppImage" \
  "$VERSION" \
  "build-artifacts/familiar-${VERSION}-local.amd64.deb"

# This bind-mounted directory is your actual working tree on the host -
# clean up everything else so it doesn't linger in `git status` after
# the container exits.
rm -rf appimage-build AppDir

echo
echo "Done: build-artifacts/familiar-${VERSION}-local.amd64.deb"
