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

## Remaining bringup blockers

- `native/src/main.c` creates and presents zeroed VRAM but invokes no game scene,
  imports no assets, starts no sounds, and exposes no in-game save flow. This
  explains the black window and silence. Standalone subsystem tests do not
  establish runtime integration.
- SDL initializes video/audio only and does not open gamepads. The gamepad event
  mapping therefore does not establish controller support yet.
- `ps1_xa.c` ignores sector coding-info when selecting sample rate and channel
  layout. Its single prediction history and mono sample buffer cannot reproduce
  stereo XA. Rate is fixed to 18.9 kHz. Independent mono/stereo fixtures are
  required before treating XA playback as faithful.
- `ps1_spu.c` provides sample voices but no SEQ event scheduler, ADSR envelopes,
  or reverb. The roadmap's music milestone is incomplete.
- `ps1_gte.c` is a mathematical approximation: perspective division, register
  saturation/flags, and average-depth scaling still need hardware-oriented
  fixtures. Primitive coverage also needs shared-edge transparency checks.
- Card read/write round-trips only prove agreement with this implementation.
  Directory chain encoding, malformed chains, recovery validation of the entire
  card, and the game's save bytes need independent fixtures. Existing tests do
  not justify the original compatibility/UI claims.

Milestone completion labels in `TODO.md` now identify these as foundations.
Finish these blockers in focused changes before returning to scene bringup.

## Verification

Both native Debug and Release compile and pass all 12 CTest checks with active
assertions. The 14-file PS1 code check passes. This does not validate audiovisual
or gameplay fidelity, and no original assets or SDK files were added.
