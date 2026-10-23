#!/usr/bin/env bash
# Builds every component in dependency order.
# Each component is installed to / (later components find earlier ones there)
# and, when STAGE is set, also to $STAGE for packaging. Meant to run as root
# inside a throwaway build container; see packaging/build-rpm.sh.
#   COMPONENTS="bayu-toolkit ..."   build only these
#   JOBS=N                          parallel jobs (default: nproc)
set -Eeuo pipefail

ROOT=$(realpath "$(dirname "$0")")
BUILD=${BUILD:-$ROOT/build}
JOBS=${JOBS:-$(nproc)}
COMPONENTS=${COMPONENTS:-"bayu-toolkit bayu-framework qt-plugins icons bayu-core bayu-dm
filemanager terminal xdg-desktop-portal-bayu settings system-monitor text-editor
calendar bayu-compositor wallpapers"}

[[ -d $ROOT/components/bayu-toolkit/third_party/skia ]] || "$ROOT/components/bayu-toolkit/scripts/fetch-skia.sh"

for c in $COMPONENTS; do
  echo "==> $c"
  cmake -S "$ROOT/components/$c" -B "$BUILD/$c" -G Ninja -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
  cmake --build "$BUILD/$c" --parallel "$JOBS"
  cmake --install "$BUILD/$c"
  [[ -z ${STAGE:-} ]] || DESTDIR=$STAGE cmake --install "$BUILD/$c"
done
