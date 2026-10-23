# Core

The Bayu session, system services and their helpers.

## Dependencies

bayu-toolkit, the display client of bayu-framework, libsystemd, ICU,
Fontconfig, Wayland development libraries and wayland-protocols (>= 1.39).
The screen recorder also needs FFmpeg, libva, libdrm, GBM and PipeWire; the
tests need GoogleTest.

## Build

```shell
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
make
```

## Install

```shell
sudo make install
```

Install `bayu-compositor` and `bayu-filemanager` as well, then select
`Bayu (Wayland)` in the display manager.

## License

BSD-3-Clause; see [LICENSE](LICENSE).
