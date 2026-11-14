# TODO

This file tracks only active engineering work and developer acceptance that is
still required. Completed implementation milestones and their limitations are
recorded in [Work 02](docs/02-enhancements.md).

## Gameplay and visual acceptance

- [ ] **Left panorama seam** — panorama backgrounds now use the known-good
      mirror from `76727ca`, bypassing per-tile host filtering. A regression
      test proves uniform samples across the center/side boundary; recheck
      the title-screen line visually. Composed-background filtering remains
      pending as part of the GPU renderer migration.
- [ ] **GPU renderer migration** — follow [Work 03](docs/03-native-gpu.md).
      Next: device/resource/shader prototype and a synthetic GPU transfer test,
      then ordered transfers and primitive rasterization. GPU implementation
      remains pending; the software reference is still the active renderer.

- [ ] **Host texture filtering** — compare `nearest`, `xbrz`, and the separate
      `bilinear`/`bicubic` background modes across farm actors, UI, title/menu
      art, maps, rain and projected terrain. Confirm xBRZ does not stylize
      explicitly marked prerendered backgrounds and assess frame pacing with
      `render_scale = native`, including the deterministic host-filter budget;
      synthetic tests establish routing and VRAM
      isolation, not perceived image quality.
- [ ] **Hor+ scene coverage** — inspect scenes outside the farm for game-side
      visibility gates that still reject actors, objects, weather, or other
      world content inside the expanded horizontal view.
- [ ] **Photo-map marker bounds** — verify native photo-map navigation with the
      dog indoors and outdoors, including locations that have no map marker.

## Audio acceptance

- [ ] **Hi-Fi reverb fidelity** — compare the optional full-rate stereo reverb
      against objective hardware traces where available and perform developer
      listening checks. Existing synthetic tests establish code behavior, not
      PlayStation hardware equivalence or perceived audio quality.

## References

- [Work 02](docs/02-enhancements.md) — completed enhancements, implementation
  notes, and known limitations.
- [Work 01](docs/01-porting.md#work-plan) — baseline port architecture and roadmap.
- [Building](docs/building.md) — Windows build and verification commands.
