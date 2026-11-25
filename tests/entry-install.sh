#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SHAII="$ROOT/build/shaii"
GSHAI="$ROOT/build/gshai"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP/source/nested"
printf '%s\n' '$entrysyn sayuser instxt' > "$TMP/source/userinstalled-source.shay"
"$GSHAI" -et "$TMP/source/nested/userinstalled.shaye" "$TMP/source/userinstalled-source.shay"
HOME="$TMP/home" make -C "$ROOT" insent "$TMP/source"
test -f "$TMP/home/.shay/entries/nested/userinstalled.shaye"
printf '%s\n' '$useentry userinstalled' 'sayuser/>"User entry loaded"' > "$TMP/main.shay"
out=$(HOME="$TMP/home" "$SHAII" "$TMP/main.shay")
[ "$out" = "User entry loaded" ]
HOME="$TMP/home" "$GSHAI" -o "$TMP/app" "$TMP/main.shay"
out=$("$TMP/app")
[ "$out" = "User entry loaded" ]
printf '%s\n' 'PASS: insent installs nested binary entries for gshai and shaii'

# $entrysyndef stores named syntax patterns in the binary package.
cat > "$TMP/pattern-source.shay" <<'EOF'
$entrysyn saypattern instxt
$entrysyndef hexnum
    ::0x0000::
    ::0xFFFF::
finish
$entrysyndef binnum
    ::0b0000::
    ::0b1111::
finish
EOF
"$GSHAI" -et "$TMP/home/.shay/entries/patternentry.shaye" "$TMP/pattern-source.shay"
cat > "$TMP/pattern-entry-main.shay" <<'EOF'
$useentry patternentry
saypattern/>"Pattern entry package loaded"
EOF
out=$(HOME="$TMP/home" "$SHAII" "$TMP/pattern-entry-main.shay")
[ "$out" = "Pattern entry package loaded" ]
printf '%s\n' 'PASS: $entrysyndef blocks compile into and load from binary .shaye packages'
