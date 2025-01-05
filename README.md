# 🔍 `wooz` A zoom / magnifier utility for wayland compositors.

## Building from source

Install dependencies:

* meson (build)
* ninja (build)
* wayland

Then run:

```sh
export CFLAGS="-O3"
meson build
ninja -C build
```

To run directly, use `build/grim`, or if you would like to do a system
installation (in `/usr/local` by default), run `ninja -C build install`.

## Contributing

If you want to contribute to `allelua` to add a feature or improve the code contact
me at [alexandre@negrel.dev](mailto:alexandre@negrel.dev), open an
[issue](https://github.com/negrel/allelua/issues) or make a
[pull request](https://github.com/negrel/allelua/pulls).

## :stars: Show your support

Please give a :star: if this project helped you!

[![buy me a coffee](https://github.com/negrel/.github/raw/master/.github/images/bmc-button.png?raw=true)](https://www.buymeacoffee.com/negrel)

## :scroll: License

MIT © [Alexandre Negrel](https://www.negrel.dev/)
