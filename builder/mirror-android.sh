#!/usr/bin/env bash
# Mirrors the Android images lapis installs (Waydroid's LineageOS: the system
# image, VANILLA, and the vendor image, MAINLINE) to R2 under android/ota/, in
# Waydroid's OTA channel format. Bayu's android-setup has lapis download them
# from there first: ota.waydro.id and SourceForge's mirrors are out of reach,
# or slower than lapis allows, from much of Indonesia (issue #79). Only the
# newest build of each is kept. Needs an rclone remote "r2", for instance from
# RCLONE_CONFIG_R2_* variables (see .github/workflows/release.yml).
#   ANDROID_MIRROR=https://r2.igos-nusantara.or.id/android/ota
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
MIRROR=${ANDROID_MIRROR:-https://r2.igos-nusantara.or.id/android/ota}
UPSTREAM=https://ota.waydro.id
DIR=r2:repo/android/ota
RCLONE=${RCLONE:-$ROOT/builder/rclone.sh}
work=$(mktemp -d) && trap 'rm -rf "$work"' EXIT

keep=()
for channel in system/lineage/waydroid_x86_64/VANILLA vendor/waydroid_x86_64/MAINLINE; do
  curl -fsSL --retry 3 -o "$work/upstream.json" "$UPSTREAM/$channel.json"
  # The newest build, with its URL on the mirror.
  read -r filename sha url < <(python3 - "$work/upstream.json" "$work/channel.json" "$MIRROR/files" <<'PY'
import json, sys
newest = max(json.load(open(sys.argv[1]))["response"], key=lambda e: e["datetime"])
upstream = newest["url"]
newest["url"] = sys.argv[3] + "/" + newest["filename"]
json.dump({"response": [newest]}, open(sys.argv[2], "w"), indent=4)
print(newest["filename"], newest["id"], upstream)
PY
)
  keep+=("$filename")
  if [ -z "$("$RCLONE" lsf "$DIR/files/$filename" 2>/dev/null)" ]; then
    echo "Mirroring $filename"
    curl -fsSL --retry 5 --retry-all-errors -C - -o "$work/$filename" "$url"
    echo "$sha  $work/$filename" | sha256sum -c --quiet
    "$RCLONE" copyto "$work/$filename" "$DIR/files/$filename" --s3-upload-concurrency 8 --stats-one-line -v
    rm -f "$work/$filename"
  fi
  # The channel last, once its image is there.
  "$RCLONE" copyto "$work/channel.json" "$DIR/$channel.json" -v
done

# Older builds go.
"$RCLONE" lsf "$DIR/files" | while read -r file; do
  [[ " ${keep[*]} " == *" $file "* ]] || "$RCLONE" deletefile "$DIR/files/$file" -v
done
echo "Mirrored ${keep[*]} to $MIRROR"
