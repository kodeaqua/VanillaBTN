#!/bin/sh
# Build and check everything inside the hmbtn-build image, from a private copy of the
# source. Use this instead of building straight from a bind mount on Docker Desktop
# (Windows/macOS): GCC 2.95.2's 32-bit cpp cannot stat files on such mounts
# ("Value too large for defined data type").
#
#   docker run --rm -v "$PWD":/src:ro -v hmbtn-build-vol:/build \
#       [-v /path/to/disc-dir:/disc:ro -e ORIGINAL_DISC=/disc/disc.bin] \
#       [-v /path/to/out:/out] hmbtn-build sh /src/tools/docker_build.sh
#
# ORIGINAL_DISC  the original disc's .bin (optional; without it only the 14 code
#                binaries are built and checked)
# /out           if mounted, disc.bin and disc.cue are copied there
set -eu
work=/tmp/hmbtn-src
rm -rf "$work"
mkdir -p "$work"
tar -C /src --exclude=.git --exclude=build --exclude=sdk/psyq-4.7 --exclude=sdk/include \
    --exclude=sdk/gcc-2.95.2 --exclude=sdk/bin -cf - . | tar -C "$work" -xf -
# A checkout with CRLF line ends (Git for Windows, core.autocrlf=true) breaks GCC 2.95.2.
grep -rIl "$(printf '\r')" "$work" | xargs -r sed -i 's/\r$//'
cmake -S "$work" -B /build -G Ninja ${ORIGINAL_DISC:+"-DORIGINAL_DISC=$ORIGINAL_DISC"}
cmake --build /build --target check
if [ -d /out ] && [ -f /build/disc.bin ]; then
    cp /build/disc.bin /build/disc.cue /out/
    echo "copied disc.bin and disc.cue to /out"
fi
