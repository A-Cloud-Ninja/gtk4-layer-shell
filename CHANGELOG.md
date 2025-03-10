# Changelog

## [Unreleased]

## [1.0.0] - 11 Apr 2023
- Port library and examples from GTK3 to GTK4
- Remove deprecated functions `gtk_layer_set_keyboard_interactivity()` and `gtk_layer_get_keyboard_interactivity()` (`gtk_layer_set_keyboard_mode()` and `gtk_layer_get_keyboard_mode()` can be used instead)
- Change how layer surface window size is controlled, use `gtk_window_set_default_size()` now
- Build [documentation](https://wmww.github.io/gtk4-layer-shell/) with GitHub actions and host with GitHub Pages
- __EDIT:__ Change license from LGPL to MIT (most of the gtk-layer-shell code was always MIT, and the LGPL bits have all been dropped)
