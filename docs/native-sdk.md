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

## Original animation code

`game_anim` compiles `src/main/graphics/anim.c` directly. Byte-pointer offset
arithmetic replaces the compiler-specific `void*` addition. Native builds omit
the farm-state include used only for PS1 return-address diagnostics; those
MIPS addresses have no native meaning. No playback or drawing rule is replaced.
`LINK_PRIM` routes through the same mapped SDK links as `addPrim`, preserving
the original packet length and preventing x64 pointer truncation.

Original fatal debugger waits terminate through the native `host_poll` trap.
Font diagnostic output exists only in Debug; Release discards calls and their
arguments at compile time. The native compatibility header does not supply
fake implementations for unported scratchpad or other CPU operations.

Synthetic fixtures invoke the actual original duration/hold/pause logic and
SPRT/FT4 generation, including UV orientation and primitive-cursor advancement.
They supply the main module's frame pointer as fixture state. Uniform texels
isolate packet generation from the backend's incomplete polygon interpolation;
independent nonuniform hardware fixtures remain required for texel fidelity.

`game_frame_buffers` compiles the original frame layout and cursor insertion.
Native insertion replaces only the 24-bit pointer cast with SDK link mapping.
The original OT dimensions, memory offsets, clearing and cursor advancement
remain authoritative. Tests allocate a sufficiently large host region, map it
once, invoke the original layout for both frames, then submit inserted packets.
Frame storage is runtime-only; its host pointer layout is never serialized as
a PS1 frame record or saved game.

## SDK timing

`sdk_sync` supplies `VSync`: mode 0 waits for the next vblank, mode 1 queries
elapsed horizontal units without waiting, n greater than 1 waits until n
vblanks since the last completed wait, and negative modes query the absolute
vblank count. This follows the SDK runtime reference linked above. The initial
implementation uses nominal 60 Hz and 15,750 horizontal units per second;
exact NTSC drift, scanline position and interrupt callbacks remain unmodeled.

The default clock and absolute-deadline wait use SDL. Synthetic tests bind a
deterministic clock to exercise deadlines without sleeping. Deadlines retain
their phase and late work does not incur another full wait. Repeated
`ResetCallback` calls, as used by original disc reads, initialize the timing
source once without rewinding it. No interrupt callback registration API is
provided yet; this is not full BIOS callback emulation.
