#!/usr/bin/env bash
# rclone, pinned, in a container: the runner's own (Ubuntu's 1.60) fails every
# first request to R2 with NotImplemented. The RCLONE_* variables pass through
# (the "r2" remote, see release.yml), and the repository, /tmp and the current
# directory are mounted at their own paths, so arguments read as on the host.
#   builder/rclone.sh ARGS...
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
IMAGE=docker.io/rclone/rclone:1.71.2

exec podman run --rm -i --network host --env 'RCLONE_*' \
  -v "$ROOT":"$ROOT" -v /tmp:/tmp -v "$PWD":"$PWD" -w "$PWD" \
  "$IMAGE" "$@"
