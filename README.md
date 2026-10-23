# Bayu

Bayu is a Wayland desktop environment and an AlmaLinux-based OS built
around it. This repository is a monorepo holding every desktop component,
the RPM packaging and the OS image builder.

## Credits

Bayu is a fork of **[Cutefish](https://github.com/cutefishos)** by Reion
Wong and the Cutefish contributors, started from the components listed in
Cutefish's [getting-started](https://github.com/cutefishos/getting-started)
guide. The desktop code is theirs at heart; Bayu took it as its own source,
renamed it, ports it to AlmaLinux and develops it independently from here on.
Cutefish is BSD-3-Clause and so is Bayu; the original copyright notices are
kept in every file and in [LICENSE](LICENSE). [UPSTREAM.md](UPSTREAM.md)
records the Cutefish revision each component was forked from.

## Layout

| Path | What |
|---|---|
| `components/` | The desktop source, one directory per component (forked from the Cutefish repositories) |
| `build.sh` | Builds all components in dependency order |
| `packaging/` | AlmaLinux 10 build container, `bayu-desktop` RPM spec, EL PAM stacks |
| `builder/` | OS image builder: `bayu.conf`, `repos.d/*.repo`, `build-image.sh`, `run-vm.sh` |

Components: bayu-toolkit, bayu-framework, bayu-core, bayu-compositor,
bayu-dm, filemanager, icons, wallpapers, settings, terminal,
xdg-desktop-portal-bayu, qt-plugins, text-editor, calendar, system-monitor,
video-player and bayu-computer-use. video-player (needs `mpv-devel`, whose
SDL2 dependency EL10 lacks) and bayu-computer-use (Rust) are kept in the tree
but not built yet.

## Building

Everything runs in containers on any Linux host with Podman; the build
environment is AlmaLinux 10 with EPEL and CRB.

```shell
packaging/build-rpm.sh          # .src.rpm + .rpm -> out/rpms, and a dnf repo in out/repo
builder/build-image.sh          # AlmaLinux 10 + Bayu -> out/bayu-os.qcow2
builder/run-vm.sh               # boot it in QEMU/KVM, VNC on the Tailscale IP
```

The first build fetches Skia, which bayu-toolkit builds, and takes a while.
`packaging/bayu-desktop.spec` builds from source: its sources are the
repository (`git archive`) and the pinned Skia checkout, which
`packaging/rpmbuild.sh` prepares, so the `.src.rpm` rebuilds anywhere with
`rpmbuild --rebuild` or mock.

### Releases

GitHub Actions (`.github/workflows/rpm.yml`) builds the `.rpm` and `.src.rpm`
on AlmaLinux 10 for every push and pull request. Pushing a `vX.Y.Z` tag
builds version X.Y.Z-1 and attaches the packages to a GitHub release:

```shell
git tag -a v0.2.0 -m "Bayu 0.2.0" && git push origin v0.2.0
```

### OS builder

`builder/build-image.sh` installs `PACKAGES` from `builder/bayu.conf` into
a fresh disk image with `dnf --installroot`, using only the `*.repo` files
in `builder/repos.d/`. To build against a different mirror, an internal
repository, or a published Bayu repository, edit or replace those files, or
point at another directory:

```shell
REPOS=/path/to/my-repos.d CONFIG=/path/to/my.conf builder/build-image.sh
```

The same repo files are installed into the image as its `/etc/yum.repos.d`,
so the installed system updates from the same place. `bayu.conf` also
sets the image size, hostname, locale, timezone, the first user and
whether it logs in automatically.

### Running

`builder/run-vm.sh` needs KVM. Bayu needs EGL/GLES2: with `GL=yes` (the
default) the guest gets virgl through the host GPU; `GL=no` gives a plain
virtio-gpu and the guest renders with Mesa's software driver. VNC listens
on the host's Tailscale address only.

## License

BSD-3-Clause; see [LICENSE](LICENSE).
