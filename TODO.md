# TODO

This file tracks **outstanding engineering work**. Implementation details for
completed features are maintained in [docs/enhancements.md](docs/enhancements.md).
The native Windows port must preserve the original game's behavior and the PS1
reference build; see [AGENTS.md](AGENTS.md) and [docs/architecture.md](docs/architecture.md).

## Graphics and display

- [x] **Milestone 21: Scalable host 3D raster** — projected polygons are
      sampled independently at integer multiples of PS1 resolution on a
      separate host surface. PS1 VRAM, 4:3 geometry, and 2D draw coordinates
      retain their original resolution. Scale 1 preserves the existing path.
- [x] **Milestone 22: Host render-scale presentation** — `render_scale =
      1..16/native` selects the supersampled software target and SDL texture;
      `native` follows output height, while allocation limits can reduce the
      effective multiplier. Logical camera and HUD coordinates stay unchanged.
- [x] **Milestone 19: CRT NTSC pixel aspect** — `pixel_aspect_ratio = crt_ntsc`
      corrects physical presentation width without changing game/HUD coordinates;
      explicit anamorphic `widescreen = stretch` continues to override PAR.
- [x] **Milestone 23: Farm-world sprite culling in Hor+** — the native-only
      farm visibility band includes the additional Hor+ view on both edges,
      allowing farm actors/objects and weather sprites through the original
      screen-box rejection. The vertical range, coordinates, collision and PS1
      reference path are unchanged. Other scenes need runtime inspection for
      separate game-side visibility gates.

## Runtime and input

- [x] **Milestone 18: Disconnect hold** — `pause_on_disconnect` now holds
      the original VSync loop and excludes held time, sharing unfocus suspension.

## Save compatibility

- [x] **Milestone 20: Persistent card unformat** — the original card utility
      can explicitly unformat a virtual card. Its raw image remains unformatted
      across restarts and prior saves remain in the rotated backup chain.

## Audio fidelity

- [x] **Milestone 24: Optional full-rate stereo Hi-Fi reverb** — the existing
      `hifi_reverb` flag now selects a 44.1 kHz stereo wet path, while the
      authentic 22.05 kHz alternating/FIR path remains unchanged. Existing ADSR
      clock/step regression cases were reviewed without changing envelope
      timing; exact hardware-waveform equivalence still requires reference
      traces and developer listening acceptance.

## References

- [Enhancements](docs/enhancements.md) — implemented features and limitations.
- [Roadmap](docs/roadmap.md) — longer-term port architecture.
- [Building](docs/building.md) — Windows build and verification commands.
