#!/usr/bin/env bash
# Uploads out/igos-bayu-live.iso to R2 as repo/iso/nightly/ign26-bayu-harian-<timestamp>.iso
# (WIB) and copies it to ign26-bayu-harian-latest.iso, then rebuilds the index.html pages
# under repo/iso so they list every nightly ISO. R2 has no symlinks: latest is a server-side
# copy, not a second upload. Timestamped ISOs older than KEEP_DAYS are deleted.
# Needs an rclone remote "r2", for instance from RCLONE_CONFIG_R2_* variables (see
# .github/workflows/release.yml).
#   KEEP_DAYS=14
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
ISO=${1:-$ROOT/out/igos-bayu-live.iso}
DIR=r2:repo/iso/nightly
NAME=ign26-bayu-harian-$(TZ=Asia/Jakarta date +%Y%m%d-%H%M).iso
LATEST=ign26-bayu-harian-latest.iso

"$ROOT/builder/rclone.sh" copyto "$ISO" "$DIR/$NAME" --s3-upload-concurrency 8 --stats-one-line -v
"$ROOT/builder/rclone.sh" copyto "$DIR/$NAME" "$DIR/$LATEST" -v
# ign-26-harian-latest.iso: the name before ign26-bayu-harian-*.
"$ROOT/builder/rclone.sh" delete "$DIR" --include ign-26-harian-latest.iso -v
"$ROOT/builder/rclone.sh" delete "$DIR" --include 'ign26-bayu-harian-2*.iso' --min-age "${KEEP_DAYS:-14}d" -v

# mkindex.py takes paths from the bucket root: put iso/ back in front.
idx=$(mktemp -d) && trap 'rm -rf "$idx"' EXIT
"$ROOT/builder/rclone.sh" lsjson -R --files-only --use-server-modtime --no-mimetype r2:repo/iso |
  python3 -c 'import json, sys
entries = json.load(sys.stdin)
for e in entries:
    e["Path"] = "iso/" + e["Path"]
json.dump(entries, sys.stdout)' |
  python3 "$ROOT/builder/mkindex.py" "$idx"
"$ROOT/builder/rclone.sh" copy "$idx/iso" r2:repo/iso --include index.html --checksum -v
echo "Published $DIR/$NAME and $DIR/$LATEST"
