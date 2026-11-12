# TODO

This file tracks **outstanding engineering work**. Implementation details for
completed features are maintained in [docs/enhancements.md](docs/enhancements.md).
The native Windows port must preserve the original game's behavior and the PS1
reference build; see [AGENTS.md](AGENTS.md) and [docs/architecture.md](docs/architecture.md).

## Graphics and display

- [ ] Add actual native-resolution 3D polygon rasterization. The current
      software renderer expands the horizontal field of view but still uses a
      240-row internal raster target. Keep original 4:3 presentation available.
- [ ] Connect `render_scale` to the render target when scalable rasterization
      is implemented; it currently affects configuration storage only.
- [ ] Implement `pixel_aspect_ratio = crt_ntsc` presentation semantics without
      changing game geometry or stretching HUD elements.
- [ ] Address world actor/sprite pop-in at very wide viewport edges. The game
      can reject actors before they reach the native renderer; any adaptation
      must preserve original collision, object positions, and the PS1 path.

## Runtime and input

- [ ] Developer to verify `pause_on_unfocus = on` in a real window: the game
      should freeze on focus loss and resume without fast-forwarding. Code and
      the synthetic clock test are done; runtime behavior is unverified.
- [ ] `pause_on_disconnect` sets `paused`, which skips input sampling but does
      not hold the original loop. Decide whether it should share the unfocus hold.

## Save compatibility

- [ ] Determine whether persistent memory-card unformat is required for the
      original card utility; current disk-backed unformat requests are not
      implemented. Preserve original save-image compatibility if added.

## Audio fidelity

- [ ] Continue SPU envelope/reverb behavior review where exact hardware
      equivalence matters; avoid changing established gameplay audio timing.

## References

- [Enhancements](docs/enhancements.md) — implemented features and limitations.
- [Roadmap](docs/roadmap.md) — longer-term port architecture.
- [Building](docs/building.md) — Windows build and verification commands.
