#!/usr/bin/env bash
# Rebuilds Fedora's usb_modeswitch and usb_modeswitch-data, which EL10 lacks,
# into out/repo. Many USB Wi-Fi adapters (and modems) first show up as a CD
# with their Windows driver, and only become a network device once
# usb_modeswitch switches them (issue #73). A build is kept in
# MODESWITCH_CACHE and reused: releases only rebuild it when a version changes.
#   MODESWITCH=2.6.2-6.fc45              Fedora's usb_modeswitch version-release
#   MODESWITCH_DATA=20191128-16.fc45     and usb_modeswitch-data's
#   MODESWITCH_CACHE=~/.cache/bayu-usb-modeswitch
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/../..")
MODESWITCH=${MODESWITCH:-2.6.2-6.fc45}
MODESWITCH_DATA=${MODESWITCH_DATA:-20191128-16.fc45}
CACHE=${MODESWITCH_CACHE:-$HOME/.cache/bayu-usb-modeswitch}/$MODESWITCH-$MODESWITCH_DATA
mkdir -p "$CACHE" "$ROOT/out/repo"

srpm() { echo "https://kojipkgs.fedoraproject.org/packages/$1/${2%-*}/${2#*-}/src/$1-$2.src.rpm"; }

if ! compgen -G "$CACHE/usb_modeswitch-data-*.rpm" >/dev/null; then
  work=$(mktemp -d) && trap 'rm -rf "$work"' EXIT
  curl -fL -o "$work/usb_modeswitch.src.rpm" "$(srpm usb_modeswitch "$MODESWITCH")"
  curl -fL -o "$work/usb_modeswitch-data.src.rpm" "$(srpm usb_modeswitch-data "$MODESWITCH_DATA")"
  podman run --rm -v "$work":/work:Z bayu-builder bash -euc '
    for srpm in /work/*.src.rpm; do
      dnf -y -q builddep "$srpm" >/dev/null
      rpmbuild --rebuild "$srpm" --define "_topdir /tmp/top" --define "dist .el10.bayu" --define "debug_package %{nil}"
    done
    cp /tmp/top/RPMS/*/*.rpm /work/'
  # Binary RPMs (arch-specific plus noarch data); never the .src.rpm.
  find "$work" -maxdepth 1 -name '*.rpm' ! -name '*.src.rpm' -exec cp -t "$CACHE/" {} +
fi
cp "$CACHE"/*.rpm "$ROOT/out/repo/"
podman run --rm -v "$ROOT/out/repo":/repo:Z bayu-builder createrepo_c -q --update /repo
ls -l "$CACHE"
