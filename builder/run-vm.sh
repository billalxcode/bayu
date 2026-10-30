#!/usr/bin/env bash
# Boots a disk image (.qcow2) or live ISO (.iso) in QEMU/KVM and serves its
# screen over VNC.
#   VNC_ADDR=100.x.y.z    listen address (default: this host's Tailscale IP)
#   VNC_DISPLAY=1         VNC display, i.e. port 5900+N
#   GL=yes|no             virgl 3D through the host GPU (egl-headless), or
#                         a plain virtio-gpu with software GL in the guest
#   SNAPSHOT=yes          throw away disk writes on exit
#   UEFI=yes              boot with OVMF firmware instead of BIOS (as on a Mac)
set -Eeuo pipefail
ROOT=$(realpath "$(dirname "$0")/..")
IMAGE=${1:-$ROOT/out/igos-bayu.qcow2}
VNC_ADDR=${VNC_ADDR:-$(tailscale ip -4 | head -1)}
VNC_DISPLAY=${VNC_DISPLAY:-1}
GL=${GL:-yes}
NATIVE_ARCH=$(uname -m)
case "$NATIVE_ARCH" in
  aarch64|arm64)
    QEMU=${QEMU:-qemu-system-aarch64}
    MACHINE=${MACHINE:-virt}
    CPU=${CPU:-host}
    # EDK2 AArch64 firmware (AlmaLinux: edk2-aarch64). Override with OVMF_CODE/VARS.
    OVMF_CODE=${OVMF_CODE:-/usr/share/EDK2/aarch64/QEMU_EFI.fd}
    ;;
  *)
    QEMU=${QEMU:-qemu-system-x86_64}
    MACHINE=${MACHINE:-q35}
    CPU=${CPU:-host}
    OVMF_CODE=${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE_4M.fd}
    ;;
esac

if [[ $GL == yes ]]; then
  GPU=(-device virtio-vga-gl -display egl-headless,rendernode=${RENDERNODE:-/dev/dri/renderD128})
else
  GPU=(-device virtio-vga -display none)
fi

if [[ $IMAGE == *.iso ]]; then
  DISK=(-cdrom "$IMAGE")
else
  DISK=(-drive "file=$IMAGE,if=virtio,format=qcow2" ${SNAPSHOT:+-snapshot})
fi
FIRMWARE=()
if [[ ${UEFI:-no} == yes ]]; then
  vars=$ROOT/out/uefi-vars-$VNC_DISPLAY.fd
  if [[ $NATIVE_ARCH == aarch64 || $NATIVE_ARCH == arm64 ]]; then
    # AArch64 EDK2 is a single combined image: copy it as the pflash vars.
    [[ -f $vars ]] || cp "${OVMF_VARS:-$OVMF_CODE}" "$vars"
    FIRMWARE=(-drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE"
              -drive "if=pflash,format=raw,file=$vars")
  else
    [[ -f $vars ]] || cp "${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS_4M.fd}" "$vars"
    FIRMWARE=(-drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE"
              -drive "if=pflash,format=raw,file=$vars")
  fi
fi

echo "VNC: $VNC_ADDR:$((5900 + VNC_DISPLAY)) ($QEMU/$MACHINE)"
exec $QEMU -name bayu -machine $MACHINE,accel=kvm -cpu $CPU -smp "${CPUS:-4}" -m "${MEM:-4G}" \
  "${FIRMWARE[@]}" "${DISK[@]}" \
  -nic user,model=virtio-net-pci "${GPU[@]}" -vnc "$VNC_ADDR:$VNC_DISPLAY" \
  -device qemu-xhci -device usb-tablet -audiodev none,id=snd \
  -serial "file:$ROOT/out/serial-$VNC_DISPLAY.log" -monitor "unix:$ROOT/out/monitor-$VNC_DISPLAY.sock,server,nowait" \
  -device virtio-serial -chardev "socket,id=qga,path=$ROOT/out/qga-$VNC_DISPLAY.sock,server=on,wait=off" \
  -device virtserialport,chardev=qga,name=org.qemu.guest_agent.0
