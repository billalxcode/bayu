# GTK Theme

The `Bayu` theme for GTK 3 and GTK 4 applications: Bayu's colours, corner
radii and control sizes (buttons, text fields, check boxes, switches,
sliders, progress bars, scroll bars, tabs, menus) over GTK's own theme, in
light (`Bayu`) and dark (`Bayu-dark`). bayu-services selects one or the
other with the desktop's dark mode, through GSettings.

libadwaita applications use their own stylesheet and keep the GNOME look,
apart from following dark mode.

## Build

```shell
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
sudo cmake --install build
```

## License

BSD-3-Clause; see [LICENSE](../../LICENSE).
