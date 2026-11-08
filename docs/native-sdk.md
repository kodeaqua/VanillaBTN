# Native SDK boundary

The native target compiles `src/main/graphics/texture_pack.c` directly into
`game_texture_pack`. It does not copy its decoding or placement rules into a
replacement loader. Native declarations under `native/compat` and the
`sdk_gpu` implementation supply the currently supported platform boundary.
These are project-owned declarations, not bundled proprietary SDK headers.

`LoadImage` writes packed 16-bit pixels to the host VRAM; `StoreImage` reads
them back. Row widths are halfwords, even though the API pointer is word-typed.
Transfers finish synchronously, so `DrawSync(0)` and `DrawSync(1)` report an
empty queue. There are no callbacks or asynchronous GPU commands yet. The
entry-point signatures and transfer semantics follow the
[SDK runtime reference](https://psx.arthus.net/sdk/Psy-Q/DOCS/LibRef47.pdf).

The host binds its VRAM with `sdk_gpu_bind` and unbinds it before shutdown.
There is one GPU context, used on the host thread. Invalid transfer rectangles
return -1 before modifying VRAM. GPU drawing environments do not affect image
transfers. No rendering, animation, or game state machine is added here.

MSVC rejects arrays of structures containing zero-length trailing arrays.
`VANILLABTN_NATIVE` therefore uses one-element trailing placeholders in the
texture bank declarations. Static checks preserve the image offset (4 bytes)
and pixel payload offset (12 bytes). File iteration uses `BANK_NEXT`, never
native `sizeof(ImageBank)` or `sizeof(BankImage)`. PS1 declarations stay intact.

Unused private original helpers reference animation code not compiled yet.
Function sections and dead-code elimination discard those unreachable helpers;
there are no pretend animation implementations. Primitive declarations preserve
the PS1 wire layout. Unsupported SDK operations remain build or link errors
rather than silent no-ops.

`original_texture_pack` tests real original loader calls with synthetic banks,
raw/RLE pixels, placement offsets and image-bank expansion. Transfer fixtures
also cover odd widths, unaligned buffers, boundary pixels, readback and invalid
rectangles. The game is still not connected to the main loop; this validates
the first original graphics module's platform boundary, not gameplay.

The original loader assumes valid banks, dimensions within its fixed row
buffer, and valid image indices. It does not accept a byte length or validate
malformed input; an absent image index can leave its result undefined. These
original preconditions are not relaxed by compiling it natively. A validated
asset handoff remains necessary before connecting imported data to gameplay.

## Original primitive submission

`game_prim` compiles the original `prim.c`. SDK packet declarations retain their
32-bit tags and GP0 payload fields; static fixtures check TILE/SPRT/FT4 sizes.
The host supplies bounded GS work storage through `sdk_work_bind`; the original
builders still own command construction, colors, coordinates and packet sizes.
Arena exhaustion stops execution instead of corrupting adjacent host memory.

Before building a frame, register its complete OT and packet arenas through
`sdk_gpu_map`. Tags use a 24-bit mapped offset, never a truncated host pointer.
Mappings are disjoint and frame-local; reset them only after draws complete.
`ClearOTagR` links each OT entry to its predecessor. `addPrim` inserts at the
head of that entry; `DrawOTag` resolves links and submits packets to the existing
GPU dispatcher. The dispatcher still has incomplete primitive coverage.

Unmapped, unaligned and out-of-range links are rejected, and a traversal budget
terminates cycles. `sdk_gpu_ok` reports a sticky submission error until mapping
reset. The host must check it when wiring the original frame loop. Existing
single-GPU synchronous transfer and lifetime constraints still apply. No game
scene is connected by this step.
