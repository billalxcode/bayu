#!/usr/bin/env bash
# Signs the RPMs not signed yet in the given directories (default: out/rpms
# and out/repo) with the IGOS repository key, whose public half is
# https://r2.igos-nusantara.or.id/26/RPM-GPG-KEY-IGN-REPO. A directory with
# repodata gets its metadata refreshed.
#   BAYU_REPO_GPG_KEY   the armored private key (release.yml takes it from a secret)
#   sign-rpms.sh [DIR...]
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
[[ -n ${BAYU_REPO_GPG_KEY:-} ]] || { echo "BAYU_REPO_GPG_KEY is not set" >&2; exit 1; }
dirs=("$@")
((${#dirs[@]})) || dirs=("$ROOT/out/rpms" "$ROOT/out/repo")

mounts=()
for i in "${!dirs[@]}"; do
  [[ -d ${dirs[$i]} ]] && mounts+=(-v "$(realpath "${dirs[$i]}")":/sign/$i:Z)
done
((${#mounts[@]})) || exit 0

# In the builder image, with rpm-sign added for the run.
podman run --rm -i -e BAYU_REPO_GPG_KEY "${mounts[@]}" bayu-builder bash -Eeuo pipefail -s <<'EOF'
dnf -y -q install rpm-sign >/dev/null
export GNUPGHOME=$(mktemp -d)
gpg --batch --quiet --import <<<"$BAYU_REPO_GPG_KEY"
key=$(gpg --batch --list-secret-keys --with-colons | awk -F: '/^fpr/ {print $10; exit}')
shopt -s nullglob
signed=0
for dir in /sign/*; do
  for rpm in "$dir"/*.rpm; do
    [[ $(rpm -qp --qf '%{SIGPGP:pgpsig}%{RSAHEADER:pgpsig}' "$rpm" 2>/dev/null) == *"Key ID"* ]] && continue
    rpmsign --addsign --define "_gpg_name $key" "$rpm" >/dev/null 2>&1
    signed=$((signed + 1))
  done
  [[ -d $dir/repodata ]] && createrepo_c -q --update "$dir"
done
echo "Signed $signed RPMs"
EOF
