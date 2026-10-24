# GTK Theme

The `Bayu` theme for GTK 3 and GTK 4 applications, following the Bayu
design system: its colour tokens (`Bayu/colors-light.css`,
`Bayu/colors-dark.css`), radii (6 tile, 8 control, 10 menu, 12 card), 36 px
buttons and fields, 44 × 26 switches, settings rows grouped in cards, a
segmented `GtkStackSwitcher`, underlined notebook tabs, and hover, press and
focus states that shift the background and ring the control in the accent.
It sits over GTK's own theme, in light (`Bayu`) and dark (`Bayu-dark`);
bayu-services selects one or the other with the desktop's dark mode, through
GSettings.

The Qt widget style in `components/qt-plugins` draws the same tokens, so GTK
and Qt applications look alike.

libadwaita applications use their own stylesheet and keep the GNOME look,
apart from following dark mode and Bayu's 48 px header bars.

## Build

```shell
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
sudo cmake --install build
```

## License

BSD-3-Clause; see [LICENSE](../../LICENSE).
