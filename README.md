# Harvest Moon: Back to Nature

A matching decompilation of *Harvest Moon: Back to Nature* (PlayStation, USA,
`SLUS_011.15`). The C source builds the EXE and its 13 overlays byte for byte and, given
your own copy of the disc, the whole disc image.

See the [VanillaBTN roadmap](ROADMAP.md) for the native port milestones.

## Building

```sh
docker build -t hmbtn-build .
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/src:ro -v /tmp/hmbtn-build:/build \
    -v /path/to/disc:/disc:ro hmbtn-build bash -c 'cmake -S /src -B /build -G Ninja \
    -DORIGINAL_DISC=/disc/disc.bin && cmake --build /build --target check'
```

`check` compares every output with the original's SHA-1. Without `-DORIGINAL_DISC` it
builds and checks only the 14 code binaries, which needs no game data.

### Windows and macOS (Docker Desktop)

Building straight from a bind mount fails there (GCC 2.95.2's 32-bit `cpp` cannot `stat`
files on the mount), and a checkout with CRLF line ends breaks it too. Use
`tools/docker_build.sh`, which builds from a private LF copy of the source inside the
container (`.gitattributes` keeps the checkout LF). PowerShell:

```powershell
docker run --rm -v "${PWD}:/src:ro" -v hmbtn-build-vol:/build `
    -v "C:\path\to\disc-dir:/disc:ro" -e "ORIGINAL_DISC=/disc/disc.bin" `
    -v "${PWD}\out:/out" hmbtn-build sh /src/tools/docker_build.sh
```

`ORIGINAL_DISC` is optional, and a mounted `/out` receives the built `disc.bin` and
`disc.cue`. Delete the `hmbtn-build-vol` volume (`docker volume rm`) to start from a clean
build.

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

[MIT](LICENSE), covering this repository's own work only; see [NOTICE](NOTICE) for the
upstream CC0 base and `tools/maspsx`, which keeps its own license. The game and the PSY-Q
SDK belong to their owners and are not included.
