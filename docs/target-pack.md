# Target packs

A target pack declares identity, compatible C toolchains, capabilities, limits, renderer/host implementation, assets, packaging, deployment, licensing, and external SDK requirements. Runtime never includes a target implementation. Target hosts satisfy runtime-defined POD boundaries and own all platform headers.

The default development target is `headless`. Its manifest is operational during CMake configuration and validates CMake 3.19+, Ninja 1.8+, and compiler family/version already selected by CMake. Resolved system tools are development-only and non-reproducible fallbacks. Future managed selection must happen before `project(... LANGUAGES C)`, through an outer bootstrap step or generated toolchain file.

`targets/linux-x11/` is an optional Linux X11/XCB host implementation enabled
with `-DGLYPHWELD_BUILD_X11_HOST=ON`. CMake discovers XCB through `pkg-config`;
when it is unavailable, the default headless configuration remains unaffected
and an explicitly requested X11 configuration fails with an actionable message.
The target owns XCB headers, event pumping, visual/image-format validation, and
conversion from Glyphweld `[A,R,G,B]` bytes to the X server's native image
bytes. It is a software-upload host, not a portable runtime dependency.

`targets/linux-wayland/` is an optional Linux Wayland host implementation
enabled with `-DGLYPHWELD_BUILD_WAYLAND_HOST=ON`. It discovers
`wayland-client`, `xkbcommon`, `wayland-scanner`, and the standard `xdg-shell`
protocol through development tools. It owns all Wayland/xkbcommon headers,
normalizes basic input, and uploads host-converted `wl_shm` XRGB8888 buffers.
When the compositor advertises XDG decoration support, it requests server-side
window controls; a compositor may still choose client-side or no decorations.
`-DGLYPHWELD_BUILD_LINUX_DESKTOP_HOSTS=ON` auto-detects both optional desktop
hosts; unavailable hosts are reported and skipped, while an explicitly
requested host remains an actionable configuration error.

Future public directions include Windows, macOS, Linux DRM/KMS/framebuffer,
Linux ARM/Raspberry Pi, Android, embedded Linux, generic RTOS/bare-metal, and
community-maintained or vendor-authorized target packs.
