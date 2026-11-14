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
