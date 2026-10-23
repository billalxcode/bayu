#!/usr/bin/env bash
# Builds Bayu inside the AlmaLinux build container and publishes the RPM as a
# dnf repository in out/repo, ready for builder/ to install from.
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
cd "$ROOT"

podman build -t bayu-builder ${ALMA_MIRROR:+--build-arg ALMA_MIRROR=$ALMA_MIRROR} ${EPEL_MIRROR:+--build-arg EPEL_MIRROR=$EPEL_MIRROR} -f packaging/Containerfile packaging
podman run --rm -v "$ROOT":/src:Z -w /src -e JOBS="${JOBS:-}" bayu-builder bash -euc '
  rm -rf /tmp/stage && STAGE=/tmp/stage ./build.sh
  mkdir -p ~/rpmbuild/SOURCES
  tar -C /tmp/stage -cf ~/rpmbuild/SOURCES/bayu-stage.tar .
  for f in bayu-dm bayu-dm-greeter bayu-auth; do cp packaging/pam/$f ~/rpmbuild/SOURCES/$f.pam; done
  rpmbuild -bb packaging/bayu-desktop.spec --define "bayu_release $(date +%Y%m%d%H%M)"
  mkdir -p out/repo && cp ~/rpmbuild/RPMS/*/*.rpm out/repo/ && createrepo_c -q out/repo'
ls -l out/repo/*.rpm
