# Milestone one: headless vertical foundation — complete

Implemented requirements are fixed-capacity retained row/column/stack/label/button UI, half-open checked integer layout, stable IDs, bounded commands, `GW_PIXEL_ARGB8_BYTES` rendering, headless counter behavior, CMake manifest validation, and portable-source enforcement.

| Requirement | File | Test/status |
| --- | --- | --- |
| CMake 3.19/Ninja headless skeleton | `CMakeLists.txt` | clean configure/build |
| Operational selected-tool validation | `targets/headless/target.json`, `cmake/ResolveTargetToolchain.cmake` | clean configure prints compiler/tool identity/version |
| Portable source isolation | `cmake/CheckPortableRuntime.cmake` | `check-portable-runtime` |
| Versioned POD ABI and stable public IDs | `src/runtime/include/glyphweld/gw_abi.h`, `gw_ui.h` | `tests/test_counter.c` |
| Caller-owned byte-defined surface | `src/runtime/include/glyphweld/gw_surface.h` | `tests/test_render.c` |
| Bounded command construction and whole-buffer prevalidation | `src/runtime/src/render/commands.c`, `src/runtime/src/render/software.c` | `tests/test_render.c` |
| Embedded DejaVu Sans Mono TTF glyph runs, bitmap fallback, and node-bound clipping | `assets/fonts/DejaVuSansMono.ttf`, `src/runtime/src/render/software.c` | `tests/test_render.c`, `tests/test_counter.c` |
| Row/column/stack layout, minimums, uniform padding, and half-open bounds | `src/runtime/src/layout/flex.c` | `tests/test_layout.c` |
| Stable grow/shrink remainders, invalid minimum size, and overflow rejection | `src/runtime/src/layout/flex.c` | `tests/test_layout.c` |
| Pointer and basic keyboard button dispatch | `src/runtime/src/ui/events.c` | `tests/test_counter.c`, `tests/test_runtime.c` |
| Retained nodes and label update | `src/runtime/src/ui/tree.c`, `examples/counter/app.c` | `tests/test_counter.c` |
| Surface stride/capacity, direct-byte output, clipping/no external writes | `src/runtime/src/render/software.c` | `tests/test_render.c` |
| Deterministic end-to-end headless counter | `targets/headless/include/glyphweld_headless.h`, `targets/headless/headless_host.c`, `examples/counter/` | `tests/test_counter.c` |
| JSON compiler, generated C, Nim, builder, public target packs | architecture/docs only | intentionally deferred |

Deferred requirements remain binding architecture constraints; no source placeholders are created for them.
