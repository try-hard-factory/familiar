#!/usr/bin/env bash
set -euo pipefail

# Builds a .deb inside a real Docker container - same distro images
# Linux-pack.yml's deb-pack job matrix uses - instead of installing
# build-deps directly on this machine. Nothing touches your host system
# except the final .deb landing in build-artifacts/. Not for real
# releases - those still come from CI/tags, this is just for quick
# local checks.
#
# The .deb wraps a self-contained AppImage (same approach PureRef's own
# .deb uses) - installing it doesn't need a system Qt at all, only
# libfuse2.
#
# Usage: ./packaging/debian/build-local.sh [image]
#   Defaults to ubuntu:24.04. Other targets from the real CI matrix:
#   ubuntu:26.04, debian:bookworm (12), debian:trixie (13).

IMAGE="${1:-ubuntu:24.04}"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker not found - install it to use this script." >&2
  exit 1
fi

cd "$(dirname "$0")/../.."

echo "Building in ${IMAGE}..."
docker run --rm \
  -v "$(pwd)":/work \
  "$IMAGE" \
  bash /work/packaging/debian/_build-in-container.sh

VERSION="$(grep 'set.*(.*FAMILIAR_VERSION' CMakeLists.txt | sed 's/[^0-9.]*//' | sed 's/)//g')"
# `apt install ./file.deb`, not `dpkg -i` - apt resolves Depends: itself
# (libfuse2, if not already on this machine); dpkg -i doesn't and just
# fails with "dependency problems" if anything's missing.
echo "Install/upgrade with: sudo apt install ./build-artifacts/familiar-${VERSION}-local.amd64.deb"
