# Building and verification

## Prerequisites

Use Docker with a running Linux-container engine. The root Dockerfile supplies
Python, CMake, Ninja, GCC 2.95.2 for PSX, MIPS binutils, and PSY-Q 4.7. Toolchain
and SDK downloads have SHA-256 checks in the Dockerfile.

Build the image from the repository root:

```sh
docker build -t hmbtn-build .
```

## Linux: verify the code binaries

```sh
docker run --rm --user "$(id -u):$(id -g)" \
  -v "$PWD":/src:ro -v /tmp/hmbtn-build:/build \
  hmbtn-build bash -c 'cmake -S /src -B /build -G Ninja && cmake --build /build --target check'
```

Expected result: `all 14 files match the original`. This covers `SLUS_011.15` and
13 overlays, using `disc/binaries.sha1` as the reference. No disc data is required.

## Windows PowerShell: build from a disposable Linux copy

Local verification encountered two compatibility issues: the old preprocessor
reported `Value too large for defined data type` when reading the Windows bind
mount, and CRLF line endings caused `stray '\\' in program` errors in multiline
macros after copying. Building from Linux storage with LF line endings passed.

Run the following from the repository root after building the Docker image.
It copies only build inputs, normalizes line endings in that disposable copy,
and saves the log and successful binary outputs under ignored `build/`.

```powershell
New-Item -ItemType Directory -Force build | Out-Null
$repoPath = (Get-Location).Path
$buildPath = Join-Path $repoPath 'build'
docker run --rm --mount "type=bind,source=$repoPath,target=/input,readonly" --mount "type=bind,source=$buildPath,target=/output" hmbtn-build bash -c 'mkdir -p /tmp/hmbtn-src && cp -r /input/src /input/cmake /input/tools /input/sdk /input/disc /input/CMakeLists.txt /tmp/hmbtn-src/ && find /tmp/hmbtn-src -type f -exec sed -i s/\\r$// {} + && cmake -S /tmp/hmbtn-src -B /tmp/hmbtn-build -G Ninja && cmake --build /tmp/hmbtn-build --target check > /output/verification.log 2>&1; result=$?; tail -n 30 /output/verification.log; if [ "$result" -eq 0 ]; then cp /tmp/hmbtn-build/SLUS_011.15 /tmp/hmbtn-build/*.bin /output/; fi; exit "$result"'
```

The container is removed when it exits, including the temporary source and build
directories. The repository source is mounted read-only. Configuration errors
appear in the terminal; compilation and matching results go to
`build/verification.log`. Treat the Docker exit code as the result; old output
binaries can remain from an earlier successful run.

If Docker cannot connect to `dockerDesktopLinuxEngine`, start Docker Desktop and
wait for its Linux engine to become ready. `docker info` checks engine availability.

## Full disc build

Use your own original USA disc image, in MODE2/2352 `.bin` format, and follow the
mount example in [README.md](../README.md). Set
`-DORIGINAL_DISC=/disc/disc.bin` during CMake configuration. This adds extraction,
archive packing, disc generation, and archive/disc hash checks. Do not add original
disc data or extracted assets to version control.

## Interpreting results

- `OK`: the output's SHA-1 matches the checked-in reference.
- `FAILED`: compilation completed, but the output differs from the reference.
- `MISSING`: an expected output is absent.
- A compiler/linker error means matching verification has not completed.

Do not replace reference hashes to hide a failure. For intentional game mods,
original hash mismatches are expected and must be reported separately from build
success and runtime testing.

On 2026-11-07, the Windows Linux-copy procedure completed all 116 Ninja steps and
matched all 14 code binaries. Full disc generation and emulator gameplay were
not tested. This records that verification run, not a guarantee for later changes.

CI in `.github/workflows/build.yml` builds and checks the code binaries on Ubuntu
and generates an objdiff progress report.
