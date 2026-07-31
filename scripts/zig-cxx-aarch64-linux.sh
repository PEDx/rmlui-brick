#!/bin/sh
set -eu

export ZIG_GLOBAL_CACHE_DIR=${ZIG_GLOBAL_CACHE_DIR:-/tmp/trimui-zig-cache}
exec zig c++ -target aarch64-linux-gnu "$@"
