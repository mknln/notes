#!/bin/sh
# Builds the .deb package for a Debian or Ubuntu release in a Docker container,
# so it can be built for any release from any machine with Docker. The package
# only installs on the release it was built for.
#
# Usage: linux/build-deb.sh <image>, e.g. debian:trixie, ubuntu:26.04 or
# ubuntu:26.10. The package is written to build/deb/<image>/, e.g.
# build/deb/ubuntu-26.04/.
set -eu

if [ $# -ne 1 ]; then
    echo "Usage: $0 <image>, e.g. debian:trixie or ubuntu:26.04" >&2
    exit 1
fi
image=$1
repo=$(cd "$(dirname "$0")/.." && pwd)
out=$repo/build/deb/$(echo "$image" | tr ':/' '--')
mkdir -p "$out"

# The repository is mounted read-only and copied, leaving out local build
# folders and settings, so the build can't change it.
docker run --rm \
    -v "$repo":/repo:ro \
    -v "$out":/out \
    -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
    "$image" sh -euc '
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq
        apt-get install -y -qq --no-install-recommends dpkg-dev >/dev/null
        mkdir /src
        tar -C /repo --exclude=./build --exclude=./builds --exclude=./.git \
            --exclude=./CMakeUserPresets.json --exclude=./.vscode -cf - . | tar -C /src -xf -
        cd /src
        apt-get build-dep -y -qq ./ >/dev/null
        dpkg-buildpackage -us -uc -b
        cp ../*.deb /out/
        chown -R "$HOST_UID:$HOST_GID" /out
    '
echo "Built: $(ls "$out"/*.deb)"
