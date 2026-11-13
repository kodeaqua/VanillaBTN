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

- [ ] **NPC dialogue crash (native gameplay)** — the Debug report captured a
      `0xC0000005` read at address `0x2` in `sprite_draw_frame`. Milestone 28
      corrects one concrete x64 sprite-state corruption in event entry, but the
      conversation must be retested in Windows before closing this issue.
- [x] **Milestone 28: Native event villager sprite-state copy** — use the real
      `FarmVillager` stride and full `AnimState` size on x64, rather than the
      PS1-only 48-byte copy at a fixed `0x1D4` stride; keep reference code intact.
- [x] **Milestone 27: Native Debug crash reports** — the game executable now
      records exceptions, explicit `SystemError` codes and numeric menu/dialog
      state in a local crash report, with call-stack symbols when available.
      The handler and its DbgHelp dependency are absent from Release; gameplay
      and the PS1 reference paths are unchanged.

- [x] **Milestone 26: Empty text safety** — the native shared text renderer
      and line measurement avoid reading before an empty optional label, and
      glyph-line backtracking is skipped at index zero. The PS1 path is unchanged.
- [x] **Milestone 25: Native text-insert source bounds** — the shared
      dialogue, event-text and menu formatter resolves optional source slots
      only inside its 12-entry table; missing or unknown insertion codes are
      skipped rather than indexing host memory beyond the table.

- [x] **Milestone 18: Disconnect hold** — `pause_on_disconnect` now holds
      the original VSync loop and excludes held time, sharing unfocus suspension.

- [x] **Milestone 29: Native chicken slot clearing** — replace the original
      0xB4-byte PS1 clear with a full `FarmChicken` clear in native builds,
      avoiding stale host pointers in freed chicken slots. The PS1 path retains
      its original constant; the native farm regression checks the whole record.

- [x] **Milestone 30: Native farm action slot clearing** — replace the original
      0xB0-byte PS1 clear with `sizeof(FarmAction)` in native builds; ensure
      freed action records discard all x64 animation pointers before reuse.

## Save compatibility

- [x] **Milestone 20: Persistent card unformat** — the original card utility
      can explicitly unformat a virtual card. Its raw image remains unformatted
      across restarts and prior saves remain in the rotated backup chain.

## Audio fidelity

- [ ] **Silent native playback** — verify the actual output device and PCM
      production on Windows. The Debug report can distinguish mixer silence
      from an SDL transport failure; runtime acceptance is still required.
- [x] **Milestone 31: Debug audio transport diagnostics** — check audio stream
      resume status, show a warning when device playback cannot start, and
      write mixer/queue counters to `%TEMP%/VanillaBTN-audio.txt` only in Debug.
      Preserve headless test behavior and keep the PS1 reference untouched.

- [x] **Milestone 32: Hi-Fi dry audio regression** — verify the native SPU
      produces the same nonzero dry PCM with authentic or Hi-Fi reverb when
      reverb sends are disabled; this does not validate a Windows output device.

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
