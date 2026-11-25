#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SHAII="$ROOT/build/shaii"
GSHAI="$ROOT/build/gshai"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP/source/nested"
printf '%s\n' 'SHAYE/1' 'instxt/>"User entry loaded"' > "$TMP/source/nested/userinstalled.shaye"
HOME="$TMP/home" make -C "$ROOT" insent "$TMP/source"
test -f "$TMP/home/.shay/entries/nested/userinstalled.shaye"
printf '%s\n' '$useentry userinstalled' > "$TMP/main.shay"
out=$(HOME="$TMP/home" "$SHAII" "$TMP/main.shay")
[ "$out" = "User entry loaded" ]
HOME="$TMP/home" "$GSHAI" -o "$TMP/app" "$TMP/main.shay"
out=$("$TMP/app")
[ "$out" = "User entry loaded" ]
printf '%s\n' 'PASS: insent installs nested entries for gshai and shaii'
