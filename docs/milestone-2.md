# Milestone two: library-first JSON compiler

Current task creates memory-to-memory `glyphweld_ui_compiler` scaffold only. It validates strict JSON UTF-8 and duplicate keys, recognizes minimal schema, returns result-owned diagnostics/output, and emits deterministic internal scaffold C.

`GwUiCompilerRequest` may omit limits and allocator: documented defaults and standard host allocator apply. Partial allocators fail. Request objects and source bytes may be released when compilation returns because output and diagnostics are result-owned. Result objects must begin zero-initialized; recompiling into a valid result disposes prior compiler-owned output first. Output bytes are UTF-8, NUL-terminated, and report size excluding NUL. Every failure has null/zero output and one primary machine-readable diagnostic. `gw_ui_compiler_result_dispose()` uses copied allocator ownership, remains valid after request lifetime, clears result, and is idempotent. A custom allocator context must remain valid until disposal completes. `GwUiCompilerResult` is an owning, non-copyable object: a result containing compiler-owned output must remain at a stable address until disposal, and copying or independently disposing a copy is invalid.

Limits default to document 1 MiB, depth 32, UI nodes 128, JSON values 4096, object members 32, array elements 128, string bytes 1024, and live parser allocation 2 MiB. Hard maxima are 8 MiB, 64, 1024, 16384, 128, 1024, 4096, and 16 MiB. Zero fields select defaults; larger values reject. `max_ui_nodes` is reserved for future validated UI nodes only; it does not limit parser AST values. Parser depth is bounded by configured limit, containers grow incrementally, and parser allocations are live-budgeted.

Implemented phase: canonical application/action/node validation, deterministic preorder, domain-separated FNV-1a IDs, generated immutable tree C, semantic color tokens, built-in global themes, runtime tree loading, build-directory generation, and generated counter loading. The developer-only driver is a build harness, never a user workflow.

The implemented JSON node subset also accepts bounded nonnegative `minimum_width`, `minimum_height`, `grow`, `shrink`, and uniform `padding` values. Minimum dimensions cannot exceed preferred dimensions. Padding is valid only for row, column, and stack containers; it is emitted into the existing `GwNodeDesc` and resolved by the portable integer layout engine. Labels and buttons may set `text_overflow` to `clip` (default), `ellipsis`, or `wrap`; containers reject it. The embedded monospaced TTF pixel scale never changes to make a line fit. Wrapping uses whitespace when available and splits an oversized word deterministically; both wrapping and ellipsis clip to the half-open text bounds.

Applications may opt into runtime integer presentation scaling. Root preferred dimensions select the whole-number scale up to 8×; layout then fills the available logical viewport at that scale rather than locking to the base dimensions. Commands begin at the top-left and clip only a final partial pixel column or row, while pointer input maps through the same scale. The X11 counter and 320×210 logical gallery opt in as compact-canvas examples; the builder stays responsive. This is runtime configuration rather than another generated node property.

The optional Linux X11/XCB and Wayland target hosts use the same generated
counter tree and portable runtime. They are enabled with
`GLYPHWELD_BUILD_X11_HOST=ON` or `GLYPHWELD_BUILD_WAYLAND_HOST=ON`; the
developer convenience switch `GLYPHWELD_BUILD_LINUX_DESKTOP_HOSTS=ON`
auto-detects and builds every available host without weakening headless.
Each host owns its platform headers and converts the runtime's byte-defined
surface locally. X11's basic translation is intentionally limited to ordinary
US-layout ASCII; Wayland uses xkbcommon for the compositor-provided keymap but
still has only basic text input, not IME support.

Stable IDs are 32-bit FNV-1a (offset `2166136261`, prime `16777619`) over ASCII `application:<app>`, `node:<app>:<node>`, and `action:<app>:<action>` bytes. Zero and detected collisions reject. Generated symbols are independently normalized and collision-checked.

Runtime tree strings and dynamic text are borrowed zero-copy strings. Generated source supplies static storage; application callers retain dynamic bytes until replacement or runtime lifetime ends.

Runtime generated-tree validation accepts at most 64 nesting levels. This
matches the compiler's maximum document depth and bounds the current preorder
validator's call stack; deeper descriptors are rejected atomically.

The developer-only generation driver uses exclusive same-directory temporary files and rollback backups. It provides transactional replacement for handled filesystem failures, not crash-level atomicity across two files. If rollback cannot restore an original file, it preserves and reports the recovery backup path.

The compiler grows generated UTF-8 C buffers through the request allocator up to a documented 32 MiB hard output limit. Allocation/limit failures expose no partial result buffers.

| Requirement | File | Test/status |
| --- | --- | --- |
| Versioned compiler API and ownership | `src/compiler/include/glyphweld/ui_compiler.h` | `src/compiler/tests/test_ui_compiler.c` |
| Defaults, hard limits, allocator resolution and result reuse | `src/compiler/src/uic_diagnostic.c`, `src/compiler/src/uic_model.c` | direct compiler tests |
| UTF-8/escape parser and duplicate keys | `src/compiler/src/uic_json.c` | direct compiler tests |
| Validated model, actions, preorder, and stable IDs | `src/compiler/src/uic_model.c` | direct compiler tests |
| LF deterministic generated tree data | `src/compiler/src/uic_emit.c` | repeated-output test and generated counter build |
| Runtime tree loading, semantic palette, and global theme | `src/runtime/src/ui/tree.c` | `tests/test_runtime.c`, generated counter/headless test |
| Text overflow metadata and rendering | `src/runtime/src/render/commands.c`, `src/runtime/src/render/software.c` | `tests/test_render.c`, direct compiler test |
| Optional integer presentation scaling | `src/runtime/src/layout/flex.c`, `src/runtime/src/render/commands.c`, `src/runtime/src/ui/events.c` | `tests/test_runtime.c` |
| Driver transaction rollback and reported recovery backup | `src/tools/uic_driver.c` | `glyphweld_uic_driver_transaction` |
| Optional X11/XCB generated-counter host | `targets/linux-x11/x11_host.c`, `examples/counter/main_x11.c` | `glyphweld_x11_host_tests`; manual display smoke when available |
| Optional Wayland generated-counter host | `targets/linux-wayland/wayland_host.c`, `examples/counter/main_wayland.c` | `glyphweld_wayland_host_tests`; manual compositor smoke when available |
| Library-first decision | `docs/decisions/0006-library-first-ui-compiler.md` | documented |
