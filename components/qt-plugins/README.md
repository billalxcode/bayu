# Qt Plugins

Qt plugins that make Qt applications at home in the Bayu session. The
`bayu` platform theme makes them follow the system's dark mode, fonts,
icon and cursor themes, open the Bayu file chooser, and scroll with the
same touchpad feel as Bayu's own apps. The `bayu` widget style draws
buttons, fields, check boxes, sliders, scroll bars, progress bars and menus
with the metrics and colours of Bayu's own controls. The image format
plugins add SVG, HEIF/HEIC/AVIF and JPEG XL.

## Dependencies

`sudo pacman -S gcc cmake qt6-base qt6-declarative qt6-tools wayland wayland-protocols librsvg libheif libjxl`

## Build

```shell
mkdir build
cd build
cmake ..
make
sudo make install
```

## License

BSD-3-Clause; see [LICENSE](LICENSE).
