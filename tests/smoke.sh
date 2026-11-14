#!/bin/sh
set -eu
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
./shay examples/hello.shay > "$tmp/hello.out"
grep -Fx "Hello World!" "$tmp/hello.out"
./shay examples/variables.shay > "$tmp/variables.out"
grep -Fx "Hello from Shay" "$tmp/variables.out"
./shay examples/tasks.shay > "$tmp/tasks.out"
grep -Fx "Zeccurs task executed." "$tmp/tasks.out"
printf '%s\n' "Shay smoke tests passed."
