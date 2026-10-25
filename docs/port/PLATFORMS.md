# Platform status

As of 2026-10-25, no native game platform is brought up.

| Target | Foundation preset | Validation |
| --- | --- | --- |
| Linux x86-64 | linux-release, linux-asan-ubsan | Deferred; requires a separate Linux host, no WSL |
| Windows x86-64 | windows-release | Eleven original source units compile; local tests passed (25/25) |
| Windows x86-64 sanitizers | windows-asan-ubsan | Clang 20.1.8 ASan/UBSan tests passed (25/25); coverage limited to the initial subset |
| macOS universal | macos-universal-release | Pending; no macOS host tested |
| Android arm64 | Pending NDK and minSdk selection | No device/build validation |

The owner prioritizes Windows. No workflow/CI changes are included or requested.
Windows validation covers the initial source subset, not full game execution.
The full-tree syntax probe passes 66 of 113 files and fails 47, with 1650 emitted
warnings before its per-file error cap. This is diagnostic inventory, not a port build.
Entry-point and A_FILE objects are in the native archive; their full boot flow,
callbacks, disc IO and overlay behavior have not been executed. The original
frame/farm setup and region helpers are now tested using native data storage.
Normal and larger-work-area layouts, pointer bounds, input-demo preservation,
GS work allocation boundaries and signed free-space budgets pass under ASan/UBSan.
The workspace test observes GPU/GS setup calls. A separate gpu.links test now
uses actual reverse OT initialization and the guarded original insertion/builders
to validate pointer handles, packet order/lengths, cursor movement and re-clearing.
Malformed handles and registry exhaustion fail explicitly. No rasterization or
DrawOTag execution is covered.
CD position helpers are tested over all 450000 valid two-digit-minute BCD
positions; disc operations remain explicit abort stubs. No disc was required.
Six lexical-audit regression fixtures also passed using a local Python 3.11
interpreter. Clang 19 ASan failed during startup; the pinned Clang 20 upgrade
resolved that startup failure (ADR-005).
The initial budgets remain targets: 60 Hz game ticks, port overhead below 2 ms
desktop / 4 ms mid-range Android, and desktop cold start below 3 seconds after
initial cache creation. Full-game measurements do not exist yet. LTO, CPU baselines, stripping,
static Windows CRT and packaging must be validated when a game target is added.

## Synthetic OT construction measurement

Date: 2026-10-25. Host: AMD Ryzen 5 4500 6-Core Processor, Windows x86-64.
Tool: `hmbtn_ot_bench` using C clock(), Clang 20.1.8 windows-release (-O2,
x86-64-v2). No sanitizer, LTO or renderer is involved. Addresses are registered
once before timing; each of 100000 repetitions clears a 2048-word OT and inserts
1000 persistent three-word packets. The code performs no per-iteration allocation.

Three runs: 799, 799, 798 ms total, or 0.007990, 0.007990, 0.007980 ms per
construction (approximately 8 microseconds). This is a warm, repeated CPU tag
workload measured with a coarse clock on a non-isolated host, not game frame
time. Payload generation, chain submission, rendering, GTE, audio and platform
work are outside the measurement. There is no valid previous native rendering
baseline for a before/after comparison; no optimization or frame-budget claim
is made. Full playthrough/device profiling is still needed to validate the
65536-live-tag registry capacity and its actual game cost.
