# 🔍 `wooz` - A zoom / magnifier utility for wayland compositors.

https://github.com/user-attachments/assets/23e9aeac-4f60-47e6-9c49-7a3706bcdbe9

Scroll with your mouse to zoom and use left mouse click and drag to move the
viewport.

Use right click to exit.


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
