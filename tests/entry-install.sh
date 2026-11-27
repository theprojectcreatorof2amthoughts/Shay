#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SHAII=${1:-"$ROOT/build/shaii"}
GSHAI=${2:-"$ROOT/build/gshai"}
SHENTRYPACK=${3:-"$ROOT/build/shentrypak"}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP/source/nested"
cat > "$TMP/source/userinstalled-source.shay" <<'EOF'
$entrysyn sayuser instxt
instxt/>"Packaged entry body executed"
EOF
"$SHENTRYPACK" "$TMP/source/userinstalled-source.shay" -o "$TMP/source/nested/userinstalled.shpak"
HOME="$TMP/home" make -C "$ROOT" insent "$TMP/source"
test -f "$TMP/home/.shay/entries/nested/userinstalled.shpak"
printf '%s\n' '$useentry userinstalled' 'sayuser/>"User entry loaded"' > "$TMP/main.shay"
out=$(HOME="$TMP/home" "$SHAII" "$TMP/main.shay")
[ "$out" = "Packaged entry body executed\nUser entry loaded" ]
HOME="$TMP/home" "$GSHAI" -o "$TMP/app" "$TMP/main.shay"
out=$("$TMP/app")
[ "$out" = "User entry loaded" ]
printf '%s\n' 'PASS: insent installs nested .shpak entries and executes packaged Shay source for gshai and shaii'

# $entrysyndef blocks compile into binary package records.
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
"$SHENTRYPACK" "$TMP/pattern-source.shay" -o "$TMP/home/.shay/entries/patternentry.shpak"
cat > "$TMP/pattern-entry-main.shay" <<'EOF'
$useentry patternentry
saypattern/>"Pattern entry package loaded"
EOF
out=$(HOME="$TMP/home" "$SHAII" "$TMP/pattern-entry-main.shay")
[ "$out" = "Pattern entry package loaded" ]
printf '%s\n' 'PASS: $entrysyndef blocks compile into and load from binary .shpak packages'

# The dedicated compiler reports exact source locations and does not emit a bad package.
cat > "$TMP/broken-entry.shay" <<'EOF'
$entrysyndef broken
    ::0xFF::
EOF
if "$SHENTRYPACK" "$TMP/broken-entry.shay" -o "$TMP/broken.shpak" 2>"$TMP/diagnostics"; then
    echo "expected shentrypak to reject an unterminated syntax definition" >&2
    exit 1
fi
grep -F "$TMP/broken-entry.shay:1:1: error: syntax definition 'broken' is missing its closing 'finish'" "$TMP/diagnostics" >/dev/null
test ! -e "$TMP/broken.shpak"
printf '%s\n' 'PASS: shentrypak reports source diagnostics and rejects invalid packages'

# gshai no longer owns entry package compilation.
if "$GSHAI" -et "$TMP/old.shpak" "$TMP/source/userinstalled-source.shay" >/dev/null 2>&1; then
    echo "expected gshai -et to be removed" >&2
    exit 1
fi
printf '%s\n' 'PASS: entry package compilation is separated from gshai'
