# Platform status

As of 2026-10-25, no native game platform is brought up.

| Target | Foundation preset | Validation |
| --- | --- | --- |
| Linux x86-64 | linux-release, linux-asan-ubsan | Deferred; requires a separate Linux host, no WSL |
| Windows x86-64 | windows-release | Local configure/build and compat.types passed (1/1) |
| macOS universal | macos-universal-release | Pending; no macOS host tested |
| Android arm64 | Pending NDK and minSdk selection | No device/build validation |

The owner prioritizes Windows. No workflow/CI changes are included or requested.
Windows validation covers only the type foundation, not the game or sanitizers.
The initial budgets remain targets: 60 Hz game ticks, port overhead below 2 ms
desktop / 4 ms mid-range Android, and desktop cold start below 3 seconds after
initial cache creation. No measurements exist yet. LTO, CPU baselines, stripping,
static Windows CRT and packaging must be validated when a game target is added.
