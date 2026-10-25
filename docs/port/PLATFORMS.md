# Platform status

As of 2026-10-25, no native game platform is brought up.

| Target | Foundation preset | Validation |
| --- | --- | --- |
| Linux x86-64 | linux-release, linux-asan-ubsan | Deferred; requires a separate Linux host, no WSL |
| Windows x86-64 | windows-release | Ten original source units compile; local tests passed (18/18) |
| Windows x86-64 sanitizers | windows-asan-ubsan | Clang 20.1.8 ASan/UBSan tests passed (18/18); coverage limited to the initial subset |
| macOS universal | macos-universal-release | Pending; no macOS host tested |
| Android arm64 | Pending NDK and minSdk selection | No device/build validation |

The owner prioritizes Windows. No workflow/CI changes are included or requested.
Windows validation covers the initial source subset, not full game execution.
The full-tree syntax probe passes 65 of 113 files and fails 48, with 1643 emitted
warnings before its per-file error cap. This is diagnostic inventory, not a port build.
Entry-point and A_FILE objects are in the native archive; their full boot flow,
callbacks, disc IO and overlay behavior have not been executed. The original
frame/farm setup and region helpers are now tested using native data storage.
Normal and larger-work-area layouts, pointer bounds, input-demo preservation,
GS work allocation boundaries and signed free-space budgets pass under ASan/UBSan.
The test observes GPU/GS setup calls; it does not implement or validate OT links.
CD position helpers are tested over all 450000 valid two-digit-minute BCD
positions; disc operations remain explicit abort stubs. No disc was required.
Six lexical-audit regression fixtures also passed using a local Python 3.11
interpreter. Clang 19 ASan failed during startup; the pinned Clang 20 upgrade
resolved that startup failure (ADR-005). No performance measurements were taken.
The initial budgets remain targets: 60 Hz game ticks, port overhead below 2 ms
desktop / 4 ms mid-range Android, and desktop cold start below 3 seconds after
initial cache creation. No measurements exist yet. LTO, CPU baselines, stripping,
static Windows CRT and packaging must be validated when a game target is added.
