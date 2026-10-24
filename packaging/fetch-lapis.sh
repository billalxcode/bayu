#!/usr/bin/env bash
# Adds a lapis release RPM (Android apps, github.com/igos-nusantara/lapis) to
# the out/repo dnf repository, verified against the release's SHA256SUMS.
#   packaging/fetch-lapis.sh [VERSION]    default: the latest release
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
REPO=igos-nusantara/lapis
VERSION=${1:-$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" | sed -n 's/.*"tag_name": *"v\([^"]*\)".*/\1/p')}
URL=https://github.com/$REPO/releases/download/v$VERSION
RPM=lapis-$VERSION-1.x86_64.rpm

tmp=$(mktemp -d) && trap 'rm -rf "$tmp"' EXIT
curl -fsSL -o "$tmp/$RPM" "$URL/$RPM"
curl -fsSL -o "$tmp/SHA256SUMS" "$URL/SHA256SUMS"
(cd "$tmp" && grep " $RPM\$" SHA256SUMS | sha256sum -c --quiet)
mkdir -p "$ROOT/out/repo" && rm -f "$ROOT"/out/repo/lapis-*.rpm && cp "$tmp/$RPM" "$ROOT/out/repo/"
podman run --rm -v "$ROOT/out/repo":/repo:Z bayu-builder createrepo_c -q --update /repo
echo "Added $RPM to out/repo"
