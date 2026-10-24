#!/usr/bin/env bash
# Rebuilds the AlmaLinux kernel with packaging/kernel/kernel-local applied
# (binder for Android containers) and adds it to the out/repo dnf repository.
# The release gets .1.bayu, so it sorts above the AlmaLinux build it came from.
#   KERNEL=6.12.0-211.67.1.el10_2    AlmaLinux kernel version-release to rebuild
#   SRPM_URL=...                     where to fetch its source RPM
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/../..")
KERNEL=${KERNEL:-6.12.0-211.68.1.el10_2}
SRPM_URL=${SRPM_URL:-https://vault.almalinux.org/10.2/BaseOS/Source/Packages/kernel-$KERNEL.src.rpm}
mkdir -p "$ROOT/out/kernel" "$ROOT/out/repo"
[[ -f $ROOT/out/kernel/kernel-$KERNEL.src.rpm ]] || curl -fL -o "$ROOT/out/kernel/kernel-$KERNEL.src.rpm" "$SRPM_URL"

podman run --rm -v "$ROOT":/src:Z -w /src bayu-builder bash -euc '
  rpm -i --define "_topdir /tmp/top" out/kernel/kernel-'"$KERNEL"'.src.rpm
  cp packaging/kernel/kernel-local /tmp/top/SOURCES/kernel-local
  dnf -y -q builddep --define "_with_baseonly 1" /tmp/top/SPECS/kernel.spec
  rpmbuild -bb /tmp/top/SPECS/kernel.spec --define "_topdir /tmp/top" --define "buildid .1.bayu" --define "dist .el10_2" \
    --with baseonly --without debug --without debuginfo --without doc --without perf \
    --without libperf --without tools --without selftests --without kabichk \
    --without kernel_abi_stablelists --without zfcpdump --without efiuki --without realtime
  cp /tmp/top/RPMS/*/*.rpm out/kernel/'
cp "$ROOT"/out/kernel/kernel*.x86_64.rpm "$ROOT/out/repo/"
podman run --rm -v "$ROOT/out/repo":/repo:Z bayu-builder createrepo_c -q --update /repo
ls -l "$ROOT"/out/kernel/*.rpm
