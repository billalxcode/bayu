#!/usr/bin/env bash
# Builds a bootable Bayu OS disk image (qcow2, BIOS/GPT) on AlmaLinux.
# Settings come from bayu.conf (or CONFIG=path); packages come from the
# *.repo files in repos.d/ (or REPOS=dir), so swapping mirrors or adding repos
# is a config change. The work runs in a privileged AlmaLinux container
# because it needs loop devices; output lands in out/<IMAGE_NAME>.qcow2.
set -Eeuo pipefail

HERE=$(realpath "$(dirname "$0")")
ROOT=$(realpath "$HERE/..")
CONFIG=$(realpath "${CONFIG:-$HERE/bayu.conf}")
REPOS=$(realpath "${REPOS:-$HERE/repos.d}")

if [[ ${1:-} != --inside ]]; then
  mkdir -p "$ROOT/out/repo"
  # Loop devices need a rootful container.
  PODMAN="podman"; [[ $EUID == 0 ]] || PODMAN="sudo podman"
  $PODMAN image exists bayu-builder || $PODMAN build -t bayu-builder -f "$ROOT/packaging/Containerfile" "$ROOT/packaging"
  exec $PODMAN run --rm --privileged --network host -v /dev:/dev -v "$HERE":/builder:ro -v "$CONFIG":/bayu.conf:ro \
    -v "$REPOS":/repos.d:ro -v "$ROOT/out/repo":/repo:ro -v "$ROOT/out":/out \
    bayu-builder bash /builder/build-image.sh --inside
fi

# --- inside the container ---------------------------------------------------
source /bayu.conf
dnf -y -q install qemu-img gdisk xfsprogs util-linux >/dev/null

RAW=/out/$IMAGE_NAME.raw
MNT=/mnt/image
cleanup() {
  umount -R "$MNT" 2>/dev/null || true
  [[ -n ${LOOP:-} ]] && losetup -d "$LOOP" 2>/dev/null || true
}
trap cleanup EXIT

rm -f "$RAW" && truncate -s "$IMAGE_SIZE" "$RAW"
sgdisk -o -n 1:0:+1M -t 1:EF02 -c 1:bios -n 2:0:0 -t 2:8300 -c 2:root "$RAW"
LOOP=$(losetup -P --find --show "$RAW")
for _ in 1 2 3 4 5; do [[ -b ${LOOP}p2 ]] && break; sleep 1; done
# Inside a container udev may not create the partition nodes.
[[ -b ${LOOP}p2 ]] || mknod "${LOOP}p2" b "$(cat /sys/block/${LOOP#/dev/}/${LOOP#/dev/}p2/dev | tr : ' ')"
mkfs.xfs -q -L bayu-root "${LOOP}p2"
UUID=$(blkid -s UUID -o value "${LOOP}p2")

mkdir -p "$MNT" && mount "${LOOP}p2" "$MNT"
mkdir -p "$MNT"/{dev,proc,sys,etc/kernel}
mount --bind /dev "$MNT/dev" && mount -t proc proc "$MNT/proc" && mount -t sysfs sys "$MNT/sys"
# psi=1: Android's lmkd (lapis) needs /proc/pressure, which EL kernels keep off.
CMDLINE="root=UUID=$UUID ro console=tty0 console=ttyS0,115200 psi=1"
echo "$CMDLINE" > "$MNT/etc/kernel/cmdline"
# Kernel installs and updates become the boot default.
mkdir -p "$MNT/etc/sysconfig"
printf 'UPDATEDEFAULT=yes\nDEFAULTKERNEL=kernel-core\n' > "$MNT/etc/sysconfig/kernel"
echo "UUID=$UUID / xfs defaults 0 0" > "$MNT/etc/fstab"

# shellcheck disable=SC2086
dnf -y --installroot="$MNT" --releasever="$RELEASEVER" --setopt=reposdir=/repos.d \
  --setopt=install_weak_deps=False --setopt=tsflags=nodocs install $PACKAGES
# Inside the image, use the same repositories it was built from; a file:///repo
# source only exists at build time, so it is left disabled.
rm -f "$MNT"/etc/yum.repos.d/*.repo && cp /repos.d/*.repo "$MNT/etc/yum.repos.d/"
grep -l 'file:///repo' "$MNT"/etc/yum.repos.d/*.repo | xargs -r sed -i 's/^enabled=1/enabled=0/'

chr() { chroot "$MNT" "$@"; }
echo "$HOSTNAME" > "$MNT/etc/hostname"
echo "LANG=$LOCALE" > "$MNT/etc/locale.conf"
echo "KEYMAP=$KEYMAP" > "$MNT/etc/vconsole.conf"
ln -sf "../usr/share/zoneinfo/$TIMEZONE" "$MNT/etc/localtime"
sed -i "s/^SELINUX=.*/SELINUX=$SELINUX/" "$MNT/etc/selinux/config"
touch "$MNT/.autorelabel"

chr useradd -m -G wheel "$USER_NAME"
echo "$USER_NAME:$USER_PASSWORD" | chr chpasswd
if [[ -n $ROOT_PASSWORD ]]; then echo "root:$ROOT_PASSWORD" | chr chpasswd; else chr passwd -l root; fi
if [[ ${SUDO_NOPASSWD:-no} == yes ]]; then
  echo "$USER_NAME ALL=(ALL) NOPASSWD: ALL" > "$MNT/etc/sudoers.d/90-$USER_NAME"
  chmod 440 "$MNT/etc/sudoers.d/90-$USER_NAME"
fi
if [[ $AUTOLOGIN == yes ]]; then
  printf '[Autologin]\nUser=%s\n' "$USER_NAME" > "$MNT/etc/bayu-dm.conf"
fi
chr systemctl enable bayu-dm.service NetworkManager.service
chr systemctl set-default graphical.target

# Bootloader: BLS entries made by kernel-install carry the build host's
# cmdline, so point them at the image's root before generating grub.cfg.
sed -i "s|^options .*|options $CMDLINE|" "$MNT"/boot/loader/entries/*.conf
cat > "$MNT/etc/default/grub" <<EOF
GRUB_TIMEOUT=3
GRUB_DISTRIBUTOR="Bayu"
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

# Each boot of a copy of this image gets its own machine-id.
: > "$MNT/etc/machine-id"

cleanup
qemu-img convert -O qcow2 "$RAW" "/out/$IMAGE_NAME.qcow2" && rm -f "$RAW"
echo "Built /out/$IMAGE_NAME.qcow2"
