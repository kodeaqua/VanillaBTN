# Work 03: Native GPU rendering

## Status

Active design and implementation work, authorized by the developer after
confirming the panorama background regression fixed in `6fc995c`.
No GPU raster backend has been implemented yet. See `TODO.md` for active work.
The existing branch and PR #3 still contain the enhancement baseline.

## Vision

Move rasterization, texture sampling, filtering, and presentation to the host
GPU. The modern preset should ultimately use this renderer by default once
its acceptance criteria pass. Preserve the original native game code and
simulation cadence. The authentic preset retains the software reference.
This work does not change VERSION_MAJOR.

## Scope and boundaries

- GPU rendering includes sprites, tiles, flat and Gouraud polygons, lines,
  panoramic extensions, depth, transparency, masks, and display composition.
- CPU code continues running game logic, command submission, input, archive
  access, and audio. GPU rendering does not mean moving these systems to GPU.
- PS1 texture addresses, CLUTs, framebuffer pages, and transfer formats remain
  compatibility interfaces. Physical host resources need not share their size.
- No original assets, disc data, or SDK files may be committed.
- The matching PS1 EXE and overlays remain the reference build.
- GPU results must not be uploaded from a fully CPU-rasterized framebuffer
  during normal rendering. That would preserve the current CPU bottleneck.

## Architecture and approach

| Boundary | Responsibility |
| --- | --- |
| SDK/packet decoder | Preserve ordering tables, draw state and command order |
| Software renderer | Reference tests and explicit authentic/recovery mode |
| GPU renderer | Own render surfaces, texture resources and rasterization |
| Transfer synchronization | Flush preceding writes before StoreImage or CPU reads; preserve MoveImage ordering and overlap behavior |
| Presentation | Display GPU surfaces directly, retaining integer scale, aspect, panoramic composition and double-buffer persistence |

Start with a Windows D3D11 implementation, subject to a resource and shader
prototype. Device creation must report failure explicitly and select a complete
software recovery path; it must not silently mix unsynchronized surfaces.
The existing SDL backend setting currently selects presentation only and must
not be described as proof of GPU rasterization.

Use a canonical 16-bit GPU VRAM resource for game-visible words plus host color
and panoramic surfaces. Preserve texture fetches from framebuffer regions,
palette changes, and mask semantics before enabling enhancements. Readback is
allowed at explicit game transfer boundaries and in synthetic verification;
normal presentation must not read a whole frame back to CPU.

## Work plan

| Order | Work | Agent verification | Developer acceptance |
| --- | --- | --- | --- |
| 1 | Device/resource/shader prototype and command boundary | Headless GPU resource upload/readback fixture; explicit device failure test | Driver/device compatibility |
| 2 | Upload, fill, copy, readback and mask semantics | Compare raw words against software, including overlapping copies | Loading and transitions |
| 3 | Sprites, tiles, lines and blend modes | Synthetic palettes, clipping, modulation, transparency and mask fixtures | Title, menus, dialogs and weather |
| 4 | Flat/Gouraud/textured polygons and geometry enhancements | Shared-edge, UV, depth, perspective and ordering fixtures | Farm and other scenes |
| 5 | GPU panorama and direct presentation | Alternating pages, persistent buffers, fade and resize fixtures; no normal frame readback | Seam-free widescreen and integer scale |
| 6 | GPU filtering and replacement cache | Cross-tile background filtering and cache invalidation fixtures | Image quality and HD pack routing |
| 7 | Default modern rollout | Both profiles, diagnostic exclusion, 14-file PS1 check and reproducible timings | Gameplay and representative GPU/driver acceptance |

## Verification

No GPU implementation checks have run yet. Each milestone must compile in
Debug and Release. Synthetic GPU tests must identify the actual device and
must fail or explicitly report unavailable hardware rather than count an
unexecuted GPU path as passed. Compare small synthetic results with the
software reference, including draw/read/draw sequences.

Measure command decoding, CPU raster work, GPU submission, transfer waits and
presentation separately. Keep timing comparisons at identical scene fixtures,
resolution, filtering and pacing settings. Do not promise a particular FPS
before measuring it. Developer diagnostics remain Debug-only.

## Implementation notes

`Ps1Video` currently exposes CPU VRAM, color, depth and palette arrays.
`sdk_gpu.c` implements synchronous LoadImage, StoreImage, MoveImage and DrawSync.
`ps1_gpu.c` decodes packets into `ps1_video` drawing calls. `host_display.c`
currently uploads the CPU-composed frame to an SDL streaming texture. These
boundaries all need migration; changing SDL's renderer driver alone cannot
complete this work.

Background panorama filtering remains on the known-good mirror until a
composed GPU surface can sample across tile and center/side boundaries.
