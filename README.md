# VanillaBTN

VanillaBTN is working toward a native Windows port of *Harvest Moon: Back to
Nature*. Its goal is to preserve the original game's play, pacing, audiovisual
identity, and content. Community mods can be creative, but the default VanillaBTN
experience should remain faithful to the unmodified game. Any future mod support
should be optional and separate from that default experience.

## Current status

This repository contains a matching decompilation of the PlayStation USA
release (`SLUS_011.15`) and a minimal native Windows bringup executable. The
native executable presents a blank PS1-size frame; it does not run the game
yet. The PS1 source builds the EXE and its 13 overlays byte for byte and,
given your own copy of the disc, the whole disc image. That build remains
the reference baseline. Building it with Docker on Windows still produces
PS1 binaries.

The [Windows roadmap](docs/roadmap.md) orders the bringup work, and the
[porting rules](docs/porting.md) define its fidelity boundaries. The
[architecture guide](docs/architecture.md) maps the current code.
[AGENTS.md](AGENTS.md) records repository rules for coding agents.
The [versioning policy](docs/versioning.md) explains the project's development
version, which can be printed with `python tools/version.py`.

## Building

On Windows, build the native bringup executable from PowerShell with Visual
Studio's Desktop development with C++ installed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-windows.ps1 -Configuration Debug
```

See [building instructions](docs/building.md) for Release and the PS1 reference
build. The native build does not use Docker, WSL, or MSYS.

For the PS1 reference build on Linux or WSL:

```sh
docker build -t vanillabtn-build .
mkdir -p /tmp/vanillabtn-build
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/src:ro -v /tmp/vanillabtn-build:/build \
    -v /path/to/disc:/disc:ro vanillabtn-build bash -c 'cmake -S /src -B /build -G Ninja \
    -DORIGINAL_DISC=/disc/disc.bin && cmake --build /build --target check'
```

`check` compares every output with the original's SHA-1. Without `-DORIGINAL_DISC` it
builds and checks only the 14 code binaries, which needs no game data.

For Windows PowerShell, use the container build helper described in
[docs/building.md](docs/building.md). It avoids problems with the old compiler
on Windows bind mounts and CRLF checkouts.

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

[MIT](LICENSE) covers this repository's own work in the current revision.
Earlier revisions published under CC0 retain that grant. `tools/maspsx` keeps
its own license. The game and the PSY-Q SDK belong to their owners and are not
included.
