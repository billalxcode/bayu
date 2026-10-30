#!/usr/bin/env bash
# Builds the bayu-desktop RPMs in the AlmaLinux 10 build container and
# publishes them as a dnf repository in out/repo, ready for builder/.
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
cd "$ROOT"

podman build -t bayu-builder ${ALMA_MIRROR:+--build-arg ALMA_MIRROR=$ALMA_MIRROR} \
  ${EPEL_MIRROR:+--build-arg EPEL_MIRROR=$EPEL_MIRROR} -f packaging/Containerfile packaging
podman run --rm -v "$ROOT":/src:Z -w /src bayu-builder packaging/rpmbuild.sh
mkdir -p out/repo
# Binary RPMs for the build host's arch (x86_64 or aarch64) plus noarch; never
# the .src.rpm.
find out/rpms -maxdepth 1 -name '*.rpm' ! -name '*.src.rpm' -exec cp -t out/repo/ {} +
podman run --rm -v "$ROOT/out/repo":/repo:Z bayu-builder createrepo_c -q --update /repo
ls -l out/repo/*.rpm
