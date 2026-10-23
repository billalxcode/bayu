#!/usr/bin/env bash
# Builds a bootable disk image (qcow2, BIOS/GPT) of a profile in
# builder/profiles/ (PROFILE, default igos).
# Output: out/<IMAGE_NAME>.qcow2. See lib.sh for CONFIG and REPOS.
set -Eeuo pipefail
source "$(dirname "$0")/lib.sh"
[[ ${1:-} == --inside ]] || run_in_builder

# --- inside the container ---------------------------------------------------
source /profile.conf
dnf -y -q install qemu-img gdisk xfsprogs util-linux >/dev/null

RAW=/out/$IMAGE_NAME.raw
MNT=/mnt/image
cleanup() {
  release_root "$MNT"
  # The image is only consistent once its filesystem is cleanly unmounted.
  if mountpoint -q "$MNT"; then umount "$MNT" || return 1; fi
  if [[ -n ${LOOP:-} ]]; then losetup -d "$LOOP" || true; LOOP=; fi
}
trap cleanup EXIT

rm -f "$RAW" && truncate -s "$IMAGE_SIZE" "$RAW"
sgdisk -o -n 1:0:+1M -t 1:EF02 -c 1:bios -n 2:0:0 -t 2:8300 -c 2:root "$RAW"
LOOP=$(losetup -P --find --show "$RAW")
for _ in 1 2 3 4 5; do [[ -b ${LOOP}p2 ]] && break; sleep 1; done
# Inside a container udev may not create the partition nodes.
[[ -b ${LOOP}p2 ]] || mknod "${LOOP}p2" b "$(cat /sys/block/${LOOP#/dev/}/${LOOP#/dev/}p2/dev | tr : ' ')"
mkfs.xfs -q -L root "${LOOP}p2"
UUID=$(blkid -s UUID -o value "${LOOP}p2")
mkdir -p "$MNT" && mount "${LOOP}p2" "$MNT"

# psi=1: Android's lmkd (lapis) needs /proc/pressure, which EL kernels keep off.
CMDLINE="root=UUID=$UUID ro console=tty0 console=ttyS0,115200 psi=1"
mkdir -p "$MNT/etc/kernel" && echo "$CMDLINE" > "$MNT/etc/kernel/cmdline"
echo "UUID=$UUID / xfs defaults 0 0" > "$MNT/etc/fstab"
install_root "$MNT"
configure_root "$MNT"
touch "$MNT/.autorelabel"

chr() { chroot "$MNT" "$@"; }
# BLS entries made by kernel-install carry the build host's cmdline, so point
# them at the image's root before generating grub.cfg.
sed -i "s|^options .*|options $CMDLINE|" "$MNT"/boot/loader/entries/*.conf
cat > "$MNT/etc/default/grub" <<EOF
GRUB_TIMEOUT=3
GRUB_DISTRIBUTOR="$DISTRO_NAME"
GRUB_DEFAULT=saved
GRUB_TERMINAL_OUTPUT="console serial"
GRUB_SERIAL_COMMAND="serial --speed=115200"
GRUB_CMDLINE_LINUX="$CMDLINE"
GRUB_ENABLE_BLSCFG=true
EOF
chr dracut --force --no-hostonly --regenerate-all
chr grub2-install --target=i386-pc "$LOOP"
chr grub2-mkconfig -o /boot/grub2/grub.cfg
# A fresh image has one kernel (dnf picks the highest, Bayu's binder kernel when
# bayu.repo carries it); make it the saved default, which GRUB_DEFAULT=saved reads.
chr grubby --set-default "$(ls -v "$MNT"/boot/vmlinuz-* | grep -v rescue | tail -1 | sed "s|^$MNT||")"

cleanup
qemu-img convert -O qcow2 "$RAW" "/out/$IMAGE_NAME.qcow2" && rm -f "$RAW"
echo "Built /out/$IMAGE_NAME.qcow2"
