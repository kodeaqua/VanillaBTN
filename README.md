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

## Native Windows x64 startup build

From a Visual Studio x64 developer shell with CMake 3.21 or newer and Ninja, run:

```powershell
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug
ctest --preset windows-x64-debug
```

Use `windows-x64-release` in the same commands for Release. The executable is under
`build/native/<preset>/native/`. Debug writes `VanillaBTN.log` beside it and uses
AddressSanitizer when available. Release builds with link-time optimization.

To use the Visual Studio LLVM tools, configure `clang` in a separate build directory:

```powershell
cmake --preset windows-x64-debug -B build/native/llvm-debug -DCMAKE_C_COMPILER=clang
cmake --build build/native/llvm-debug
ctest --test-dir build/native/llvm-debug --output-on-failure
```

For LLVM Release, use the Release preset and `build/native/llvm-release`.
LLVM Debug enables AddressSanitizer and UndefinedBehaviorSanitizer when available.

### Build and run the game locally

Point CMake at a local PsyZ checkout at revision `00fb42e` with its SDL submodule
initialized. Configure from a Windows x64 developer shell with Clang.

```powershell
cmake --preset windows-x64-release -B build/native/game-release -DCMAKE_C_COMPILER=clang "-DVANILLABTN_PSYZ_SOURCE_DIR=path/to/psyz"
cmake --build build/native/game-release
ctest --test-dir build/native/game-release --output-on-failure
& .\build\native\game-release\native\vanillabtn.exe --disc 'path\to\disc.cue'
```

Use `windows-x64-debug` and a separate build directory for Debug. The build links
the game and overlays into `vanillabtn.exe`. `--disc` accepts the USA BIN or CUE
and verifies it using Python 3.10 or newer on `PATH`. PsyZ stores virtual cards
under `%LOCALAPPDATA%\VanillaBTN`.
[Transfer diaries with DuckStation](#transfer-diaries-with-duckstation)

In Debug builds only, pass `--debug-menu` with `--disc` to start in `08_DEBUG`
(minigame/scene jump menu and `30_GF_swind` settings window):

```powershell
& .\build\native\game-debug\native\vanillabtn.exe --disc 'path\to\disc.cue' --debug-menu
```

Use the D-pad to move and Circle or Start to open an entry. House, minigame,
save/load, trade and farm-return entries start from a fresh farm session.
PocketStation tests report that the device is unavailable on Windows.

### Transfer diaries with DuckStation

- Use the USA version (`SLUS-01115`). Close both games before changing card files.
- VanillaBTN stores one 32 KiB file per diary in `%LOCALAPPDATA%\VanillaBTN\bu00`
  (card 1) or `bu10` (card 2). `BASLUS-01115-FARM0` through
  `BASLUS-01115-FARM2` are diaries 1 through 3.
- DuckStation stores a full card as `.mcd` and exports individual diaries as
  `.mcs`. An `.mcs` export adds a 128-byte directory record before the diary.
  [DuckStation memory card format](https://github.com/stenzek/duckstation/blob/master/src/core/memory_card_image.cpp)

**VanillaBTN to DuckStation**

1. Right-click the game in DuckStation and select **Edit Memory Cards…**.
2. On the destination card, choose **Import… → Import Single Save File…** and
   select **All Files (*.*)**. Import the extensionless diary with its original name.
3. Click **Save**, then start the game and choose **Continue**.

**DuckStation to VanillaBTN**

1. In the memory card editor, export the diary as an `.mcs` file to a folder
   outside `bu00` and `bu10`.
2. Run this in PowerShell with the export's path. It writes a new raw diary beside
   the export, using the slot name stored in its directory record.

```powershell
$export = Get-Item -LiteralPath '.\diary.mcs' -ErrorAction Stop
$bytes = [IO.File]::ReadAllBytes($export.FullName)
if ($bytes.Length -ne 32896 -or $bytes[128] -ne 0x53 -or $bytes[129] -ne 0x43) {
    throw 'Expected a 32 KiB diary in a DuckStation MCS export.'
}
$slot = [Text.Encoding]::ASCII.GetString($bytes, 10, 20).Split([char]0)[0]
if ($slot -cnotmatch '^BASLUS-01115-FARM[0-2]$') {
    throw 'Expected a Harvest Moon USA diary slot.'
}
$raw = Join-Path $export.DirectoryName $slot
$out = [IO.File]::Open($raw, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
try {
    $out.Write($bytes, 128, 32768)
} finally {
    $out.Dispose()
}
```

3. If the destination slot exists, move its file to a backup folder outside
   the card directories. Copy the new diary into `bu00` or `bu10` and choose
   **Continue** in VanillaBTN. Keep the file extensionless and preserve its slot name.

### Import a reference disc locally

With Python 3.10 or newer on `PATH`, run the native executable with your BIN or CUE:

```powershell
& .\build\native\windows-x64-release\native\vanillabtn.exe --import-disc 'path\to\disc.cue'
```

The bundled importer checks the USA reference SHA-1 before writing to
`%LOCALAPPDATA%\VanillaBTN\disc`. To select another output directory, use
`python tools/psx_disc.py import path/to/disc.cue --out path/to/local-data`.

A CUE must describe one `MODE2/2352` track starting at `00:00:00`. Keep the BIN
with the CUE, and choose an output directory outside the checkout and save directory.

A fresh import publishes the output directory after extraction succeeds. A repeat
import stages files first and preserves unrelated files in the destination.

### Stage a Windows startup package

After building Release, install into an empty directory under `build/`:

```powershell
cmake --install build/native/windows-x64-release --prefix build/package/VanillaBTN
```

Distribute the installed directory as a unit. It contains the startup executable,
importer, checksum manifest, and notices. The executable requires the Visual C++
x64 runtime. Disc import also requires Python 3.10 or newer on `PATH`.

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
