# 🔍 `wooz` - A zoom / magnifier utility for wayland compositors.

https://github.com/user-attachments/assets/23e9aeac-4f60-47e6-9c49-7a3706bcdbe9

A powerful and customizable screen magnifier for Wayland compositors with mouse and keyboard controls.

## Usage

```sh
wooz [options...]
```

### Options

* `-h, --help` - Show help message and quit
* `--map-close KEY` - Set key to close (e.g., 'Esc', 'q', 'x')
* `--mouse-track` - Enable mouse tracking (follow mouse without clicking)
* `--zoom-in PERCENT` - Set initial zoom percentage (e.g., '10%', '50%')
* `--invert-scroll` - Invert scroll direction (scroll up zooms in)

### Controls

**Mouse:**
* Scroll wheel - Zoom in/out at mouse position
* Left click + drag - Pan the view
* Right click - Exit
* Double click - Restore/unzoom to original view

**Keyboard:**
* `+` / `-` - Zoom in/out at screen center
* Arrow keys - Pan the view
* `0` - Restore/unzoom to original view
* `Esc` - Exit (default, customizable with `--map-close`)

### Examples

```sh
# Start with 10% zoom at center
wooz --zoom-in 10%

# Enable mouse tracking (viewport follows mouse)
wooz --mouse-track

# Use 'q' key to exit instead of Esc
wooz --map-close q

# Combine options
wooz --zoom-in 25% --mouse-track --map-close x

# Invert scroll direction (scroll up to zoom in)
wooz --invert-scroll
```


## Building from source

Install dependencies:

* meson (build)
* ninja (build)
* wayland (viewporter, XDG shell, wlr screencopy and core protocols)

Then run:

```sh
export CFLAGS="-O3"
meson build
ninja -C build
```

To run directly, use `build/wooz`, or if you would like to do a system
installation (in `/usr/local` by default), run `ninja -C build install`.

## Contributing

If you want to contribute to `wooz` to add a feature or improve the code contact
me at [alexandre@negrel.dev](mailto:alexandre@negrel.dev), open an
[issue](https://github.com/negrel/wooz/issues) or make a
[pull request](https://github.com/negrel/wooz/pulls).

## :stars: Show your support

Please give a :star: if this project helped you!

[![buy me a coffee](https://github.com/negrel/.github/raw/master/.github/images/bmc-button.png?raw=true)](https://www.buymeacoffee.com/negrel)

## :scroll: License

MIT © [Alexandre Negrel](https://www.negrel.dev/)
