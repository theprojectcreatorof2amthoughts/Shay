#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SHAII=${1:-"$ROOT/build/shaii"}
GSHAI=${2:-"$ROOT/build/gshai"}
SHENTRYPACK=${3:-"$ROOT/build/shentrypak"}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP/source/nested"
 : > "$TMP/source/nested/main.shaii"
cat > "$TMP/source/nested/userinstalled-source.shay" <<'EOF'
$shaiiport "main.shaii"
$entrysyn sayuser instxt
instxt/>"Packaged entry body executed"
EOF
"$SHENTRYPACK" "$TMP/source/nested/userinstalled-source.shay" -o "$TMP/source/nested/userinstalled.shpak"
HOME="$TMP/home" make -C "$ROOT" insent "$TMP/source"
test -f "$TMP/home/.shay/entries/nested/userinstalled.shpak"
rm -f "$TMP/home/.shay/entries/nested/main.shaii"
test ! -e "$TMP/home/.shay/entries/nested/main.shaii"
printf '%s\n' '$useentry userinstalled' 'sayuser/>"User entry loaded"' > "$TMP/main.shay"
out=$(HOME="$TMP/home" "$SHAII" "$TMP/main.shay")
expected=$(printf '%s\n%s' "Packaged entry body executed" "User entry loaded")
[ "$out" = "$expected" ]
HOME="$TMP/home" "$GSHAI" -o "$TMP/app" "$TMP/main.shay"
out=$("$TMP/app")
[ "$out" = "$expected" ]
printf '%s\n' 'PASS: insent installs nested .shpak entries and executes packaged Shay source for gshai and shaii'

# Optional decorations on $entrysyndef syntax names compile into binary package records.
cat > "$TMP/pattern-source.shay" <<'EOF'
$entrysyn saypattern instxt
$entrysyndef dobase
    ::0x0000::
    ::0xFFFF::
finish
$entrysyndef $binnum
    ::0b0000::
    ::0b1111::
finish
$entrysyndef octnum()
    ::0o0000::
    ::0o7777::
finish
$entrysyndef $hexnum()
    ::0x0000::
    ::0xFFFF::
finish
EOF
"$SHENTRYPACK" "$TMP/pattern-source.shay" -o "$TMP/home/.shay/entries/patternentry.shpak"
cat > "$TMP/pattern-entry-main.shay" <<'EOF'
$useentry patternentry
saypattern/>"Pattern entry package loaded"
EOF
out=$(HOME="$TMP/home" "$SHAII" "$TMP/pattern-entry-main.shay")
[ "$out" = "Pattern entry package loaded" ]
printf '%s\n' 'PASS: $entrysyndef accepts bare, prefixed, suffixed, and combined zero-argument names'

# A code-bodied $entrysyndef creates a zero-argument function that can call
# a differently named implementation from its bundled .shaii library.
mkdir -p "$TMP/baseinit-source" "$TMP/home/.shay/entries"
cat > "$TMP/baseinit-source/main.shaii" <<'EOF'
$declr baseExec()
instxt/>"Base function executed"
finish
EOF
cat > "$TMP/baseinit-source/baseinit-source.shay" <<'EOF'
$shaiiport "main.shaii"
$entrysyndef $dobase()
    dofunc baseExec
finish
EOF
"$SHENTRYPACK" "$TMP/baseinit-source/baseinit-source.shay" -o "$TMP/home/.shay/entries/baseinit.shpak"
test "$(wc -c < "$TMP/home/.shay/entries/baseinit.shpak")" -gt 69
rm -rf "$TMP/baseinit-source"
cat > "$TMP/baseinit-main.shay" <<'EOF'
$useentry baseinit
$dobase
dobase
EOF
out=$(HOME="$TMP/home" "$SHAII" "$TMP/baseinit-main.shay")
expected=$(printf '%s\n%s' "Base function executed" "Base function executed")
[ "$out" = "$expected" ]
HOME="$TMP/home" "$GSHAI" -o "$TMP/baseinit-app" "$TMP/baseinit-main.shay"
out=$("$TMP/baseinit-app")
[ "$out" = "$expected" ]
printf '%s\n' 'PASS: code-bodied $entrysyndef calls a differently named bundled .shaii function in shaii and gshai'
# shentrypak validates ordinary Shay syntax instead of blindly packaging bad source.
printf '%s\n' '$notashay' > "$TMP/invalid-shay.shay"
if "$SHENTRYPACK" "$TMP/invalid-shay.shay" -o "$TMP/invalid-shay.shpak" 2>"$TMP/invalid-shay-diagnostics"; then
    echo "expected shentrypak to reject invalid Shay syntax" >&2
    exit 1
fi
test ! -e "$TMP/invalid-shay.shpak"
printf '%s\n' 'PASS: shentrypak validates Shay syntax before writing packages'

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
if "$GSHAI" -et "$TMP/old.shpak" "$TMP/source/nested/userinstalled-source.shay" >/dev/null 2>&1; then
    echo "expected gshai -et to be removed" >&2
    exit 1
fi
printf '%s\n' 'PASS: entry package compilation is separated from gshai'
