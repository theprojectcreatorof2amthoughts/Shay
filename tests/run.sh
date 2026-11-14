#!/usr/bin/env sh
set -eu
SHAII="${1:-./build/shaii}"
GSHAI="${2:-./build/gshai}"
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
pass() { printf 'PASS: %s\n' "$1"; }
fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

"$SHAII" --version | grep -q 'shaii' || fail "interpreter version"
pass "shaii interpreter version"
"$GSHAI" --version | grep -q 'gshai' || fail "compiler version"
pass "gshai compiler version"
"$SHAII" --check "$ROOT/examples/hello.shay" >/dev/null || fail "hello check"
pass "interpreter syntax check"
out=$("$SHAII" "$ROOT/examples/hello.shay")
printf '%s\n' "$out" | grep -q 'Hello, world from Shay!' || fail "interpreter text output"
printf '%s\n' "$out" | grep -q 'Shay' || fail "interpreter variable output"
pass "interpreter execution"

cat > "$TMP/lexer-parser.shay" <<'EOF'
brew greeting = "Line 1\nLine 2"
brew debt = -42
instxt-"Escaped quote: \"Shay\""
insval-greeting
insval-debt
EOF
out=$("$SHAII" "$TMP/lexer-parser.shay")
printf '%s\n' "$out" | grep -q 'Escaped quote: "Shay"' || fail "escaped quote output"
printf '%s\n' "$out" | grep -q 'Line 1' || fail "escaped newline output"
printf '%s\n' "$out" | grep -q -- '-42' || fail "negative number literal"
pass "lexer strings and signed numeric literals"

cat > "$TMP/bad-syntax.shay" <<'EOF'
brew value =
instxt-"unfinished
EOF
if "$SHAII" --check "$TMP/bad-syntax.shay" >"$TMP/out" 2>"$TMP/err"; then fail "malformed syntax"; fi
grep -q 'expected a string, number, or boolean literal' "$TMP/err" || fail "parser diagnostic"
grep -q 'unterminated string literal' "$TMP/err" || fail "lexer diagnostic"
pass "lexer and parser diagnostics"

cat > "$TMP/task.shaz" <<'EOF'
instxt-"Zeccurs task ran"
EOF
cat > "$TMP/lib.shaii" <<'EOF'
$shazport [task.shaz]
EOF
cat > "$TMP/main.shay" <<'EOF'
$shaiiport [lib.shaii]
EOF
out=$("$SHAII" "$TMP/main.shay")
printf '%s\n' "$out" | grep -q 'Zeccurs task ran' || fail "library to Zeccurs import"
pass "Shaii imports Zeccurs"

cat > "$TMP/siil-main.shay" <<'EOF'
$shaiiport [math.shaii]
$shaiiport [text.shaii]
insval-math_pi
instxt-"separator:"
insval-text_space
EOF
out=$("$SHAII" "$TMP/siil-main.shay")
printf '%s\n' "$out" | grep -q '3.14159' || fail "siil math library lookup"
printf '%s\n' "$out" | grep -q 'separator:' || fail "siil text library lookup"
pass "interpreter resolves standard libraries from siil/"

CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/siil-compiled" "$TMP/siil-main.shay" || fail "compiler standard-library import"
out=$("$TMP/siil-compiled")
printf '%s\n' "$out" | grep -q '3.14159' || fail "compiled siil constant output"
pass "compiler resolves standard libraries from siil/"

cat > "$TMP/bad.shaz" <<'EOF'
brew forbidden = 7
EOF
if "$SHAII" --check "$TMP/bad.shaz" >"$TMP/out" 2>"$TMP/err"; then fail "Zeccurs variable restriction"; fi
grep -q 'variable-free' "$TMP/err" || fail "Zeccurs diagnostic"
pass "Zeccurs rejects variable declarations"

cat > "$TMP/a.shaz" <<'EOF'
$shazport [b.shaz]
EOF
cat > "$TMP/b.shaz" <<'EOF'
$shazport [a.shaz]
EOF
if "$SHAII" --check "$TMP/a.shaz" >"$TMP/out" 2>"$TMP/err"; then fail "circular import"; fi
grep -q 'circular import' "$TMP/err" || fail "circular import diagnostic"
pass "circular imports diagnosed"

cat > "$TMP/no-library.shaii" <<'EOF'
$shaiiport [other.shaii]
EOF
if "$SHAII" --check "$TMP/no-library.shaii" >"$TMP/out" 2>"$TMP/err"; then fail "nested Shaii import"; fi
grep -q 'cannot import other .shaii' "$TMP/err" || fail "Shaii import restriction diagnostic"
pass "Shaii-to-Shaii imports rejected"

cp "$ROOT/examples/hello.shay" "$TMP/compiled.shay"
CXX="${CXX:-c++}" "$GSHAI" -ob "$TMP/compiled.shay" || fail "object compilation"
test -s "$TMP/compiled.sho" || fail "missing .sho output"
nm -g "$TMP/compiled.sho" | grep -q 'shay_program_entry' || fail ".sho missing Shay entry symbol"
cat > "$TMP/harness.cpp" <<'EOF'
extern "C" int shay_program_entry();
int main() { return shay_program_entry(); }
EOF
"${CXX:-c++}" -std=c++20 "$TMP/harness.cpp" "$TMP/compiled.sho" -o "$TMP/object-app"
out=$("$TMP/object-app")
printf '%s\n' "$out" | grep -q 'Hello, world from Shay!' || fail "object output"
pass "gshai emits linkable .sho objects"

CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/standalone" "$TMP/compiled.shay" || fail "executable compilation"
test -x "$TMP/standalone" || fail "missing executable"
out=$("$TMP/standalone")
printf '%s\n' "$out" | grep -q 'Hello, world from Shay!' || fail "executable text output"
printf '%s\n' "$out" | grep -q 'Shay' || fail "executable variable output"
pass "gshai -o creates and runs a standalone executable"

CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/compiled.shay" || fail "default executable output"
test -x "$TMP/compiled" || fail "default executable path"
pass "gshai -o derives executable filename"

cat > "$TMP/functions.shaii" <<'EOF'
$declr add(x, y)
    forw x + y
finish

$declr square(value)
    forw value * value
finish
EOF
cat > "$TMP/function-main.shay" <<'EOF'
$shaiiport [functions.shaii]
insval-add(10, 20)
insval-square(6)
EOF
"$SHAII" --check "$TMP/function-main.shay" >/dev/null || fail "function syntax check"
out=$("$SHAII" "$TMP/function-main.shay")
printf '%s\n' "$out" | grep -q '^30 || fail "interpreter function addition"
printf '%s\n' "$out" | grep -q '^36 || fail "interpreter function multiplication"
pass "interpreter declares, imports, and calls functions"
CXX=c++ "$GSHAI" -o "$TMP/function-app" "$TMP/function-main.shay" || fail "compile function program"
out=$("$TMP/function-app")
printf '%s\n' "$out" | grep -q '^30 || fail "compiled function addition"
printf '%s\n' "$out" | grep -q '^36 || fail "compiled function multiplication"
pass "compiler generates callable native functions"

cat > "$TMP/missing-finish.shay" <<'EOF'
$declr broken(x)
    forw x + 1
EOF
if "$SHAII" --check "$TMP/missing-finish.shay" >"$TMP/out" 2>"$TMP/err"; then fail "function missing finish"; fi
grep -q "expected 'finish' to close the function" "$TMP/err" || fail "missing finish diagnostic"
pass "function closing keyword is required"

printf '\nAll Shay toolchain tests passed.\n'
