#!/usr/bin/env sh
set -eu
for tool in git cmake ninja cc; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf 'Missing required prerequisite: %s\n' "$tool" >&2
    exit 1
  fi
done
git submodule update --init --recursive
cmake -S . -B build/linux/x86_64/debug/headless -G Ninja -DCMAKE_BUILD_TYPE=Debug
