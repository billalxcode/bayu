#!/usr/bin/env bash
# Rebuilds RPM Fusion's wl-kmod (akmod-wl: wl, the Broadcom Wi-Fi driver of
# older MacBooks) with packaging/wl's patches and adds it to out/repo. EL10's
# kernel lacks lib80211 though it is a 6.12, and RPM Fusion's akmod-wl does
# not build there. The release gets .1.bayu, above RPM Fusion's. A build is
# kept in WL_CACHE and reused: releases only rebuild it when WL changes.
# x86_64 only: the driver is for the Broadcom chips of Intel MacBooks. On
# aarch64 this script does nothing and succeeds, so `make kernel` works there.
#   WL=6.30.223.271-63.el10      RPM Fusion's version-release to rebuild
#   WL_CACHE=~/.cache/bayu-wl
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/../..")
ARCH=$(uname -m)
if [[ $ARCH == aarch64 || $ARCH == arm64 ]]; then
  echo "wl: no aarch64 driver, skipping"
  exit 0
fi
WL=${WL:-6.30.223.271-63.el10}
SRPM_URL=${SRPM_URL:-https://download1.rpmfusion.org/nonfree/el/updates/10/SRPMS/w/wl-kmod-$WL.src.rpm}
CACHE=${WL_CACHE:-$HOME/.cache/bayu-wl}/$WL
mkdir -p "$CACHE" "$ROOT/out/repo"

if ! compgen -G "$CACHE/akmod-wl-*.rpm" >/dev/null; then
  work=$(mktemp -d) && trap 'rm -rf "$work"' EXIT
  curl -fL -o "$work/wl-kmod.src.rpm" "$SRPM_URL"
  cp "$ROOT"/packaging/wl/*.patch "$work/"
  podman run --rm -v "$work":/work:Z bayu-builder bash -euc '
    dnf -y -q install kmodtool elfutils-libelf-devel >/dev/null
    rpm -i --define "_topdir /tmp/top" /work/wl-kmod.src.rpm
    spec=/tmp/top/SPECS/wl-kmod.spec
    # The patches here after those of RPM Fusion, numbered from 100.
    n=100
    for patch in /work/*.patch; do
      cp "$patch" /tmp/top/SOURCES/
      sed -i "/^Patch37:/a Patch$n: $(basename "$patch")" "$spec"
      sed -i "/^%patch -P 37 /a %patch -P $n -p1" "$spec"
      n=$((n + 1))
    done
    # --nodeps: buildsys-build-rpmfusion-kerneldevpkgs-akmod only gathers
    # the build root of RPM Fusion; an akmod needs kmodtool, installed above.
    rpmbuild -bb "$spec" --nodeps --define "_topdir /tmp/top" --define "dist .el10.1.bayu"
    cp /tmp/top/RPMS/*/*.rpm /work/'
  cp "$work"/*.rpm "$CACHE/"
fi
cp "$CACHE"/*.rpm "$ROOT/out/repo/"
podman run --rm -v "$ROOT/out/repo":/repo:Z bayu-builder createrepo_c -q --update /repo
ls -l "$CACHE"
