# Architecture

## System boundary

Glyphweld has a portable ISO C99 runtime and replaceable target packs. Runtime owns retained UI state, deterministic layout, normalized-event dispatch, bounded dirty state, and bounded drawing-command construction. A target host owns platform event pumping, presents frame output, and sends presentation availability/completion back to the runtime. The runtime must tolerate presenter back-pressure; it never waits for a display operation.

Runtime contains no platform headers, platform handles, target preprocessor branches, filesystem access, networking, process control, decoding, compilation, or blocking work. Applications and target hosts may depend on public runtime headers. Runtime may never include or link a target implementation. Target headers and handles never cross the public ABI.

The optional Linux X11/XCB and Wayland target hosts are thin software
presenters. XCB headers and calls are confined to `targets/linux-x11/`; Wayland,
`xdg-shell`, and xkbcommon calls are confined to `targets/linux-wayland/`. Both
consume the runtime's byte-defined `GW_PIXEL_ARGB8_BYTES` surface and convert
bytes before presentation; neither treats the runtime surface as native-endian
`uint32_t` pixels. X11 uploads to its selected image format with `xcb_put_image`;
Wayland uses `wl_shm` XRGB8888 buffers. Their bounded event pumps normalize
pointer, basic keyboard/text, resize, redraw, and close events into portable
events. The hosts remain separate target packs that share only public runtime
data and application behavior.

## ABI, memory, and rendering

The public ABI is versioned C99 POD using fixed-width types, explicit ownership, and result codes. `GwEvent`, `GwNodeDesc`, `GwDrawCommand`, `GwSurface`, `GwRuntimeStorage`, `GwPalette`, and `GwTheme` are extensible ABI objects; this milestone requires `struct_size == sizeof(current structure)`. `GwStr`, `GwRect`, and `GwColor` are fixed value types, not extensible objects. Public ABI lengths, capacities, and counts use `uint32_t` or `uint64_t`; `size_t` does not cross ABI.

`GwRuntime` and `GwNode` are caller-owned implementation layouts for this temporary vertical slice. They are explicitly internal and unstable. Applications use `GwId` and API calls, never their fields. A later milestone replaces them with opaque caller-owned storage without changing stable public IDs.

The embedded profile uses fixed capacity and no allocation after initialization. Work is bounded by caller-provided pool, event, damage, and command capacities. Allocation convenience for a future desktop builder cannot become implicit runtime ownership.

`GW_PIXEL_ARGB8_BYTES` is a straight-alpha Glyphweld byte sequence: byte 0 alpha, byte 1 red, byte 2 green, byte 3 blue. It has four bytes per pixel and a byte stride. It is not a packed native ARGB integer, and runtime code must not interpret storage as native-endian `uint32_t`. Presenters convert it when their native format differs. Commands and writes use half-open integer rectangles and validate supplied surface dimensions, stride, capacity, and command data before rendering. The software renderer supports clear, filled rectangles, and bounded UTF-8 glyph runs through an embedded DejaVu Sans Mono TrueType face. It rasterizes with fixed per-frame scratch storage and blends grayscale glyph coverage into the byte-defined surface; the compact 5×7 printable-ASCII face remains the no-allocation fallback for an unavailable or oversized TTF glyph. Glyph bytes are borrowed through the command and are clipped to both their node rectangle and the supplied surface.

## Layout and execution contract

Layout is retained, deterministic, integer-only, and uses half-open rectangles `[x, x + width)` and `[y, y + height)`. `GwNodeDesc` provides nonnegative preferred and minimum outer dimensions, nonzero grow and shrink weights, and uniform nonnegative container padding. Rows and columns grow or shrink along their main axis using integer weighted shares; unassigned remainder pixels are applied in stable child order. Shrink never crosses a child minimum. A layout whose minimum sizes, gaps, or padding cannot fit returns `GW_ERROR_INVALID_SIZE`; arithmetic overflow returns `GW_ERROR_OVERFLOW`. Stack children receive the padded content rectangle. Runtime uses no floating point for layout.

`GW_SCALE_MODE_INTEGER_FIT` is an optional presentation mode. The root preferred dimensions select whole-number scale thresholds up to `GW_RUNTIME_MAX_INTEGER_SCALE` (8). At the selected scale, layout receives the available physical viewport divided by that scale and rounded up, so responsive containers keep expanding until the next scale step. Transformed commands start at the top-left; a final partial pixel column or row clips at the surface rather than producing letterboxing. Pointer input maps back through the same scale. Applications that leave the mode disabled retain ordinary responsive layout.

Generated-tree preorder validation permits at most `GW_RUNTIME_MAX_TREE_DEPTH`
(64) nesting levels, matching the compiler's document-depth hard maximum. A
deeper descriptor is rejected before caller-owned runtime state is mutated.

The public host interface remains POD-oriented: hosts provide normalized keyboard, mouse, touch, controller, focus, resize, timer, and presentation events; runtime produces bounded output. The initial normalized keys are Tab, Enter, and Space: Tab cycles button focus, while Enter and Space activate the focused button. Focused buttons receive a one-pixel high-contrast `text`-token ring. `gw_runtime_frame_dirty()` and `gw_runtime_clear_frame_dirty()` permit hosts to present only requested frames without reaching into unstable runtime layouts. Builder file operations, project parsing, compilation, assets, process execution, and network work must run outside the UI dispatch path and report completion through queued events.

## Declarative projects and target packs

Canonical projects are strict declarative JSON, independent of C. `glyphweld_ui_compiler` is an internal memory-to-memory C99 library linked into future single Glyphweld editor executable. It is not a user-facing compiler product. Editor-owned private toolchains remain later work. Generated C is disposable build data; exported applications exclude compiler/editor/parser code.

Compiler output is deterministic UTF-8 C source with LF lines, no timestamps, paths, locale, pointers, random data, or machine metadata. Compiler result owns output and diagnostics independently of request lifetime. Stable IDs use FNV-1a 32-bit: offset `2166136261`, prime `16777619`, canonical ASCII `application:<app>`, `node:<app>:<id>`, and `action:<app>:<id>` byte domains; zero and collisions reject. Canonical JSON becomes a private validated model, then disposable generated C descriptors; hand-authored application behavior remains separate until future Nim integration.

Generated trees load into fixed caller-owned runtime storage through a validated descriptor. Semantic `GwColorToken` values resolve through the installed global `GwTheme` at command construction; foreground tokens resolve into the embedded TTF renderer. Each runtime begins with the built-in dark DSi-inspired theme: a deep navy canvas, raised slate-blue surfaces, cobalt controls, cyan focus, and bounded one-pixel bevel details. `gw_runtime_set_theme()` validates and copies a complete theme atomically, marks the frame dirty, and lets one application-wide replacement restyle existing generated trees. `GwPalette` remains the narrower semantic-color replacement API. Per-node themes, arbitrary project colors, gradients, inheritance, and a full styling language remain deferred. Text shaping, kerning, fallback-face selection, and font caching remain deferred. Builder authoring remains scene-tree/container-inspired without copying third-party implementation or terminology.

Tree strings and `gw_runtime_set_text()` strings are borrowed zero-copy values. Callers retain bytes until replacement or runtime lifetime ends; generated C strings have static lifetime.

Target packs describe identity, CPU properties, compiler/linker/sysroot/startup metadata, executable/package behavior, host/renderer/input implementations, filesystem/network/thread capabilities, asset conversion, deployment, licensing, limits, and supported framework capabilities. Commands are argument arrays rather than shell strings. Normal configuration does not fetch the network or mutate Git. Managed toolchain selection is a future outer-bootstrap/generated-toolchain-file responsibility before CMake enables C.

Generated applications include only application behavior, generated data, portable runtime, and the selected target. They exclude builder code, parsers, compilers, and unused targets.

The current X11 builder workbench is itself a generated Glyphweld application:
it links generated data, application behavior, the portable runtime, and the
selected host. It is an early visual smoke surface, not yet a project editor.
The separate widget gallery follows the same path and exercises every current
widget and container in a real resizable window. Future editing, project I/O,
and export work must remain outside runtime dispatch and use the editor's
non-blocking task boundary.

## Roadmap and scope status

Implemented now: headless retained row/column/stack, preferred/minimum/weighted responsive layout with uniform container padding, labels/buttons with embedded monospaced TTF text plus bitmap fallback, deterministic software rendering, a built-in global DSi-inspired theme, strict JSON/model/generated-C counter integration, and optional Linux X11/XCB and Wayland software hosts. The default theme gives `surface` panels a four-sided one-pixel bright/dark bevel and gives `accent` buttons matching raised outlines; surface-backed labels use the same horizontal inset as panel content. This is runtime-wide rather than application-specific. Text uses a fixed scale rather than resizing to fit; `GwTextOverflow` explicitly selects clipping, ellipsis, or deterministic word wrapping within half-open text bounds.

Documented but deferred: visual builder development; managed toolchain installation/cache/export; Nim-to-C behavior; broader layout/widgets; and public target packs for Windows, macOS, Linux DRM/KMS, Linux framebuffer, Linux ARM/Raspberry Pi, Android, embedded Linux, and generic RTOS/bare-metal targets.

Exploratory: capability-specific renderer enhancements, community-maintained target packs, vendor-authorized platform integrations, and custom embedded targets. These are not implementation commitments until a target pack and acceptance criteria are approved.

## Risks

Avoid a monolithic per-node state structure, platform leakage into the ABI, ambiguous percentage/fill rules, renderer-specific widget behavior, implicit allocation, unbounded work, and a builder UI that performs I/O or compilation synchronously. Managed toolchain distribution requires provenance, pinning, licensing, and offline behavior before it can replace the development-only system fallback.

## Dependency policy

Portable runtime stays dependency-minimal and third-party-type-free at public ABI boundary. Builder/host work may use mature external libraries when they improve maintainability, licensing fit, determinism, size, portability, or correctness over custom infrastructure. Glyphweld owns wrapper interfaces and links only required features, preferably statically where practical. Dependencies remain out of exported applications unless selected target/application capability requires them.
