#!/usr/bin/env bash
# Adds the binary RPMs of out/repo (bayu-desktop, nabau, lapis) to the Bayu dnf
# repository on R2, repo/26/bayu (https://r2.igos-nusantara.or.id/26/bayu/),
# then has the IGOS mirror host pull it into /var/www/html/26/bayu. R2 is the
# source: the host's own daily sync of 26/ leaves bayu/ alone. Earlier
# versions stay, for downgrades.
# Needs an rclone remote "r2" (RCLONE_CONFIG_R2_*, see release.yml), the
# signing key (BAYU_REPO_GPG_KEY, see sign-rpms.sh) and SSH to the host.
#   BAYU_REPO_HOST=user@host   default igos@103.175.219.85
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
HOST=${BAYU_REPO_HOST:-igos@103.175.219.85}

shopt -s nullglob
rpms=("$ROOT"/out/repo/*.x86_64.rpm "$ROOT"/out/repo/*.noarch.rpm)
((${#rpms[@]})) || { echo "No RPMs in out/repo" >&2; exit 1; }

repo=$(mktemp -d) && trap 'rm -rf "$repo"' EXIT
"$ROOT/builder/rclone.sh" copy r2:repo/26/bayu "$repo" --exclude index.html
cp "${rpms[@]}" "$repo/"
# Every package in it signed, earlier releases included.
"$ROOT/builder/sign-rpms.sh" "$repo"
podman run --rm -v "$repo":/repo:Z bayu-builder createrepo_c -q --update /repo
"$ROOT/builder/rclone.sh" sync "$repo" r2:repo/26/bayu --exclude index.html --checksum --stats-one-line -v

# Its index.html pages, as builder/mkindex.py draws the bucket's; paths from
# the bucket root.
idx=$(mktemp -d) && trap 'rm -rf "$repo" "$idx"' EXIT
"$ROOT/builder/rclone.sh" lsjson -R --files-only --use-server-modtime --no-mimetype r2:repo/26/bayu |
  python3 -c 'import json, sys
entries = json.load(sys.stdin)
for e in entries:
    e["Path"] = "26/bayu/" + e["Path"]
json.dump(entries, sys.stdout)' |
  python3 "$ROOT/builder/mkindex.py" "$idx"
"$ROOT/builder/rclone.sh" copy "$idx/26/bayu" r2:repo/26/bayu --include index.html --checksum -v

ssh -o BatchMode=yes "$HOST" \
  "rclone sync r2:repo/26/bayu /var/www/html/26/bayu --exclude index.html --checksum --stats-one-line -v 2>&1 | tail -1"
echo "Published ${#rpms[@]} RPMs to repo/26/bayu"
