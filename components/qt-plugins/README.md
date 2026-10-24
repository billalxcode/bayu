# Qt Plugins

Qt plugins that make Qt applications at home in the Bayu session. The
`bayu` platform theme makes them follow the system's dark mode, fonts,
icon and cursor themes, open the Bayu file chooser, and scroll with the
same touchpad feel as Bayu's own apps. The `bayu` widget style draws Qt
Widgets with the Bayu design system, the same tokens as the GTK theme in
`components/gtk-theme`: primary, secondary, ghost and destructive buttons,
36 px fields and selects, menus with an accent check mark, cards, tab bars
and segmented controls, with background shifts for hover and press and an
accent ring for keyboard focus. Applications can opt into a few variants
with dynamic properties: `primary` and `danger` on a `QPushButton`, `switch`
on a `QCheckBox`, `badge`, `keycap` and `banner` on a `QLabel`, and the
object names `Card` (a `QFrame`) and `Row` (a hoverable `QWidget`). The
image format plugins add SVG, HEIF/HEIC/AVIF and JPEG XL.

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
