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

- [x] **NPC dialogue crash (native gameplay)** — the Debug report captured a
      `0xC0000005` read at address `0x2` in `sprite_draw_frame`. Milestone 28
      corrected the x64 sprite-state corruption in event entry; the developer
      retested the conversation in Windows and confirmed the crash is fixed.
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

- [x] **Milestone 33: Empty `.ANI` part lists in native drawing** — avoid
      forming a pointer before the part array when a sprite, text sprite or
      SPRT builder sees zero/negative part count. PS1 draw logic stays intact;
      animation packet fixtures cover each native builder.

- [x] **Milestone 39: Native photo-map marker bounds** — when the player or
      dog has a location without a map spot, skip that marker instead of
      dereferencing `photo_map_spots[-1]`. The cursor and known locations are
      unchanged; the PS1 reference path is preserved. Verify the map with the
      dog indoors/outdoors in native gameplay.

## Save compatibility

- [x] **Milestone 20: Persistent card unformat** — the original card utility
      can explicitly unformat a virtual card. Its raw image remains unformatted
      across restarts and prior saves remain in the rotated backup chain.

## Audio fidelity

- [x] **Milestone 36: Optimized Debug stall trace** — `--trace-stalls`
      records in-memory timing outliers across VSync game work, waits, SDL
      events, input, audio and presentation. `DebugOptimized` retains Debug
      diagnostics while using `/O2`; normal Release excludes all tracing.
- [x] **Title-screen music continuity** — the observed audio lag/dropouts
      are resolved after Milestone 38's bounded SDL catch-up queue. New
      dropouts should be investigated separately rather than expanding the
      queue or altering the game/SPU clock without evidence.
- [x] **Milestone 38: Bound SDL catch-up queue depth** — a four-tick audio
      catch-up could push the SDL queue beyond its old eight-tick threshold,
      delaying playback before later submissions were skipped. Account for
      onset reserve bytes and trim catch-up submissions to four queued ticks,
      retaining the newest PCM while the SPU always advances in full.
- [x] **Milestone 37: Native output sample-clock pacing** — generate host PCM
      for elapsed wall time rather than assuming every VSync callback arrives
      on schedule. Cap catch-up at four ticks, preserve accelerated turbo
      audio, reset after suspension and keep the PS1 sound state authoritative.

- [x] **Milestone 35: Music-onset SDL queue reserve** — when a newly
      playing music sequence first produces audible PCM, prefill at most two
      60 Hz ticks of silence before its first sample to tolerate a short
      title-loading stall. Later ticks are unchanged, and game/SPU clocks
      are not advanced for the reserve.
- [x] **Music onset reserve (Windows initial check)** — the audio onset itself
      improved after Milestone 35; a later, simultaneous frame/audio hitch
      remains tracked separately above.

- [x] **Silent native playback** — Windows Release playback has been confirmed
      audible; the later one-off title hitch is tracked separately.
- [x] **Milestone 34: MSVC Debug audio report compilation** — isolate the
      Windows temporary-file API in the Debug crash-report module so PS1
      `RECT` and Windows SDK declarations do not collide in `main.c`.

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
