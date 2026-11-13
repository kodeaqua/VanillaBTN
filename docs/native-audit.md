# Native bringup audit

Reviewed the native work from `fb5cfd8` through `3895816`, the current runtime,
tests, and repository guidance. This is a source and synthetic audit; gameplay
acceptance remains the developer's responsibility.

## Corrected defects

- Release defined `NDEBUG`, disabling assertion-based checks in several native
  tests. Test executables now explicitly undefine it while linking optimized
  Release libraries.
- Replacing a card file deleted its old blocks before checking capacity. A failed
  replacement could lose the save. Mutations now use a staged card copy and
  commit only on success. Regression checks cover a full card and an oversized
  input, including preservation of all original bytes.
- XA interpolation and volume products, and SPU master-volume multiplication
  after summing voices, could overflow signed 32-bit arithmetic. These products
  now use 64-bit intermediates. A full-scale XA regression checks the output.

## Findings at audit time

These findings describe the code at the audited commits. They were rechecked
against the current source by reading code and tests only; none was verified
against hardware or runtime playback, and none establishes fidelity. Track
current open work in `TODO.md`.

| Finding at audit time | Current source state | Still open |
|---|---|---|
| `main.c` presented zeroed VRAM and ran no scene | Startup calls the original game loop and dispatcher | Runtime fidelity is the developer's to judge |
| SDL opened video/audio only | `SDL_Init` includes `SDL_INIT_GAMEPAD`; two slots and rumble are wired | Hands-on controller check |
| XA ignored coding info, mono history, fixed 18.9 kHz | `ps1_xa.c` reads the sector coding byte for stereo and rate; stereo decode keeps per-channel history; `ps1_xa_test.c` has a stereo fixture | Mono-versus-stereo fixtures cover one sector shape; listening check |
| No SEQ scheduler, ADSR or reverb | `ps1_seq.c` and `ps1_reverb.c` exist with tests, and `ps1_spu.c` has an ADSR generator | Accuracy against hardware is unmeasured |
| GTE was an approximation | Perspective divide uses the hardware UNR table and sets flag bits on saturation | Wider register and flag fixtures; shared-edge transparency checks |
| Card tests only proved self-agreement | Tests corrupt magic, checksums, directory frames and recovery files | Independent fixtures for chain encoding, malformed chains and real save bytes |

## Verification

Both native Debug and Release compile and pass all 74 CTest checks (the count
grows as tests are added) with active assertions. The PS1 `check` target passes. This does not validate audiovisual
or gameplay fidelity, and no original assets or SDK files were added.
