# Platform status

As of 2026-10-25, no native game platform is brought up.

| Target | Foundation preset | Validation |
| --- | --- | --- |
| Linux x86-64 | linux-release, linux-asan-ubsan | Deferred; requires a separate Linux host, no WSL |
| Windows x86-64 | windows-release | Eight original source units compile; local tests passed (17/17) |
| Windows x86-64 sanitizers | windows-asan-ubsan | Clang 20.1.8 ASan/UBSan tests passed (17/17); coverage limited to the initial subset |
| macOS universal | macos-universal-release | Pending; no macOS host tested |
| Android arm64 | Pending NDK and minSdk selection | No device/build validation |

The owner prioritizes Windows. No workflow/CI changes are included or requested.
Windows validation covers the initial source subset, not full game execution.
The full-tree syntax probe passes 65 of 113 files and fails 48, with 1647 emitted
warnings before its per-file error cap. This is diagnostic inventory, not a port build.
Entry-point and A_FILE objects are now in the native archive, but their hardware
addresses, callbacks, disc IO and overlay behavior have not been executed.
CD position helpers are tested over all 450000 valid two-digit-minute BCD
positions; disc operations remain explicit abort stubs. No disc was required.
Six lexical-audit regression fixtures also passed using a local Python 3.11
interpreter. Clang 19 ASan failed during startup; the pinned Clang 20 upgrade
resolved that startup failure (ADR-005). No performance measurements were taken.
The initial budgets remain targets: 60 Hz game ticks, port overhead below 2 ms
desktop / 4 ms mid-range Android, and desktop cold start below 3 seconds after
initial cache creation. No measurements exist yet. LTO, CPU baselines, stripping,
static Windows CRT and packaging must be validated when a game target is added.
