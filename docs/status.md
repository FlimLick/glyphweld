# Status

## Completed work

The repository baseline, strict generated-counter pipeline, and headless implementation are complete. Implementation is consolidated beneath `src/`, with target packs, examples, tests, build support, and documentation kept at the repository root. The current build uses a development-only system compiler/tool fallback validated by the headless manifest. Optional Linux X11/XCB and Wayland hosts now open the generated counter, builder workbench, and widget gallery in resizable windows using software pixel upload.

## Active milestone

Milestone two: generated counter-tree integration and early visual workbench. Compiler remains internal and library-first.

## Next approved task

Add text input and a non-blocking editor task boundary before implementing real project editing or Build/Export; broader visual-builder work remains later.

## Known failures and technical debt

No known test failures. Runtime/node layouts remain unstable until opaque caller-owned storage replaces them. Layout supports preferred/minimum dimensions, weighted grow/shrink, gaps, uniform container padding, and optional root-based integer presentation scaling; margins, alignment modes, scrolling, and percentage sizing remain deferred. Every runtime starts with one built-in dark DSi-inspired global theme: deep navy canvas, raised slate-blue `surface` panels with four-sided bright/dark bevels, cobalt `accent` buttons with matching raised outlines, and cyan focus. A complete replacement is validated and copied atomically; per-node styling and a project styling language remain deferred. Renderer supports clear/fill rectangles and embedded DejaVu Sans Mono TTF glyph runs with UTF-8 decoding and alpha coverage; the 5×7 ASCII face remains a deterministic fallback. Label and button text has an explicit clip, ellipsis, or deterministic wrap policy and never scales down to fit. It has no shaping, kerning, fallback-face selection, or glyph cache yet. X11 handles pointer buttons, visible Tab/Enter/Space button interaction, and a basic US-layout ASCII key/text mapping. Wayland uses the compositor keymap through xkbcommon for basic key/text events. Both hosts are software presenters with no IME, clipboard, advanced layout support, acceleration, or sophisticated pacing. The builder workbench is fixture selection and status feedback only: it has no project file I/O, text editing, compiler invocation, or export yet. System tools remain non-reproducible development fallbacks.

## Deferred features

Visual builder, Nim, managed toolchains, build caches, advanced text shaping/caching, and public platform targets other than the optional Linux X11/XCB and Wayland hosts are deferred.
