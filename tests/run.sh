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
grep -q 'expected a literal or numeric expression after' "$TMP/err" || fail "parser diagnostic"
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

mkdir -p "$TMP/external"
cat > "$TMP/external/custom.shaii" <<'EOF'
instxt-"External library loaded"
EOF
cat > "$TMP/external-main.shay" <<'EOF'
$shaiiport "external/custom.shaii"
EOF
out=$("$SHAII" "$TMP/external-main.shay") || fail "quoted external library import"
printf '%s\\n' "$out" | grep -q 'External library loaded' || fail "interpreter external library output"
pass "quoted shaiiport imports external files from disk"
CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/external-app" "$TMP/external-main.shay" || fail "compiler quoted external library import"
out=$("$TMP/external-app") || fail "run compiled external library program"
printf '%s\\n' "$out" | grep -q 'External library loaded' || fail "compiler external library output"
pass "compiler resolves quoted external library imports"

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

CXX="${CXX:-c++}" "$GSHAI" --shaywall --shaywextra --shayo2 -o "$TMP/native-flags-app" "$TMP/compiled.shay" || fail "Shay-native warning and optimization flags"
test -x "$TMP/native-flags-app" || fail "native flags did not produce executable"
out=$("$TMP/native-flags-app")
printf '%s\\n' "$out" | grep -q 'Hello, world from Shay!' || fail "native flags executable output"
pass "gshai accepts --shaywall, --shaywextra, and --shayo2"

CXX="${CXX:-c++}" "$GSHAI" --shayo0 -o "$TMP/no-opt-app" "$TMP/compiled.shay" || fail "Shay -O0 flag"
test -x "$TMP/no-opt-app" || fail "Shay -O0 output"
pass "gshai supports Shay optimization levels"


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
printf '%s\n' "$out" | grep -q '30' || fail "interpreter function addition"
printf '%s\n' "$out" | grep -q '36' || fail "interpreter function multiplication"
pass "interpreter declares, imports, and calls functions"
CXX=c++ "$GSHAI" -o "$TMP/function-app" "$TMP/function-main.shay" || fail "compile function program"
out=$("$TMP/function-app")
printf '%s\n' "$out" | grep -q '30' || fail "compiled function addition"
printf '%s\n' "$out" | grep -q '36' || fail "compiled function multiplication"
pass "compiler generates callable native functions"

cat > "$TMP/missing-finish.shay" <<'EOF'
$declr broken(x)
    forw x + 1
EOF
if "$SHAII" --check "$TMP/missing-finish.shay" >"$TMP/out" 2>"$TMP/err"; then fail "function missing finish"; fi
grep -q "expected 'finish' to close the function" "$TMP/err" || fail "missing finish diagnostic"
pass "function closing keyword is required"

cat > "$TMP/conditionals.shay" <<'EOF'
brew x = 20
brew y = 10
ifit x > y
    instxt-"X is greater than Y"
exec
ifit x < y
    instxt-"This must not appear"
exec
ifit x == 20
    instxt-"Equality works"
exec
EOF
out=$("$SHAII" "$TMP/conditionals.shay") || fail "interpreter ifit execution"
printf '%s\n' "$out" | grep -q 'X is greater than Y' || fail "interpreter ifit true branch"
printf '%s\n' "$out" | grep -q 'Equality works' || fail "interpreter ifit equality"
if printf '%s\n' "$out" | grep -q 'must not appear'; then fail "interpreter ifit false branch"; fi
pass "interpreter evaluates ifit comparisons and exec blocks"
CXX=c++ "$GSHAI" -o "$TMP/conditional-app" "$TMP/conditionals.shay" || fail "compile ifit program"
out=$("$TMP/conditional-app") || fail "run compiled ifit program"
printf '%s\n' "$out" | grep -q 'X is greater than Y' || fail "compiled ifit true branch"
printf '%s\n' "$out" | grep -q 'Equality works' || fail "compiled ifit equality"
if printf '%s\n' "$out" | grep -q 'must not appear'; then fail "compiled ifit false branch"; fi
pass "compiler generates native ifit conditionals"
cat > "$TMP/missing-exec.shay" <<'EOF'
ifit 2 > 1
    instxt-"missing terminator"
EOF
if "$SHAII" --check "$TMP/missing-exec.shay" >"$TMP/out" 2>"$TMP/err"; then fail "ifit missing exec"; fi
grep -q "expected 'exec' to close the ifit block" "$TMP/err" || fail "missing exec diagnostic"
pass "ifit requires exec terminator"

cat > "$TMP/preflight-errors.shay" <<'EOF'
brew first = 10
brew first = 20
insval-missing_value
ifit missing_condition > 0
    insval-missing_inside_block
exec
insval-missing_function(unknown_argument)
EOF
if "$GSHAI" -o "$TMP/must-not-exist" "$TMP/preflight-errors.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler preflight accepted invalid source"; fi
grep -q "variable 'first' is already declared" "$TMP/err" || fail "duplicate variable preflight diagnostic"
grep -q "unknown variable 'missing_value'" "$TMP/err" || fail "unknown top-level variable preflight diagnostic"
grep -q "ifit condition references unknown variable 'missing_condition'" "$TMP/err" || fail "ifit condition preflight diagnostic"
grep -q "unknown variable 'missing_inside_block' inside ifit block" "$TMP/err" || fail "ifit body preflight diagnostic"
grep -q "unknown function 'missing_function'" "$TMP/err" || fail "unknown function preflight diagnostic"
grep -q "function argument references unknown variable 'unknown_argument'" "$TMP/err" || fail "function argument preflight diagnostic"
test ! -e "$TMP/must-not-exist" || fail "compiler emitted output despite preflight errors"
pass "gshai preflight reports multiple semantic errors and blocks output"


cat > "$TMP/function-body.shaii" <<'EOF'
$declr useme(x, y)
    ifit x > y
        instxt-"the first val is bigger than second val"
    exec
finish
EOF
cat > "$TMP/function-body-main.shay" <<'EOF'
$shaiiport "function-body.shaii"
instxt-"function body test:"
insval-useme(7, 6)
insval-useme(6, 7)
EOF
"$SHAII" --check "$TMP/function-body-main.shay" >/dev/null || fail "statement function body syntax check"
out=$("$SHAII" "$TMP/function-body-main.shay") || fail "interpreter statement function call"
printf '%s\n' "$out" | grep -q 'the first val is bigger than second val' || fail "interpreter function ifit body"
if [ "$(printf '%s\n' "$out" | grep -c 'the first val is bigger than second val')" -ne 1 ]; then
    fail "interpreter function condition scope"
fi
pass "interpreter executes ifit and instxt inside void functions"
CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/function-body-app" "$TMP/function-body-main.shay" || fail "compile statement function body"
out=$("$TMP/function-body-app") || fail "run compiled statement function body"
printf '%s\n' "$out" | grep -q 'the first val is bigger than second val' || fail "compiler function ifit body"
if [ "$(printf '%s\n' "$out" | grep -c 'the first val is bigger than second val')" -ne 1 ]; then
    fail "compiler function condition scope"
fi
pass "compiler generates ifit and instxt inside void functions"

cat > "$TMP/numeric-initializers.shay" <<'EOF'
brew base = 5
brew multiplier = 3
brew total = base * multiplier + 2
brew offset = -(total - 20)
insval-total
insval-offset
ifit total > 10
    instxt-"numeric initializer condition works"
exec
EOF
"$SHAII" --check "$TMP/numeric-initializers.shay" >/dev/null || fail "numeric initializer syntax check"
out=$("$SHAII" "$TMP/numeric-initializers.shay") || fail "interpreter numeric initializers"
printf '%s\n' "$out" | grep -q '^17$' || fail "interpreter numeric expression precedence"
printf '%s\n' "$out" | grep -q '^3$' || fail "interpreter unary numeric expression"
printf '%s\n' "$out" | grep -q 'numeric initializer condition works' || fail "interpreter numeric initializer condition"
pass "interpreter evaluates arithmetic brew initializers with precedence and unary operators"
"$GSHAI" -o "$TMP/numeric-initializer-app" "$TMP/numeric-initializers.shay" || fail "compile numeric initializers"
out=$("$TMP/numeric-initializer-app") || fail "run compiled numeric initializers"
printf '%s\n' "$out" | grep -q '^17$' || fail "compiler numeric expression precedence"
printf '%s\n' "$out" | grep -q '^3$' || fail "compiler unary numeric expression"
printf '%s\n' "$out" | grep -q 'numeric initializer condition works' || fail "compiler numeric initializer condition"
pass "compiler evaluates arithmetic brew initializers consistently with interpreter"

cat > "$TMP/non-numeric-initializer.shay" <<'EOF'
brew label = "hello"
brew invalid = label + 1
EOF
if "$GSHAI" -o "$TMP/non-numeric-app" "$TMP/non-numeric-initializer.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted non-numeric initializer operand"; fi
grep -q "numeric initializer references non-numeric variable 'label'" "$TMP/err" || fail "non-numeric initializer diagnostic"
test ! -e "$TMP/non-numeric-app" || fail "compiler emitted invalid numeric initializer output"
pass "compiler rejects non-numeric variables in arithmetic initializers"

cat > "$TMP/boolean-initializer.shay" <<'EOF'
brew invalid = true + 1
EOF
if "$GSHAI" -o "$TMP/boolean-app" "$TMP/boolean-initializer.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted boolean arithmetic operand"; fi
grep -q "numeric initializer cannot use boolean literal 'true'" "$TMP/err" || fail "boolean arithmetic diagnostic"
pass "compiler rejects boolean literals in numeric initializers"

printf '\nAll Shay toolchain tests passed.\n'
