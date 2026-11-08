#!/bin/bash
set -euo pipefail

if (( $# > 1 )); then
    echo "usage: bash /input/tools/build-in-container.sh [original-disc.bin]" >&2
    exit 2
fi

mkdir -p /work
cp -a /input/CMakeLists.txt /input/cmake /input/disc /input/sdk /input/src /input/tools /work/

# GCC 2.95's preprocessor cannot reliably read Windows bind mounts or CRLF
# continuation lines. Normalize only the temporary source copy.
python3 - <<'PY'
from pathlib import Path

for path in Path('/work/src').rglob('*'):
    if path.suffix in ('.c', '.h'):
        data = path.read_bytes()
        if b'\r\n' in data:
            path.write_bytes(data.replace(b'\r\n', b'\n'))
PY

cmake_args=()
if (( $# == 1 )); then
    cmake_args+=("-DORIGINAL_DISC=$1")
fi
cmake -S /work -B /work/build -G Ninja "${cmake_args[@]}"
cmake --build /work/build --target check --parallel 4

mkdir -p /input/build
while read -r hash name; do
    name=${name%$'\r'}
    cp "/work/build/$name" "/input/build/$name"
done < /work/disc/binaries.sha1
if (( $# == 1 )); then
    while read -r hash name; do
        name=${name%$'\r'}
        cp "/work/build/$name" "/input/build/$name"
    done < /work/disc/disc.sha1
    cp /work/build/disc.cue /input/build/disc.cue
fi
