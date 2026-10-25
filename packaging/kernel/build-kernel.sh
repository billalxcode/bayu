#!/usr/bin/env bash
# Rebuilds the AlmaLinux kernel with packaging/kernel/kernel-local applied
# (binder for Android containers) and adds it to the out/repo dnf repository.
# The release gets .1.bayu, so it sorts above the AlmaLinux build it came from.
# A build is kept in KERNEL_CACHE and reused: releases only rebuild it when
# KERNEL changes.
#   KERNEL=6.12.0-211.68.1.el10_2    AlmaLinux kernel version-release to rebuild
#   SRPM_URL=...                     where to fetch its source RPM
#   KERNEL_CACHE=~/.cache/bayu-kernel
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/../..")
KERNEL=${KERNEL:-6.12.0-211.68.1.el10_2}
SRPM_URL=${SRPM_URL:-https://vault.almalinux.org/10.2/BaseOS/Source/Packages/kernel-$KERNEL.src.rpm}
CACHE=${KERNEL_CACHE:-$HOME/.cache/bayu-kernel}/$KERNEL
mkdir -p "$CACHE" "$ROOT/out/repo"

if ! compgen -G "$CACHE/kernel-core-*.rpm" >/dev/null; then
  work=$(mktemp -d) && trap 'rm -rf "$work"' EXIT
  curl -fL -o "$work/kernel.src.rpm" "$SRPM_URL"
  cp "$ROOT/packaging/kernel/kernel-local" "$work/"
  podman run --rm -v "$work":/work:Z bayu-builder bash -euc '
    rpm -i --define "_topdir /tmp/top" /work/kernel.src.rpm
    cp /work/kernel-local /tmp/top/SOURCES/kernel-local
    dnf -y -q builddep --define "_with_baseonly 1" /tmp/top/SPECS/kernel.spec
    rpmbuild -bb /tmp/top/SPECS/kernel.spec --define "_topdir /tmp/top" --define "buildid .1.bayu" --define "dist .el10_2" \
      --with baseonly --without debug --without debuginfo --without doc --without perf \
      --without libperf --without tools --without selftests --without kabichk \
      --without kernel_abi_stablelists --without zfcpdump --without efiuki --without realtime
    cp /tmp/top/RPMS/*/*.rpm /work/'
  cp "$work"/kernel*.x86_64.rpm "$CACHE/"
fi
cp "$CACHE"/kernel*.x86_64.rpm "$ROOT/out/repo/"
podman run --rm -v "$ROOT/out/repo":/repo:Z bayu-builder createrepo_c -q --update /repo
ls -l "$CACHE"
