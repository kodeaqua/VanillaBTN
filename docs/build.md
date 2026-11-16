# Local Windows Build and Toolchain

> **Documentation only until implemented.** These are required build contracts and future commands, not claims that `Cargo.toml`, application code, or a working Windows executable already exist.

## Supported default target

- **Host:** developer's Windows machine.
- **Rust:** stable toolchain targeting `x86_64-pc-windows-msvc` by default.
- **Build tools:** Visual Studio **Desktop development with C++** / MSVC and Windows SDK; Clang is available to investigate but is not a mandatory replacement for MSVC.
- **GPU:** approved multi-backend `wgpu` (`dx12`, `vulkan`, `gles`/OpenGL, `metal`; D3D12 default on Windows) + WGSL shaders, hosted by `winit`; no emulated PS1 graphics hardware and no substitute non-`wgpu` renderer. See [`stack.md`](stack.md).
- **Audio/input:** approved `kira`/`cpal`, `gilrs` and `winit`, as required by their task gates. No third-party stacks without developer approval.
- **Docker:** optional for **local PS1 reference checks only** when actually needed or authorized; not part of the native Rust build/runtime. Do not alter the upstream Dockerfile.
- **CI:** **do not add/edit/run any CI**, even locally via `act` or workflow dispatch. Leave original hmbtn-decomp workflows intact.

## Inspect local Windows prerequisites (PowerShell)

```powershell
rustc --version
cargo --version
rustup show active-toolchain
rustup target list --installed
where.exe cl
where.exe clang
```

`cl.exe` may be discoverable only from **Developer PowerShell for Visual Studio** or a properly initialized MSVC environment. A missing `cl` in a normal shell does not prove C++ tools were not installed. Check actual build success rather than relying on a guessed environment.

## Build contract: Debug versus Release

| | Debug | Release |
| --- | --- | --- |
| Command once workspace exists | `cargo build` | `cargo build --release` |
| Cargo profile | `dev` (`opt-level = 0`, symbols, debug assertions) | `release` (optimized, debug assertions off) |
| Diagnostic code | Compiled and active | **Not compiled into the app** |
| Logging | Timestamped, leveled `VanillaBTN.log` | **No application logging**, log file, tracing/profiler/debug overlay |
| Correctness checks | Must remain active | Must remain active |

The locked stack/approval process in [`stack.md`](stack.md) applies to any Cargo dependency additions. Use Cargo's default profiles unless an explicit setting is needed for the required contract. Prefer `#[cfg(debug_assertions)]` to exclude Debug-only logging code at compile time; avoid merely setting the logger level to `off` in Release. If another Cargo feature or dependency can re-enable instrumentation, explicitly inspect and test it. Optimized Rust must still handle invalid assets and save failures correctly.

**Debug log location:** `%LOCALAPPDATA%\VanillaBTN\VanillaBTN.log` (created through an OS-appropriate per-user data directory). Never hardcode developer usernames or writable locations under the repository or `Program Files`. Log startup/profile/version, relevant errors and selected state transitions; avoid writing every frame. Bound growth and avoid credentials, user paths and save content.

## Local verification once executable exists

```powershell
cargo fmt --all -- --check
cargo test
cargo build
cargo build --release
```

Always format Rust edits with `cargo fmt` and verify `cargo fmt --all -- --check` before committing. Use focused Clippy and other relevant **local** checks when appropriate to the current milestone. Launch both resulting binaries on Windows and verify the behaviors in [`validation.md`](validation.md). Building successfully is **not** evidence of gameplay parity.

**Important:** the Debug log may already exist when checking Release. Its continued existence alone does not prove the Release build wrote to it. Check whether Release creates, opens, or changes it using an appropriately controlled local check and review compile-time feature gates. Never delete a developer's existing log without approval.

## Source, files and Git hygiene

- New Rust/TOML/Markdown and documentation are UTF-8 + LF. Keep existing upstream file endings unchanged, especially PS1 C/ASM/build artifacts. Avoid whole-file CRLF/LF diffs.
- Use `PathBuf` and environment-aware directories; support Windows path spaces, Unicode and case-insensitive path behavior.
- Existing upstream `.gitignore` must be **extended, not replaced**, if new generated artifacts need exclusions. Typical additions when relevant: `/target/`, `VanillaBTN.log`, local save directories, user-owned game data, and IDE caches. Inspect the current tracked ignore rules before changing them.
- Never commit `target` outputs, source game discs/extracted copyright data, SDK binaries, credentials, personal machine paths or logs.
- `.gitattributes` shipped with this documentation pack is **scoped**; do not normalize the decompiled reference tree. `git diff --check` before every commit.

## Original PS1 reference checks

The upstream repo documents a Docker-based CMake/Ninja reference build and optional original-disc hash comparison. These are **separate** from building the native Rust application, and any reference build is a local operation requiring appropriate assets and tools. Do not silently make Docker a prerequisite for native Windows builds and do not run or modify CI.
