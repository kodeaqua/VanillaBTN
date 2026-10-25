# Native build setup

## Matching build

Supported matching setup target: a separate Linux x86-64 host. The owner excludes
WSL from the current work; do not install, invoke, or require it.
The historical GCC archive contains Linux executables, not Windows programs.
Windows/macOS native matching support has not been established. This restriction
does not apply to the future native game executable.

Requirements derived from `Dockerfile`, `cmake/`, `tools/`, and the old workflow:

| Component | Required upstream selection | Source / verification |
| --- | --- | --- |
| GCC cpp/cc1 | 2.95.2 PSX, old-gcc release 0.17 | GitHub decompals/old-gcc; SHA-256 below |
| PSY-Q runtime | 4.7 | Owner-provided local zip; SHA-256 below |
| MIPS binutils | `mips-linux-gnu-{as,ld,objcopy}` | Host distribution package; upstream did not pin a version |
| Python | minimum 3.10; reference image used 3.14 | Standard library only; no pip packages found |
| CMake | minimum 3.20 matching, 3.24 port | Host distribution package; reference did not pin patch version |
| Ninja | reference distribution package | Upstream did not pin version |
| maspsx | vendored revision | `tools/maspsx/UPSTREAM` |
| curl, tar, sha256sum | host tools | Setup download, extraction, verification |
| objdiff-cli | 3.8.2, optional report only | Previous workflow; not needed by `check` |

Archive hashes, preserved from the reference Dockerfile:

```
932ed3669710a82b12570c29c46ff42989b6505b2af10db84d4551d55dfc0b1c  gcc-2.95.2-psx.tar.gz
e3df241adde663fba1f5bff1f0070e0d32d9518ebb11d60a914e5e016f704745  psyq-4.7.zip
```

On a provisioned Linux host, from the repository root:

```sh
PSYQ_ARCHIVE=/path/to/your/runtime-4.7.zip bash tools/setup-native.sh
cmake -S . -B build/match -G Ninja
cmake --build build/match --target check
```

Setup checks the local SDK archive and downloads only the checksum-pinned GCC
archive from the release URL in the script. It prepares ignored `sdk/` directories,
normalizes header names/line endings, and configures the standard matching build
directory with absolute tool paths. Repeating it re-verifies both inputs and
refreshes their contents. It does not install system packages, invoke sudo, or
download proprietary SDK files. A partial download is never used as a compiler.

An existing matching build directory configured for different tool paths must be
reconfigured by setup first. Do not put SDK archives in tracked directories.
The compatibility script `tools/install_host_sdk.sh` now forwards to native setup.
No container command is required.

`check` without ORIGINAL_DISC verifies 14 code binaries against
`disc/binaries.sha1`. Add `-DORIGINAL_DISC=/path/to/disc.bin` only for local disc
reconstruction. CI must never receive disc data.

The reference did not pin host package versions. A fully reproducible host-tool
manifest remains pending a successful matching baseline; no versions have been
invented here. Workflows/CI are excluded by owner instruction. Existing upstream
workflow files are unchanged and are not part of this setup.

## Port foundation

Presets live in the separate `port/` project. From that directory:

```sh
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
cmake --preset linux-asan-ubsan
cmake --build --preset linux-asan-ubsan
ctest --preset linux-asan-ubsan
```

Desktop presets also exist for `windows-release` (LLVM-mingw/UCRT, without MSVC)
and `macos-universal-release`.
These build the compatibility layer and four initial original source units.
Android presets await an NDK
pin and platform configuration; no Android support is claimed yet.

No SDL dependency, SDK, network, or game data is required by the port foundation.
It is not a playable game. Only `math_util.c`, `farm_data_size.c`, `horse_names.c`,
and `picture_codes.c` from `src/` are compiled so far.

## Windows first

From the repository root, run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1
```

The policy override applies only to this process, not to machine/user settings.
The script downloads checksum-pinned LLVM-mingw 20250709, CMake 3.31.6 and Ninja
1.12.1 into ignored `build/host-tools/`, verifies cached archives, then configures,
builds and tests `windows-release`. It does not require administrator access,
WSL, MSVC or the PSX SDK. Subsequent runs reuse local downloads. Network is
required for initial tool provisioning, never for the foundation tests themselves.

To run the preset commands individually in a PowerShell session that permits scripts:

```powershell
& ./port/packaging/windows/setup-toolchain.ps1
Set-Location port
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

For native Windows ASan/UBSan tests:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File port/packaging/windows/build.ps1 -Preset windows-asan-ubsan
```

The sanitizer preset uses a dynamic ASan runtime from the toolchain directory
added to PATH by setup; release builds retain static linkage. The wrapper uses
`cmake --fresh` so compiler upgrades cannot drop cached preset options. When
running CMake directly after upgrading a compiler, also use `--fresh` on configure.

The optional source audit requires any local Python 3.10+ interpreter, independently
of the port build. It does not need an SDK, a network connection, or game assets:

```powershell
python -m unittest discover -s port/analysis -p "test_*.py"
python port/analysis/source_audit.py --output build/port/source-audit.json
```

It reports lexical candidates, not a complete compiler/linker inventory. See
`ANALYSIS.md` for limitations. Python is not required by the Windows build wrapper.

## Validation on 2026-10-25

An earlier Ubuntu WSL launch failed (`HCS_E_HYPERV_NOT_INSTALLED`); the owner
subsequently excluded WSL, so no further WSL attempts are required. Matching
validation is deferred to a separate Linux host. CMake, Ninja and Clang were not found on the
Windows PATH initially. After provisioning project-local tools, Windows release
configure/build and all six tests passed. The Windows ASan/UBSan preset also
built and passed six tests with Clang 20.1.8; Clang 19's ASan had failed during
startup (ADR-005). The matching build, Linux/macOS builds and full game execution
remain unverified. Original source files have not been edited.
