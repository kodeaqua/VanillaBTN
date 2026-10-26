#!/bin/sh
# Compatibility entry point; setup now runs entirely on the Linux host.
set -eu
exec bash "$(dirname "$0")/setup-native.sh" "$@"
