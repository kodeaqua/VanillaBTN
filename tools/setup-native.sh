#!/usr/bin/env bash
# Prepare the matching build without containers. SDK input is owner-supplied.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
    echo 'Matching setup requires Linux x86-64 (or a working Ubuntu WSL distribution).' >&2
    exit 1
fi
for tool in python3 cmake ninja curl sha256sum tar mips-linux-gnu-as mips-linux-gnu-ld mips-linux-gnu-objcopy; do
    command -v "$tool" >/dev/null || { echo "Missing host tool: $tool" >&2; exit 1; }
done
sdk_archive=${PSYQ_ARCHIVE:-}
if [[ ! -f $sdk_archive ]]; then
    echo 'Set PSYQ_ARCHIVE to your local Runtime Library 4.7 zip (see docs/port/NATIVE_BUILD.md).' >&2
    exit 1
fi
printf '%s  %s\n' e3df241adde663fba1f5bff1f0070e0d32d9518ebb11d60a914e5e016f704745 "$sdk_archive" | sha256sum -c -
cache="$root/build/native-downloads"
mkdir -p "$cache"
gcc_archive="$cache/gcc-2.95.2-psx.tar.gz"
gcc_sha=932ed3669710a82b12570c29c46ff42989b6505b2af10db84d4551d55dfc0b1c
if [[ ! -f $gcc_archive ]]; then
    curl -fL --retry 3 -o "$gcc_archive.part" \
        https://github.com/decompals/old-gcc/releases/download/0.17/gcc-2.95.2-psx.tar.gz
    printf '%s  %s\n' "$gcc_sha" "$gcc_archive.part" | sha256sum -c -
    mv -- "$gcc_archive.part" "$gcc_archive"
fi
printf '%s  %s\n' "$gcc_sha" "$gcc_archive" | sha256sum -c -
mkdir -p "$root/sdk/gcc-2.95.2" "$root/sdk/psyq-4.7" "$root/sdk/include"
tar -xzf "$gcc_archive" -C "$root/sdk/gcc-2.95.2"
python3 - "$sdk_archive" "$root/sdk" <<'PY'
from pathlib import Path
import sys
import zipfile

archive, destination = sys.argv[1], Path(sys.argv[2])
with zipfile.ZipFile(archive) as sdk:
    sdk.extractall(destination / "psyq-4.7")
for header in (destination / "psyq-4.7" / "INCLUDE").rglob("*"):
    if header.is_file():
        output = destination / "include" / str(header.relative_to(destination / "psyq-4.7" / "INCLUDE")).lower()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(header.read_bytes().replace(b"\r", b""))
PY
chmod u+x "$root/sdk/gcc-2.95.2/cpp" "$root/sdk/gcc-2.95.2/cc1"
cmake -S "$root" -B "$root/build/match" -G Ninja \
    -DPSX_GCC_DIR="$root/sdk/gcc-2.95.2" \
    -DPSYQ_SDK_DIR="$root/sdk/psyq-4.7" \
    -DPSYQ_INCLUDE_DIR="$root/sdk/include"
echo 'Ready: cmake --build build/match --target check'
