# Local Windows disc validation

The hmbtn_disc_probe below is a disc/parser diagnostic; it does not boot the
game or validate rendering, audio, input, or save behavior. The separate
Windows -Game build reaches the title and farm; see WINDOWS_PLAYTEST.md for
current gameplay limits and owner checks.

The data-free tests run with the normal Windows presets. A separate diagnostic
reads the owner's original USA SLUS_011.15 raw BIN track without extracting or
writing any game files:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release
& ./build/port/windows-release/hmbtn_disc_probe.exe 'D:/my-disc/Harvest Moon - Back to Nature (USA).bin'
```

Replace the example path with the locally owned track. Do not add the BIN, CUE,
ISO, extracted assets or local absolute paths to git. The probe accepts raw BIN
directly. It does not validate CUE parsing, Windows runtime disc discovery, or
Android SAF import. This diagnostic's stdio stream limits offsets to LONG_MAX;
it is not the final SDL filesystem layer and does not support large multi-track
images or Android SAF.

The runtime hashes the complete track before mounting it and rejects a wrong
region/revision. Validation checks ISO-9660 metadata and reads the initial
sectors of SYSTEM.CNF, SLUS_011.15, A_FILE.HDT and A_FILE.BIN via the parser and
the SDK-facing CD interface. It then checks the 33 archive offsets, without
logging contents. It does not execute the PS1 EXE or overlay binaries.

Validated on 2026-10-25 in Windows release and ASan/UBSan: 90967 raw sectors,
archive size 133935104 bytes, 32 bounded member ranges. No game boot, rendered
title screen, audio or gameplay is claimed by this result.

For the sanitizer executable, provision the toolchain in the same PowerShell
process before running so its ASan DLL is on PATH:

```powershell
# In a PowerShell process with script execution enabled for this invocation:
& ./port/packaging/windows/setup-toolchain.ps1
& ./build/port/windows-asan-ubsan/hmbtn_disc_probe.exe 'D:/my-disc/Harvest Moon - Back to Nature (USA).bin'
```

The separate boot probe links the reachable original game/overlay C graph and
enters hm_game_main after validation. Build its excluded target explicitly:

```powershell
# After setup-toolchain.ps1 in the same PowerShell process:
cmake --build build/port/windows-release --target hmbtn_boot_probe
& ./build/port/windows-release/hmbtn_boot_probe.exe 'D:/my-disc/Harvest Moon - Back to Nature (USA).bin'
```

The native title/menu now run, and local release/sanitizer replays complete
naming/birthday, the opening scenes and free movement in the house. See
WINDOWS_PLAYTEST.md for the authored replay and verified capture hash.
Unsupported SDK calls retain named failures while broader gameplay is tested.
The console probe uses virtual time and has no SDL window. Farm overlay reentry
and native file-backed memory-card containers have since been implemented;
complete save/load and other-overlay coverage remain owner-unverified.

For the optional SDL game-window diagnostic, configure the existing GPU spike
build with `-DHMBTN_BUILD_NATIVE_WINDOW=ON`, then build `hmbtn_native`. It accepts
the same BIN argument. This is a development build with limited gameplay
coverage. Vulkan captures show the title, menu, naming dialogue and farm. Keyboard
arrows map to the D-pad, X/Z to Cross/Square, W/D to Triangle/Circle, Return to
Start and Backspace to Select. Replay input is validated through the original
pad polling code; physical keyboard interaction remains unverified.
The development-only HMBTN_CAPTURE_FRAME environment variable can name a local
BMP output; it overwrites the last scanout each tick. Keep captures under the
ignored build directory. It is unset by default and is not release telemetry.

Local diagnostics accept HMBTN_INPUT_REPLAY: a text file containing one
`decimal_tick hexadecimal_PadButton_mask` transition per line, ordered strictly
by VBlank tick. Blank lines and whole-line # comments are allowed. After the
last transition, the final held mask persists, so finish with a zero mask.
With replay configured, its pad samples override keyboard input. Clear that
environment variable before manual play testing.

HMBTN_BOOT_TICKS bounds either probe to 1..1000000 VBlanks. Reaching the limit
prints a diagnostic completion message; that is not a completed game boot or
playthrough. The window probe paces against host time; the console probe runs
virtual ticks as fast as the reference renderer allows. Keep replays/captures
under ignored build output unless they contain only authored input transitions.
