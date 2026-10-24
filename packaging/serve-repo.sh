#!/usr/bin/env bash
# Serves out/repo over HTTP so running systems can install new builds with
# `sudo dnf upgrade --refresh` instead of a new image. Listens on this host's
# Tailscale address only (or ADDR); point systems at it with BAYU_REPO_URL in
# a builder profile, or a repo file:
#   [bayu-dev]
#   baseurl=http://<address>:8090/
#   gpgcheck=0
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
ADDR=${ADDR:-$(tailscale ip -4 | head -1)}
PORT=${PORT:-8090}
mkdir -p "$ROOT/out/repo"
echo "Serving $ROOT/out/repo at http://$ADDR:$PORT/"
exec python3 -m http.server --bind "$ADDR" --directory "$ROOT/out/repo" "$PORT"
