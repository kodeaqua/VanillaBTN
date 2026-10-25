# Local Windows disc validation

The image/boot tools below are diagnostics. They do not yet provide a playable
title screen, audio playback, native saves or host input.

The data-free tests run with the normal Windows presets. A separate diagnostic
reads the owner's original USA SLUS_011.15 raw BIN track without extracting or
writing any game files:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-release
& ./build/port/windows-release/hmbtn_disc_probe.exe 'D:/my-disc/Harvest Moon - Back to Nature (USA).bin'
```

Replace the example path with the locally owned track. Do not add the BIN, CUE,
ISO, extracted assets or local absolute paths to git. The probe accepts the BIN
directly; CUE parsing and the game's SDL first-run picker remain pending. This
diagnostic's stdio stream limits offsets to LONG_MAX; it is not the final SDL
filesystem layer and does not support large multi-track images or Android SAF.

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

Current observed stop: `Unimplemented PSX API: MemCardInit`, with a nonzero abort
exit. Before that point, the original code performs callback/VSync setup, real
A_FILE reads, silent sound configuration, and GTE/GPU environment setup. This
is an expected bring-up failure, not a completed boot. Unsupported SDK calls
must retain their named failures while implementations are added. The console
probe uses virtual time and has no SDL window; hardware presentation is tested
separately in GPU_TESTING.md. Native overlay reentry also remains unsupported.
