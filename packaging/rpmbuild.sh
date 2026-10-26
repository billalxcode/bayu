#!/usr/bin/env bash
# Builds the bayu-desktop .src.rpm and .rpm from this checkout into out/rpms.
# Runs on AlmaLinux 10 with the BuildRequires installed (dnf builddep), as
# packaging/build-rpm.sh and .github/workflows/rpm.yml do.
# Version: a vX.Y.Z tag on HEAD gives X.Y.Z-1; otherwise the last tag (or
# 0.1.0) with a 0.<date>git<sha> snapshot release.
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
cd "$ROOT"
git config --global --add safe.directory "$ROOT"

if tag=$(git describe --tags --exact-match --match 'v[0-9]*' 2>/dev/null); then
  version=${tag#v}; release=1
else
  last=$(git describe --tags --abbrev=0 --match 'v[0-9]*' 2>/dev/null || echo v0.1.0)
  version=${last#v}; release=0.$(date -u +%Y%m%d%H%M)git$(git rev-parse --short HEAD)
fi

top=$(mktemp -d)
mkdir -p "$top/SOURCES"
git archive --format=tar.gz --prefix="bayu-$version/" -o "$top/SOURCES/bayu-$version.tar.gz" HEAD
skia=components/bayu-toolkit/third_party
[[ -d $skia/skia ]] || components/bayu-toolkit/scripts/fetch-skia.sh
tar -C "$skia" --exclude=.git -czf "$top/SOURCES/skia.tar.gz" skia
# bayu-computer-use's crates, with the Cargo.lock that pins them, so the
# .src.rpm builds offline. Its .cargo/config.toml is for Debian's packages.
cu=$(mktemp -d)
cp -a components/bayu-computer-use/. "$cu"
rm -rf "$cu/.cargo" "$cu/target"
(cd "$cu" && cargo vendor --quiet vendor >/dev/null)
tar -C "$cu" -czf "$top/SOURCES/computer-use-vendor.tar.gz" vendor Cargo.lock
rm -rf "$cu"

rpmbuild -ba packaging/bayu-desktop.spec --define "_topdir $top" \
  --define "bayu_version $version" --define "bayu_release $release"
mkdir -p out/rpms
cp "$top"/SRPMS/*.src.rpm "$top"/RPMS/*/*.rpm out/rpms/
rm -rf "$top"
ls -l out/rpms
