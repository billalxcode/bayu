#!/usr/bin/env bash
# Signs the RPMs of out/rpms and out/repo that are not signed yet with the IGOS
# repository key (its public half: https://r2.igos-nusantara.or.id/26/RPM-GPG-KEY-IGN-REPO),
# then refreshes out/repo's metadata.
#   BAYU_REPO_GPG_KEY   the armored private key (release.yml takes it from a secret)
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
[[ -n ${BAYU_REPO_GPG_KEY:-} ]] || { echo "BAYU_REPO_GPG_KEY is not set" >&2; exit 1; }

# In the builder image, with rpm-sign added for the run.
podman run --rm -i -e BAYU_REPO_GPG_KEY -v "$ROOT/out":/out:Z bayu-builder bash -Eeuo pipefail -s <<'EOF'
dnf -y -q install rpm-sign >/dev/null
export GNUPGHOME=$(mktemp -d)
gpg --batch --quiet --import <<<"$BAYU_REPO_GPG_KEY"
key=$(gpg --batch --list-secret-keys --with-colons | awk -F: '/^fpr/ {print $10; exit}')
shopt -s nullglob
signed=0
for rpm in /out/rpms/*.rpm /out/repo/*.rpm; do
  [[ $(rpm -qp --qf '%{SIGPGP:pgpsig}%{RSAHEADER:pgpsig}' "$rpm" 2>/dev/null) == *"Key ID"* ]] && continue
  rpmsign --addsign --define "_gpg_name $key" "$rpm" >/dev/null
  signed=$((signed + 1))
done
[[ -d /out/repo ]] && createrepo_c -q --update /out/repo
echo "Signed $signed RPMs"
EOF
