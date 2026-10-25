# Harvest Moon: Back to Nature

A matching decompilation of *Harvest Moon: Back to Nature* (PlayStation, USA,
`SLUS_011.15`). The C source builds the EXE and its 13 overlays byte for byte and, given
your own copy of the disc, the whole disc image.

## Building

The native port currently targets Windows first. To build and test its Phase 0
foundation and initial source subset locally (not a playable game yet):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1
```

This provisions project-local tools and requires no WSL or PSX SDK. No workflow/CI
changes are included. See [port status](docs/port/ANALYSIS.md).

For the original matching binaries:

Build directly on a separate Linux x86-64 host, without containers.
See [native setup and prerequisites](docs/port/NATIVE_BUILD.md). Supply your own
PSY-Q 4.7 archive; SDK files and game data must remain outside version control.

```sh
PSYQ_ARCHIVE=/path/to/runtime-4.7.zip bash tools/setup-native.sh
cmake -S . -B build/match -G Ninja
cmake --build build/match --target check
```

`check` compares every output with the original's SHA-1. Without `-DORIGINAL_DISC` it
builds and checks only the 14 code binaries, which needs no game data.

The separate native port is in Phase 0. Its presets live in `port/` and currently
build the compatibility layer and ten source units, including the archived game
entry point and A_FILE reader. No playable game executable is linked yet. See
[reconnaissance and outstanding gates](docs/port/ANALYSIS.md).

## Exceptions

Two things in the build are not compiled from source:

- **One patched SDK word.** The game links PSY-Q 4.7, but the only 4.7 release
  available has a later `LIBCD.LIB`: it is dated 2000-05-10, while every other 4.7
  library is dated 2000-02-18. The game's copy of that library has one word different, a
  flag in `BIOS_1`'s `.data`, so the build patches it back (`sdk/patches.txt`).
- **Padding bytes.** Where a file's data ends off a 4-byte boundary, the original
  build padded it. In the EXE, and in Sony's own library objects, that padding is zero.
  In 13 places in the overlays it is not: it holds leftover memory, mostly text with
  DOS line ends (`\r\n`, `\t0`; one is `te\t0\r\n`, the end of an assembler line
  `.byte 0`). No object holds these bytes, so each overlay lists them as `GAP` entries
  in its `CMakeLists.txt`.

## License

[CC0 1.0](LICENSE), covering this repository's own work only; `tools/maspsx` keeps its
own license. The game and the PSY-Q SDK belong to their owners and are not included.
