#!/usr/bin/env bash
# Builds every component in dependency order.
# With STAGE set, each component is installed only into $STAGE and later
# components find earlier ones there (this is what the RPM does). Without it,
# each component is installed to / as upstream intends, which needs root.
#   COMPONENTS="bayu-toolkit ..."   build only these
#   JOBS=N                          parallel jobs (default: nproc)
set -Eeuo pipefail

ROOT=$(realpath "$(dirname "$0")")
BUILD=${BUILD:-$ROOT/build}
JOBS=${JOBS:-$(nproc)}
COMPONENTS=${COMPONENTS:-"bayu-toolkit bayu-framework qt-plugins icons bayu-core bayu-dm
filemanager terminal xdg-desktop-portal-bayu settings system-monitor text-editor
calendar bayu-compositor wallpapers gtk-theme nabau bayu-store"}

[[ -d $ROOT/components/bayu-toolkit/third_party/skia ]] || "$ROOT/components/bayu-toolkit/scripts/fetch-skia.sh"

if [[ -n ${STAGE:-} ]]; then
  STAGE=$(realpath -m "$STAGE")
  export CMAKE_PREFIX_PATH=$STAGE/usr
  export PKG_CONFIG_PATH=$STAGE/usr/lib64/pkgconfig:$STAGE/usr/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
  export LD_LIBRARY_PATH=$STAGE/usr/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
  export PATH=$STAGE/usr/bin:$PATH
fi

for c in $COMPONENTS; do
  echo "==> $c"
  cmake -S "$ROOT/components/$c" -B "$BUILD/$c" -G Ninja -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
  cmake --build "$BUILD/$c" --parallel "$JOBS"
  if [[ -n ${STAGE:-} ]]; then DESTDIR=$STAGE cmake --install "$BUILD/$c"; else cmake --install "$BUILD/$c"; fi
done
