# Third-party dependencies

The native port foundation adds no third-party runtime dependencies.
SDL3 and a renderer have not yet been integrated or selected by spike.

Windows host tools (downloaded to ignored `build/host-tools/`, not vendored):

| Tool | Version | License | Source |
| --- | --- | --- | --- |
| LLVM-mingw | 20250709, Clang 20.1.8 | LLVM Apache-2.0 with LLVM exceptions; mingw-w64 permissive component notices | https://github.com/mstorsjo/llvm-mingw/releases/tag/20250709 |
| CMake | 3.31.6 | BSD-3-Clause | https://github.com/Kitware/CMake/releases/tag/v3.31.6 |
| Ninja | 1.12.1 | Apache-2.0 | https://github.com/ninja-build/ninja/releases/tag/v1.12.1 |

Archive SHA-256 pins are in `port/packaging/windows/setup-toolchain.ps1`.
CMake was checked against its published SHA-256 manifest. LLVM-mingw and Ninja
pins were computed from the official GitHub release downloads, not independently
signed publisher manifests. Tool packages retain their own third-party notices.
Release test executables import Windows system UCRT/API DLLs. Sanitizer tests
also load the toolchain's ASan runtime DLL from PATH; no tool package or
third-party runtime DLL is redistributed by this change. Distribution notice
review remains required when packaging the actual game.

Matching-only tools remain separate: see `NATIVE_BUILD.md` and
`tools/maspsx/LICENSE`. Proprietary SDK headers/libraries are local build inputs,
never port code or distribution content. No third-party PSX implementation has
been imported or approved.
