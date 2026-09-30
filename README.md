# Glyphweld

Glyphweld is an early visual builder for small native applications.

The planned workflow is a visual editor that compiles a project into C data,
combines it with the portable C99 runtime and a selected target, then builds a
self-contained native app. Exported apps will not include the editor or
compiler.

This repository contains the runtime, internal JSON-to-C compiler, headless
test target, and Linux X11 and Wayland examples that the editor will build on.

Current version: 0.1.0. Breaking changes are expected before a stable release.

## Build

You need CMake 3.19 or newer, a C99 compiler, and Ninja.

```sh
cmake -S . -B build/linux/x86_64/debug/headless -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux/x86_64/debug/headless
ctest --test-dir build/linux/x86_64/debug/headless --output-on-failure
```

The headless build is the default and does not need desktop libraries.

## Desktop examples

X11 support needs XCB development headers. Wayland support needs
`wayland-client`, `xkbcommon`, `wayland-scanner`, and `wayland-protocols`.

```sh
cmake -S . -B build/linux/x86_64/debug/desktop -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DGLYPHWELD_BUILD_LINUX_DESKTOP_HOSTS=ON
cmake --build build/linux/x86_64/debug/desktop
```

The desktop build provides the counter, builder workbench, and widget gallery
for each available host.

## Current state

Glyphweld supports row, column, and stack layouts, labels, buttons, software
rendering, keyboard focus, and pointer input. The examples load generated UI
trees from JSON project data.

The builder workbench can display example projects. It cannot edit or save
project files, build applications, or export applications yet. Text input,
clipboard support, IME support, GPU rendering, and non-Linux targets are not
implemented.

Future targets include desktop systems, direct Linux display backends, ARM
Linux devices, Android, and embedded systems. This version only includes the
headless, X11, and Wayland targets in this repository.

## Layout

- `src/runtime/` contains the portable runtime and public headers.
- `src/compiler/` contains the internal JSON-to-C compiler.
- `targets/` contains the headless, X11, and Wayland hosts.
- `examples/` contains the counter, workbench, and widget gallery.
- `tests/` contains the automated checks.

More design detail is in `docs/architecture.md`.

## Third-party code

DejaVu Sans Mono is embedded for text rendering. Its license is in
`assets/fonts/LICENSE-DejaVu.txt`. The renderer uses `stb_truetype` from
`third_party/stb/` under its included license.

## License

Glyphweld is released under the MIT License. See `LICENSE`.
