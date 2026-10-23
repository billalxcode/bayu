# Bayu Framework

Shared system APIs for Bayu components and applications, on the
[bayu-toolkit](https://github.com/igos-nusantara/bayu) base
library:

- `clients/`: clients of system services (accounts, appearance, audio,
  BlueZ, display, NetworkManager, power profiles, time and date, UPower), one
  library each, in the `BayuClients` package.
- `files/`: file listing, MIME types, operations, trash, volumes and
  thumbnails, in the `BayuFiles` package.

## Dependencies

### Debian/Ubuntu

```shell
sudo apt install cmake pkg-config build-essential \
    libicu-dev libpng-dev \
    libpipewire-0.3-dev libspa-0.2-dev libgtest-dev
```

### Arch Linux

```shell
sudo pacman -S --needed base-devel cmake pkgconf \
    icu libpng pipewire gtest
```

and [bayu-toolkit](https://github.com/igos-nusantara/bayu).

## Build and Install

```shell
mkdir build
cd build
cmake ..
make
sudo make install
```

## License

BSD-3-Clause; see [LICENSE](LICENSE).
