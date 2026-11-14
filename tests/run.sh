#!/usr/bin/env sh
set -eu
GSHai="${1:-./build/gshai}"
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
pass() { printf 'PASS: %s\n' "$1"; }
fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

"$GSHai" --version | grep -q 'gshai' || fail "version command"
pass "version command"
"$GSHai" --check "$ROOT/examples/hello.shay" >/dev/null || fail "hello check"
pass "hello syntax check"
out=$("$GSHai" "$ROOT/examples/hello.shay")
printf '%s\n' "$out" | grep -q 'Hello, world from Shay!' || fail "text output"
printf '%s\n' "$out" | grep -q 'Shay' || fail "variable output"
pass "hello execution"

cat > "$TMP/lexer-parser.shay" <<'EOF'
brew greeting = "Line 1\nLine 2"
brew debt = -42
instxt-"Escaped quote: \"Shay\""
insval-greeting
insval-debt
EOF
out=$("$GSHai" "$TMP/lexer-parser.shay")
printf '%s\n' "$out" | grep -q 'Escaped quote: "Shay"' || fail "escaped quote output"
printf '%s\n' "$out" | grep -q 'Line 1' || fail "escaped newline output"
printf '%s\n' "$out" | grep -q -- '-42' || fail "negative number literal"
pass "lexer strings and signed numeric literals"

cat > "$TMP/bad-syntax.shay" <<'EOF'
brew value =
instxt-"unfinished
EOF
if "$GSHai" --check "$TMP/bad-syntax.shay" >"$TMP/out" 2>"$TMP/err"; then fail "malformed syntax"; fi
grep -q 'expected a string, number, or boolean literal' "$TMP/err" || fail "parser diagnostic"
grep -q 'unterminated string literal' "$TMP/err" || fail "lexer diagnostic"
pass "lexer and parser diagnostics"

cat > "$TMP/task.shaz" <<'EOF'
instxt-"Zeccurs task ran"
EOF
cat > "$TMP/lib.shaii" <<'EOF'
$shazport "task.shaz"
EOF
cat > "$TMP/main.shay" <<'EOF'
$shaiiport "lib.shaii"
EOF
out=$("$GSHai" "$TMP/main.shay")
printf '%s\n' "$out" | grep -q 'Zeccurs task ran' || fail "library to Zeccurs import"
pass "Shaii imports Zeccurs"

cat > "$TMP/bad.shaz" <<'EOF'
brew forbidden = 7
EOF
if "$GSHai" --check "$TMP/bad.shaz" >"$TMP/out" 2>"$TMP/err"; then fail "Zeccurs variable restriction"; fi
grep -q 'variable-free' "$TMP/err" || fail "Zeccurs diagnostic"
pass "Zeccurs rejects variable declarations"

cat > "$TMP/a.shaz" <<'EOF'
$shazport "b.shaz"
EOF
cat > "$TMP/b.shaz" <<'EOF'
$shazport "a.shaz"
EOF
if "$GSHai" --check "$TMP/a.shaz" >"$TMP/out" 2>"$TMP/err"; then fail "circular import"; fi
grep -q 'circular import' "$TMP/err" || fail "circular import diagnostic"
pass "circular imports diagnosed"

cat > "$TMP/no-library.shaii" <<'EOF'
$shaiiport "other.shaii"
EOF
if "$GSHai" --check "$TMP/no-library.shaii" >"$TMP/out" 2>"$TMP/err"; then fail "nested Shaii import"; fi
grep -q 'cannot import other .shaii' "$TMP/err" || fail "Shaii import restriction diagnostic"
pass "Shaii-to-Shaii imports rejected"
printf '\nAll gshai bootstrap tests passed.\n'
