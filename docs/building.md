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

The executable currently opens an empty bringup window. The `native_startup`
CTest check launches it headlessly with `--check-build`; the developer checks
the window manually. This build does not yet load game data or run gameplay.
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
proprietary game data in the ignored `build/` tree. The current native
executable does not load them yet. Its runtime build needs no Python.

## PS1 reference build

These commands build PS1 binaries, even when Docker runs on Windows. A native
Windows game is not available yet. The PS1 reference build requires Docker and
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
