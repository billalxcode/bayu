# Installer end-to-end tests

`e2e.py` installs the live ISO in a VM the way a person would, through the
installer's window, then boots what it installed and checks it.

```sh
PROFILE=igos builder/build-iso.sh          # out/igos-bayu-live.iso
tests/installer/e2e.py --list
tests/installer/e2e.py win-free two-disks  # or no names: every scenario
E2E_VNC=0.0.0.0:1 tests/installer/e2e.py --explore win-mbr   # boot it and look
```

Each scenario:

1. `mkdisk.sh` makes its disks. Windows is a stand-in, not an installation:
   its partitions, file systems, boot files and folders, which is what the
   installer, os-prober and GRUB look at. The partitions to keep are noted.
2. The ISO boots in QEMU (KVM, 1280x800). The installer is driven by pointer
   and keys over QMP, each step waiting for the screen to allow it. Every
   step leaves a screenshot.
3. When Anaconda and the post-install end, their logs are kept and the kept
   partitions are compared: table entry, file system UUID, the first 64 MiB.
4. The disks boot alone (the GRUB menu is captured), the new account logs in
   over SSH and the system is checked: running, Windows in the GRUB menu
   where there is one, the kernel with binder.

| Scenario | Disks | Choice in the installer |
|---|---|---|
| `blank`, `blank-bios` | empty, UEFI / BIOS | defaults |
| `luks` | empty | *Encrypt my data* |
| `win-free`, `win-free-sata` | Windows, 20 GiB free, exFAT data (issue #26) | *Use the free space* |
| `win-esp200` | as `win-free`, 200 MiB EFI partition | *Use the free space* |
| `win-full` | Windows filling the disk | *Install alongside Windows* |
| `win-mbr` | Windows on MBR, BIOS | *Use the free space* |
| `two-disks` | Windows, and an empty disk | defaults: the empty disk must be picked |
| `too-small` | 4 GB | must refuse |

Results are in `out/e2e/<scenario>/`: `result.txt`, the screenshots, the
installation logs and `system.txt`.

Needs `qemu-system-x86_64` with KVM, OVMF, `qemu-nbd`, `sgdisk`, `sfdisk`,
`mkntfs`, `mkfs.exfat`, `sshpass`, and sudo for the disks. `E2E_VNC` (default
`127.0.0.1:1`) is where the VM's display is served; `E2E_SSH_PORT` (2224) the
port forwarded to its SSH.
