#!/usr/bin/env bash
# Builds a live ISO of a profile in builder/profiles/ (PROFILE, default
# igos). The ISO boots on BIOS and UEFI machines, from
# optical media or written to a USB stick (dd), and starts the desktop with
# the profile's user logged in. Output: out/<IMAGE_NAME>-live.iso.
# See lib.sh for CONFIG and REPOS.
set -Eeuo pipefail
source "$(dirname "$0")/lib.sh"
[[ ${1:-} == --inside ]] || run_in_builder

# --- inside the container ---------------------------------------------------
source /profile.conf
dnf -y -q install grub2-tools grub2-tools-extra grub2-efi-x64-modules grub2-pc-modules \
  xorriso mtools squashfs-tools >/dev/null

WORK=/var/tmp/live
ROOTFS=$WORK/rootfs
TREE=$WORK/iso
ISO=/out/$IMAGE_NAME-live.iso
trap 'release_root "$ROOTFS"' EXIT
rm -rf "$WORK" && mkdir -p "$TREE"/{LiveOS,boot/grub}

# psi=1: Android's lmkd (lapis) needs /proc/pressure, which EL kernels keep
# off. The live system is not SELinux-labelled, so SELinux stays off there.
CMDLINE="root=live:CDLABEL=$ISO_LABEL rd.live.image rd.live.overlay.overlayfs=1 psi=1 selinux=0"
mkdir -p "$ROOTFS/etc/kernel" && echo "$CMDLINE" > "$ROOTFS/etc/kernel/cmdline"
: > "$ROOTFS/etc/fstab"
install_root "$ROOTFS" dracut-live
configure_root "$ROOTFS"

# The installed kernel is the one with a vmlinuz (kmod packages create bare
# module trees for the kernel they were built against).
KVER=$(basename "$(dirname "$(ls -v "$ROOTFS"/lib/modules/*/vmlinuz | tail -1)")")
chroot "$ROOTFS" dracut --force --no-hostonly --add dmsquash-live --omit plymouth \
  /boot/initramfs-live.img "$KVER"
cp "$ROOTFS/lib/modules/$KVER/vmlinuz" "$TREE/boot/vmlinuz"
mv "$ROOTFS/boot/initramfs-live.img" "$TREE/boot/initrd.img"
release_root "$ROOTFS"
rm -rf "$ROOTFS"/tmp/* "$ROOTFS"/var/cache/dnf/*

mksquashfs "$ROOTFS" "$TREE/LiveOS/squashfs.img" -comp zstd -noappend -quiet
cat > "$TREE/boot/grub/grub.cfg" <<EOF
set default=0
set timeout=5
menuentry "Start $DISTRO_NAME" {
  linux /boot/vmlinuz $CMDLINE quiet
  initrd /boot/initrd.img
}
menuentry "Start $DISTRO_NAME (basic graphics)" {
  linux /boot/vmlinuz $CMDLINE nomodeset
  initrd /boot/initrd.img
}
EOF
grub2-mkrescue -o "$ISO" "$TREE" -- -volid "$ISO_LABEL"
echo "Built $ISO ($(du -h "$ISO" | cut -f1), kernel $KVER)"
