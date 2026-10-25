#!/usr/bin/env bash
# Uploads out/igos-bayu-live.iso to R2 as repo/iso/nightly/ign-26-harian-latest.iso,
# replacing the last one, then rebuilds the index.html pages under repo/iso so they list
# every nightly ISO. Needs an rclone remote "r2", for instance from
# RCLONE_CONFIG_R2_* variables (see .github/workflows/release.yml).
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
ISO=${1:-$ROOT/out/igos-bayu-live.iso}
NAME=ign-26-harian-latest.iso

"$ROOT/builder/rclone.sh" copyto "$ISO" "r2:repo/iso/nightly/$NAME" --s3-upload-concurrency 8 --stats-one-line -v

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
echo "Published r2:repo/iso/nightly/$NAME"
