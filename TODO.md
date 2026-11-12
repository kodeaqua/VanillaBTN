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
- [x] **Milestone 19: CRT NTSC pixel aspect** — `pixel_aspect_ratio = crt_ntsc`
      corrects physical presentation width without changing game/HUD coordinates;
      explicit anamorphic `widescreen = stretch` continues to override PAR.
- [ ] Address world actor/sprite pop-in at very wide viewport edges. The game
      can reject actors before they reach the native renderer; any adaptation
      must preserve original collision, object positions, and the PS1 path.

## Runtime and input

- [x] **Milestone 18: Disconnect hold** — `pause_on_disconnect` now holds
      the original VSync loop and excludes held time, sharing unfocus suspension.

## Save compatibility

- [x] **Milestone 20: Persistent card unformat** — the original card utility
      can explicitly unformat a virtual card. Its raw image remains unformatted
      across restarts and prior saves remain in the rotated backup chain.

## Audio fidelity

- [ ] Continue SPU envelope/reverb behavior review where exact hardware
      equivalence matters; avoid changing established gameplay audio timing.

## References

- [Enhancements](docs/enhancements.md) — implemented features and limitations.
- [Roadmap](docs/roadmap.md) — longer-term port architecture.
- [Building](docs/building.md) — Windows build and verification commands.
