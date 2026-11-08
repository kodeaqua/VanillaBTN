# Building on Windows and checking the PS1 reference

## Native Windows bringup

Install Visual Studio with Desktop development with C++. From the repository
root, run in an ordinary PowerShell session:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-windows.ps1 -Configuration Debug
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-windows.ps1 -Configuration Release
```

The script locates Visual Studio's x64 compiler, CMake, and Ninja, then
configures, builds, and runs CTest for each profile. It downloads the pinned
[SDL3 3.4.18 release](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18)
on first configure and checks its SHA-256. The output is
`build/native/<profile>/vanillabtn.exe`; the build tree is ignored by Git.
No WSL, MSYS, Docker, or Python is needed for this native target.

The executable runs original startup and scene handlers using locally imported
assets. The `native_startup` CTest checks executable linkage with `--check-build`;
other tests check SDK and rendering behavior with synthetic fixtures. Actual
visual, audio and gameplay acceptance remains with the developer.
To inspect the Debug window, run `& .\build\native\Debug\vanillabtn.exe` and
close it with the window's close button.

## Importing game data on Windows

For development, provide your own USA `SLUS_011.15` disc image in raw
MODE2/2352 `.bin` format. Python 3 is needed for this import step only:

```powershell
$disc = Join-Path $env:USERPROFILE 'Downloads\your-disc.bin'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/import-game.ps1 `
  -Disc $disc
```

The importer writes extracted disc files to `build/game-data/disc/` and 32
archive members to `build/game-data/members/`. Those files are local,
proprietary game data in the ignored `build/` tree. Run from the repository root after importing:

```powershell
& .\build\native\Debug\vanillabtn.exe --data .\build\game-data\disc
```

The host mounts this disc tree and runs the original startup and scene
handlers. Its runtime needs no Python. The native port remains incomplete;
full farm, save/load and audiovisual fidelity require further work.

For an optional code-level integration check using your local disc data:

```powershell
& .\build\native\Debug\original_boot_test.exe --data .\build\game-data\disc
```

This separate probe uses dummy SDL devices, a synthetic clock, in-memory cards
and a finite 900-vblank budget. It checks GPU status and counts colored frames
and nonzero PCM intervals without displaying or judging gameplay. It is not
part of CTest because the normal suite requires no proprietary data. Diagnostics
belong to this test executable, not the Release game executable.

Add `--demo` to run 5,000 vblanks, reaching the original title attract demo.
At the budget boundary it writes the selected framebuffer to the ignored
`build/native-demo.bmp`. This is a diagnostic capture, not an image comparison
or proof of gameplay fidelity. On a crash, the probe reports original state
counters and symbol names without dumping asset data or source paths. The
probe uses in-memory cards and never writes your card files.

Use `--cycle` instead of `--demo` for a 10,000-vblank run. It additionally
requires the original input replay to reach the farm and return to title;
a missed transition fails the probe. This checks scene execution, not whether
the recording matches the original console's visuals or movement.

To build a separate local candidate while a game window is open:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-windows.ps1 `
  -Configuration Debug -ExecutableName vanillabtn-candidate
```

This produces `build/native/Debug/vanillabtn-candidate.exe`. Omitting the name
selects the standard `vanillabtn.exe` output again.

## PS1 reference build

These commands build PS1 binaries, even when Docker runs on Windows. The incomplete native
Windows runtime is built separately by the command above. The PS1 reference build requires Docker and
produces the EXE and 13 overlays from source.
With your own USA `SLUS_011.15` disc image in MODE2/2352 `.bin` format, it also
rebuilds `A_FILE` and a `.bin`/`.cue` disc image. The original game data and
PSY-Q SDK are not included in this repository.

## Linux or WSL with a Linux filesystem checkout

From the repository root:

```sh
docker build -t vanillabtn-build .
mkdir -p /tmp/vanillabtn-build
docker run --rm --user "$(id -u):$(id -g)" \
  -v "$PWD":/src:ro -v /tmp/vanillabtn-build:/build \
  vanillabtn-build bash -c 'cmake -S /src -B /build -G Ninja && cmake --build /build --target check'
```

For a full disc build, mount the folder containing your `.bin` and add
`-DORIGINAL_DISC`:

```sh
mkdir -p /tmp/vanillabtn-build
docker run --rm --user "$(id -u):$(id -g)" \
  -v "$PWD":/src:ro -v /tmp/vanillabtn-build:/build \
  -v /path/to/disc-folder:/disc:ro \
  vanillabtn-build bash -c 'cmake -S /src -B /build -G Ninja -DORIGINAL_DISC=/disc/disc.bin && cmake --build /build --target check'
```

The outputs in these examples are under `/tmp/vanillabtn-build` on the host.
Choose a different build directory if switching between code-only and full
disc builds.

## Windows PowerShell with Docker Desktop

Run these commands from the repository root. The helper copies source files
to the container's Linux filesystem, converts CRLF to LF there, builds, runs
`check`, and copies verified outputs to the gitignored `build/` directory.
The source checkout is not rewritten.

```powershell
docker build -t vanillabtn-build .
docker run --rm -v "${PWD}:/input" vanillabtn-build bash /input/tools/build-in-container.sh
```

For the full disc build, set the folder and exact `.bin` filename to yours:

```powershell
$discFolder = 'C:\path\to\disc-folder'
$discFile = 'disc.bin'
docker run --rm -v "${PWD}:/input" -v "${discFolder}:/disc:ro" vanillabtn-build `
  bash /input/tools/build-in-container.sh "/disc/$discFile"
```

For example, a filename with spaces is accepted when `$discFile` contains
the full name. Open `build/disc.cue` in an emulator after the full build.

Do not run the old `cpp` directly against a Windows bind mount: it may report
`Value too large for defined data type`. A Windows checkout with CRLF can also
break multi-line C macros. The helper handles both in the temporary copy.

## What a passing check means

The code-only target checks 14 files against `disc/binaries.sha1`. With
`ORIGINAL_DISC`, it checks 17 files, adding `A_FILE.BIN`, `A_FILE.HDT`, and
`disc.bin` from `disc/disc.sha1`. A nonzero exit means at least one file failed
to build or did not match the reference SHA-1. `disc.cue` is generated alongside
`disc.bin` but is not in the SHA-1 lists.
