# TODO

This file tracks only active engineering work and developer acceptance that is
still required. Completed implementation milestones and their limitations are
recorded in [Work 02](docs/02-enhancements.md).

## Gameplay and visual acceptance

- [ ] **Left panorama seam** — negative filtered subpixels now use floor
      division, including mask lookups and repeated backgrounds. Recheck the
      title-screen line; synthetic coverage does not prove its visual cause.
- [ ] **GPU renderer migration** — move rasterization and texture filtering
      onto the host GPU, preserve PS1 transfer/readback semantics, and compare
      against the software reference. This migration is not implemented yet.

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
