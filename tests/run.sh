#!/usr/bin/env sh
set -eu
SHAII="${1:-./build/shaii}"
GSHAI="${2:-./build/gshai}"
SHUI="${3:-./build/shui}"
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$SHAII" in /*) ;; *) SHAII="$ROOT/$SHAII";; esac
case "$GSHAI" in /*) ;; *) GSHAI="$ROOT/$GSHAI";; esac
case "$SHUI" in /*) ;; *) SHUI="$ROOT/$SHUI";; esac
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
pass() { printf 'PASS: %s\n' "$1"; }
fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }

"$SHAII" --version | grep -q 'shaii' || fail "interpreter version"
pass "shaii interpreter version"
"$GSHAI" --version | grep -q 'gshai' || fail "compiler version"
pass "gshai compiler version"

cat > "$TMP/unused-variable.shay" <<'EOF'
brew unused_result = "unused"
instxt/>"Compiler still succeeds"
EOF
"$GSHAI" --shaywall --shaywextra -o "$TMP/unused-variable-app" "$TMP/unused-variable.shay" >"$TMP/out" 2>"$TMP/err" || { cat "$TMP/out" "$TMP/err" >&2; fail "unused-variable warning stopped compilation"; }
grep -q ": warning: variable 'unused_result' is never used" "$TMP/err" || fail "Shay unused-variable warning"
grep -q 'brew unused_result = "unused"' "$TMP/err" || fail "unused-variable source line"
grep -q '~~~~~~~~~~~~^' "$TMP/err" || fail "unused-variable caret underline"
if grep -q 'unused function' "$TMP/err"; then fail "generated C++ warnings were not suppressed"; fi
out=$("$TMP/unused-variable-app") || fail "run program with Shay warning"
printf '%s\n' "$out" | grep -q 'Compiler still succeeds' || fail "warning changed generated program behavior"
pass "Shay reports unused variables with source carets without leaking native C++ warnings"

"$SHAII" --check "$ROOT/examples/hello.shay" >/dev/null || fail "hello check"
pass "interpreter syntax check"
out=$("$SHAII" "$ROOT/examples/hello.shay")
printf '%s\n' "$out" | grep -q 'Hello, world from Shay!' || fail "interpreter text output"
printf '%s\n' "$out" | grep -q 'Shay' || fail "interpreter variable output"
pass "interpreter execution"

cat > "$TMP/multi-output.shay" <<'EOF'
brew first = "First"
brew second = "Second"
brew third = "Third"
insval/>first, second, third
EOF
expected=$(printf 'First\nSecond\nThird')
out=$("$SHAII" "$TMP/multi-output.shay") || fail "interpreter multiple-variable output"
[ "$out" = "$expected" ] || fail "interpreter multiple-variable output order"
pass "insval/> outputs multiple variables in order in the interpreter"

CXX="${CXX:-c++}" "$GSHAI" --shaywall --shaywextra -o "$TMP/multi-output-app" "$TMP/multi-output.shay" >"$TMP/out" 2>"$TMP/err" || { cat "$TMP/out" "$TMP/err" >&2; fail "compile multiple-variable output"; }
if grep -q "variable 'first' is never used\|variable 'second' is never used\|variable 'third' is never used" "$TMP/err"; then fail "multiple-variable output was not counted as variable use"; fi
out=$("$TMP/multi-output-app") || fail "run compiled multiple-variable output"
[ "$out" = "$expected" ] || fail "compiled multiple-variable output order"
pass "gshai compiles multiple-variable output and tracks all variables as used"

cat > "$TMP/multi-output-function.shaii" <<'EOF'
$declr output_pair()
    insval/>first, second
finish
EOF
cat > "$TMP/multi-output-function.shay" <<'EOF'
$shaiiport [multi-output-function.shaii]
brew first = "Function first"
brew second = "Function second"
dofunc output_pair
EOF
expected=$(printf 'Function first\nFunction second')
out=$("$SHAII" "$TMP/multi-output-function.shay") || fail "interpreter multiple-variable output inside function"
[ "$out" = "$expected" ] || fail "interpreter multiple-variable function output"
CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/multi-output-function-app" "$TMP/multi-output-function.shay" || fail "compile multiple-variable function output"
out=$("$TMP/multi-output-function-app") || fail "run compiled multiple-variable function output"
[ "$out" = "$expected" ] || fail "compiled multiple-variable function output"
pass "insval/> supports multiple variables inside function bodies"

cat > "$TMP/multi-output-ten.shay" <<'EOF'
brew v1 = "1"
brew v2 = "2"
brew v3 = "3"
brew v4 = "4"
brew v5 = "5"
brew v6 = "6"
brew v7 = "7"
brew v8 = "8"
brew v9 = "9"
brew v10 = "10"
insval/>v1, v2, v3, v4, v5, v6, v7, v8, v9, v10
EOF
expected=$(printf '1\n2\n3\n4\n5\n6\n7\n8\n9\n10')
out=$("$SHAII" "$TMP/multi-output-ten.shay") || fail "interpreter ten-variable output"
[ "$out" = "$expected" ] || fail "interpreter ten-variable output"
CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/multi-output-ten-app" "$TMP/multi-output-ten.shay" || fail "compile ten-variable output"
out=$("$TMP/multi-output-ten-app") || fail "run compiled ten-variable output"
[ "$out" = "$expected" ] || fail "compiled ten-variable output"
pass "insval/> accepts and outputs the maximum of 10 variables"

cat > "$TMP/multi-output-eleven.shay" <<'EOF'
brew v1 = "1"
brew v2 = "2"
brew v3 = "3"
brew v4 = "4"
brew v5 = "5"
brew v6 = "6"
brew v7 = "7"
brew v8 = "8"
brew v9 = "9"
brew v10 = "10"
brew v11 = "11"
insval/>v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11
EOF
if "$SHAII" --check "$TMP/multi-output-eleven.shay" >"$TMP/out" 2>"$TMP/err"; then fail "interpreter accepted eleven-variable output"; fi
grep -q "insval/> supports a maximum of 10 variables" "$TMP/err" || fail "eleven-variable parser diagnostic"
if "$GSHAI" -o "$TMP/multi-output-eleven-app" "$TMP/multi-output-eleven.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted eleven-variable output"; fi
grep -q "insval/> supports a maximum of 10 variables" "$TMP/err" || fail "compiler eleven-variable parser diagnostic"
pass "insval/> rejects more than 10 variables with a clear diagnostic"


# The new self-closing operator is mandatory for these three statements.
for legacy in "instxt-\"old syntax\"" "instxtexit-\"old syntax\"" "insval-name" "inpvarforw-name"; do
    case "$legacy" in
        instxt-*|instxtexit-*) printf '%s\n' "$legacy" >"$TMP/legacy-statement.shay" ;;
        *) printf '%s\n' "brew name = \"value\"" "$legacy" >"$TMP/legacy-statement.shay" ;;
    esac
    if "$SHAII" --check "$TMP/legacy-statement.shay" >"$TMP/out" 2>"$TMP/err"; then
        fail "legacy statement syntax was accepted: $legacy"
    fi
done
pass "legacy hyphen syntax is rejected for instxt, instxtexit, insval, and inpvarforw"

cat > "$TMP/input-created.shay" <<'EOF'
instxt/>"What's your name?"
inpvarforw/>name
instxt/>"Hello,"
insval/>name
EOF
out=$(printf 'Yamen Awartani\n' | "$SHAII" "$TMP/input-created.shay") || fail "interpreter input declaration"
printf '%s\n' "$out" | grep -q 'Yamen Awartani' || fail "interpreter input preserves spaces"
pass "inpvarforw creates a variable and reads a full input line"
"$SHAII" --check "$TMP/input-created.shay" >/dev/null || fail "inpvarforw check-only mode"
pass "shaii --check does not wait for input"

cat > "$TMP/input-existing.shay" <<'EOF'
brew name = "old value"
inpvarforw/>name
insval/>name
EOF
out=$(printf 'new value with spaces\n' | "$SHAII" "$TMP/input-existing.shay") || fail "interpreter input reassignment"
printf '%s\n' "$out" | grep -q 'new value with spaces' || fail "interpreter input overwrites an existing variable"
pass "inpvarforw overwrites an existing variable"

cat > "$TMP/input-repeat.shay" <<'EOF'
inpvarforw/>name
inpvarforw/>name
insval/>name
EOF
out=$(printf 'first value\nsecond value\n' | "$SHAII" "$TMP/input-repeat.shay") || fail "repeat inpvarforw"
printf '%s\n' "$out" | grep -q 'second value' || fail "repeat inpvarforw updates the variable"
pass "repeated inpvarforw statements reuse and update the same variable"

CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/input-created-app" "$TMP/input-created.shay" || fail "compile program with inpvarforw"
out=$(printf 'compiled input with spaces\n' | "$TMP/input-created-app") || fail "run compiled input program"
printf '%s\n' "$out" | grep -q 'compiled input with spaces' || fail "compiled input preserves spaces"
pass "gshai compiles inpvarforw into standard-input reading"

CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/input-existing-app" "$TMP/input-existing.shay" || fail "compile existing-variable input program"
out=$(printf 'updated compiler value\n' | "$TMP/input-existing-app") || fail "run existing-variable input program"
printf '%s\n' "$out" | grep -q 'updated compiler value' || fail "compiled input overwrites an existing variable"
pass "gshai supports inpvarforw on existing variables"

cat > "$TMP/input-then-brew.shay" <<'EOF'
inpvarforw/>name
brew name = "duplicate"
EOF
if CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/input-then-brew" "$TMP/input-then-brew.shay" >"$TMP/out" 2>"$TMP/err"; then fail "brew redeclared an inpvarforw variable"; fi
grep -q "variable 'name' is already declared" "$TMP/err" || fail "inpvarforw declaration tracking"
pass "inpvarforw registers newly created variables for duplicate-declaration checks"

cat > "$TMP/input-global-function.shay" <<'EOF'
$shaiiport "input-global-function.shaii"
instxt/>"hi"
inpvarforw/>noinp
insval/>useme(4, 8)
ifit !#useme
    instxtexit/>"Error!"
exec
EOF
cat > "$TMP/input-global-function.shaii" <<'EOF'
$declr useme(x, y)
    ifit x*y = noinp
        !#useme
    elseit
        instxt/>"Result:"
        forw x*y
    exec
finish
EOF
CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/input-global-function-app" "$TMP/input-global-function.shay" || fail "compile imported function using later inpvarforw global"
out=$(printf '31\n' | "$TMP/input-global-function-app") || fail "run imported function using later inpvarforw global"
printf '%s\n' "$out" | grep -q 'Result:' || fail "later inpvarforw global was not visible inside imported function"
printf '%s\n' "$out" | grep -q '32' || fail "imported function returned the wrong result"
pass "imported functions can reference globals declared after their import"
cat > "$TMP/lexer-parser.shay" <<'EOF'
brew greeting = "Line 1\nLine 2"
brew debt = -42
instxt/>"Escaped quote: \"Shay\""
insval/>greeting
insval/>debt
EOF
out=$("$SHAII" "$TMP/lexer-parser.shay")
printf '%s\n' "$out" | grep -q 'Escaped quote: "Shay"' || fail "escaped quote output"
printf '%s\n' "$out" | grep -q 'Line 1' || fail "escaped newline output"
printf '%s\n' "$out" | grep -q -- '-42' || fail "negative number literal"
pass "lexer strings and signed numeric literals"

cat > "$TMP/bad-syntax.shay" <<'EOF'
brew value =
instxt/>"unfinished
EOF
if "$SHAII" --check "$TMP/bad-syntax.shay" >"$TMP/out" 2>"$TMP/err"; then fail "malformed syntax"; fi
grep -q 'expected a string, number, or boolean literal' "$TMP/err" || fail "parser diagnostic"
grep -q 'unterminated string literal' "$TMP/err" || fail "lexer diagnostic"
pass "lexer and parser diagnostics"
if "$GSHAI" -o "$TMP/bad-syntax-app" "$TMP/bad-syntax.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted malformed syntax"; fi
grep -q "bad-syntax.shay:1:13: error:" "$TMP/err" || fail "compiler parser error column"
grep -q "bad-syntax.shay:2:9: error: unterminated string literal" "$TMP/err" || fail "compiler lexer error column"
pass "gshai preserves exact parser and lexer error columns"
# Force colors for deterministic assertions, even when CI captures stderr.
if CLICOLOR_FORCE=1 "$GSHAI" -o "$TMP/bad-syntax-colored-app" "$TMP/bad-syntax.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted malformed syntax with forced colors"; fi
esc=$(printf '\033')
grep -q "${esc}\\[1;36m" "$TMP/err" || fail "diagnostic source location color"
grep -q "${esc}\\[1;31merror${esc}\\[0m" "$TMP/err" || fail "diagnostic error color"
grep -q "${esc}\\[1;31m\\^" "$TMP/err" || fail "diagnostic error caret color"
if NO_COLOR=1 CLICOLOR_FORCE=1 "$GSHAI" -o "$TMP/bad-syntax-no-color-app" "$TMP/bad-syntax.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted malformed syntax with NO_COLOR"; fi
if grep -q "$esc" "$TMP/err"; then fail "NO_COLOR did not disable diagnostic colors"; fi
pass "gshai colors errors and source locations and honors NO_COLOR"

cat > "$TMP/task.shaz" <<'EOF'
instxt/>"Zeccurs task ran"
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
insval/>math_pi
instxt/>"separator:"
insval/>text_space
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
instxt/>"External library loaded"
EOF
cat > "$TMP/external-main.shay" <<'EOF'
$shaiiport "external/custom.shaii"
EOF
out=$("$SHAII" "$TMP/external-main.shay") || fail "quoted external library import"
printf '%s\n' "$out" | grep -q 'External library loaded' || fail "interpreter external library output"
pass "quoted shaiiport imports external files from disk"
CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/external-app" "$TMP/external-main.shay" || fail "compiler quoted external library import"
out=$("$TMP/external-app") || fail "run compiled external library program"
printf '%s\n' "$out" | grep -q 'External library loaded' || fail "compiler external library output"
pass "compiler resolves quoted external library imports"

cat > "$TMP/bad.shaz" <<'EOF'
brew forbidden = 7
EOF
if "$SHAII" --check "$TMP/bad.shaz" >"$TMP/out" 2>"$TMP/err"; then fail "Zeccurs variable restriction"; fi
grep -q 'variable-free' "$TMP/err" || fail "Zeccurs diagnostic"
pass "Zeccurs rejects variable declarations"

cat > "$TMP/input-task.shaz" <<'EOF'
inpvarforw/>task_value
EOF
if "$SHAII" --check "$TMP/input-task.shaz" >"$TMP/out" 2>"$TMP/err"; then fail "Zeccurs input declaration restriction"; fi
grep -q "variable-free; 'inpvarforw' is not allowed" "$TMP/err" || fail "Zeccurs input declaration diagnostic"
if CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/input-task" "$TMP/input-task.shaz" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted Zeccurs input declaration"; fi
grep -q "variable-free; 'inpvarforw' is not allowed" "$TMP/err" || fail "compiler Zeccurs input diagnostic"
pass "Zeccurs rejects inpvarforw declarations"

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
printf '%s\n' "$out" | grep -q 'Hello, world from Shay!' || fail "native flags executable output"
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
insval/>add(10, 20)
insval/>square(6)
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
    instxt/>"X is greater than Y"
exec
ifit x < y
    instxt/>"This must not appear"
exec
ifit x == 20
    instxt/>"Equality works"
exec
EOF
out=$("$SHAII" "$TMP/conditionals.shay") || fail "interpreter ifit execution"
printf '%s\n' "$out" | grep -q 'X is greater than Y' || fail "interpreter ifit true branch"
printf '%s\n' "$out" | grep -q 'Equality works' || fail "interpreter ifit equality"
if printf '%s\n' "$out" | grep -Eq 'must not appear|Wrong else branch|Wrong nested else branch'; then fail "interpreter ifit false branch"; fi
pass "interpreter evaluates ifit comparisons and exec blocks"
CXX=c++ "$GSHAI" -o "$TMP/conditional-app" "$TMP/conditionals.shay" || fail "compile ifit program"
out=$("$TMP/conditional-app") || fail "run compiled ifit program"
printf '%s\n' "$out" | grep -q 'X is greater than Y' || fail "compiled ifit true branch"
printf '%s\n' "$out" | grep -q 'Equality works' || fail "compiled ifit equality"
if printf '%s\n' "$out" | grep -Eq 'must not appear|Wrong else branch|Wrong nested else branch'; then fail "compiled ifit false branch"; fi
pass "compiler generates native ifit conditionals"
cat > "$TMP/missing-exec.shay" <<'EOF'
ifit 2 > 1
    instxt/>"missing terminator"
EOF
if "$SHAII" --check "$TMP/missing-exec.shay" >"$TMP/out" 2>"$TMP/err"; then fail "ifit missing exec"; fi
grep -q "expected 'exec' to close the ifit block" "$TMP/err" || fail "missing exec diagnostic"
pass "ifit requires exec terminator"

cat > "$TMP/preflight-errors.shay" <<'EOF'
brew first = 10
brew first = 20
insval/>missing_value
ifit missing_condition > 0
    insval/>missing_inside_block
exec
insval/>missing_function(unknown_argument)
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

cat > "$TMP/source-location.shay" <<'EOF'
brew first = 10
insval/>missing_value
EOF
if "$GSHAI" -o "$TMP/source-location-app" "$TMP/source-location.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted invalid source-location test"; fi
grep -q "insval/>missing_value" "$TMP/err" || fail "compiler diagnostic source line"
grep -q "~~~~~~~~~~~~\^" "$TMP/err" || fail "compiler diagnostic caret underline"
pass "gshai diagnostics display the source line and pinpoint the offending identifier"
grep -q "source-location.shay:2:9: error:" "$TMP/err" || fail "compiler diagnostic line and column"

cat > "$TMP/warning-location.shay" <<'EOF'
brew unused_value = "not used"
instxt/>"done"
EOF
if "$GSHAI" --shaywall -o "$TMP/warning-location-app" "$TMP/warning-location.shay" >"$TMP/out" 2>"$TMP/err"; then :; else fail "compile unused-variable warning location test"; fi
grep -q "warning-location.shay:1:6: warning: variable 'unused_value' is never used" "$TMP/err" || fail "warning diagnostic line and column"
grep -q "brew unused_value" "$TMP/err" || fail "warning diagnostic source statement"
pass "gshai warnings display line and column and point to the declared variable"
if CLICOLOR_FORCE=1 "$GSHAI" --shaywall -o "$TMP/warning-colored-app" "$TMP/warning-location.shay" >"$TMP/out" 2>"$TMP/err"; then :; else fail "compile warning color test"; fi
esc=$(printf '\033')
grep -q "${esc}\\[1;33mwarning${esc}\\[0m" "$TMP/err" || fail "diagnostic warning color"
pass "gshai highlights warnings in yellow"




cat > "$TMP/function-body.shaii" <<'EOF'
$declr useme(x, y)
    ifit x > y
        instxt/>"the first val is bigger than second val"
    exec
finish
EOF
cat > "$TMP/function-body-main.shay" <<'EOF'
$shaiiport "function-body.shaii"
instxt/>"function body test:"
insval/>useme(7, 6)
insval/>useme(6, 7)
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


cat > "$TMP/logical-conditions.shay" <<'EOF'
brew x = 20
brew y = 10
brew z = 5
brew invalid_number = "not numeric"
ifit x > y andit y > z
    instxt/>"AND works"
exec
ifit x < y orit y == 10
    instxt/>"OR works"
exec
ifit notit x < y
    instxt/>"NOT works"
exec
ifit (x > y andit y > z) orit z > 100
    instxt/>"Grouped logic works"
exec
ifit x < y andit y > z
    instxt/>"This must not appear"
exec
ifit x > y orit invalid_number > 0
    instxt/>"Short-circuit OR works"
exec
ifit x < y andit invalid_number > 0
    instxt/>"This must not appear either"
exec
ifit x < y
    instxt/>"Wrong else branch"
elseit
    instxt/>"ELSE works"
    ifit y > z
        instxt/>"Nested else condition works"
    elseit
        instxt/>"Wrong nested else branch"
    exec
exec
EOF
"$SHAII" --check "$TMP/logical-conditions.shay" >/dev/null || fail "logical condition syntax check"
out=$("$SHAII" "$TMP/logical-conditions.shay") || fail "interpreter logical conditions"
for expected in "AND works" "OR works" "NOT works" "Grouped logic works" "Short-circuit OR works" "ELSE works" "Nested else condition works"; do
    printf '%s\n' "$out" | grep -q "$expected" || fail "interpreter $expected"
done
if printf '%s\n' "$out" | grep -Eq 'must not appear|Wrong else branch|Wrong nested else branch'; then fail "interpreter logical false branch"; fi
pass "interpreter supports andit, orit, notit, and grouped conditions"
CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/logical-conditions-app" "$TMP/logical-conditions.shay" || fail "compile logical conditions"
out=$("$TMP/logical-conditions-app") || fail "run compiled logical conditions"
for expected in "AND works" "OR works" "NOT works" "Grouped logic works"; do
    printf '%s\n' "$out" | grep -q "$expected" || fail "compiler $expected"
done
if printf '%s\n' "$out" | grep -Eq 'must not appear|Wrong else branch|Wrong nested else branch'; then fail "compiler logical false branch"; fi
pass "compiler generates native logical conditions"


cat > "$TMP/error-conditions.shaii" <<'EOF'
$declr useme(x, y)
    forw x / y
finish
$declr safe(x, y)
    forw x + y
finish
EOF
cat > "$TMP/error-conditions.shay" <<'EOF'
$shaiiport [error-conditions.shaii]
insval/>useme(10, 0)
ifit !#useme
    instxt/>"The useme call failed"
elseit
    instxt/>"The useme call succeeded"
exec
insval/>safe(10, 2)
ifit !#safe
    instxt/>"Wrong: safe inherited another function's failure"
elseit
    instxt/>"Safe call succeeded"
exec
insval/>useme(10, 2)
ifit !#useme
    instxt/>"Wrong: stale useme failure"
elseit
    instxt/>"Successful useme call cleared its failure"
exec
instxt/>"Execution continued"
EOF
out=$("$SHAII" "$TMP/error-conditions.shay") || fail "interpreter operation-specific error condition"
printf '%s\n' "$out" | grep -q 'The useme call failed' || fail "interpreter !# failure branch"
printf '%s\n' "$out" | grep -q 'Execution continued' || fail "interpreter continues after handled failure"
printf '%s\n' "$out" | grep -q 'Safe call succeeded' || fail "interpreter operation-specific status"
printf '%s\n' "$out" | grep -q 'Successful useme call cleared its failure' || fail "interpreter clears stale failure"
if printf '%s\n' "$out" | grep -q 'The useme call succeeded'; then fail "interpreter !# selected success branch"; fi
pass "interpreter tracks a specific function call with !#"

CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/error-conditions-app" "$TMP/error-conditions.shay" || fail "compile operation-specific error condition"
out=$("$TMP/error-conditions-app") || fail "compiled operation-specific error condition"
printf '%s\n' "$out" | grep -q 'The useme call failed' || fail "compiler !# failure branch"
printf '%s\n' "$out" | grep -q 'Execution continued' || fail "compiler continues after handled failure"
printf '%s\n' "$out" | grep -q 'Safe call succeeded' || fail "compiler operation-specific status"
printf '%s\n' "$out" | grep -q 'Successful useme call cleared its failure' || fail "compiler clears stale failure"
if printf '%s\n' "$out" | grep -q 'The useme call succeeded'; then fail "compiler !# selected success branch"; fi
pass "compiler tracks a specific function call with !#"

cat > "$TMP/error-exit.shay" <<'EOF'
$shaiiport [error-conditions.shaii]
insval/>useme(10, 0)
ifit !#useme
    instxtexit/>"Error!"
exec
instxt/>"This must not appear"
EOF
if "$SHAII" "$TMP/error-exit.shay" >"$TMP/out" 2>"$TMP/err"; then fail "interpreter instxtexit exit status"; fi
grep -q '^Error!$' "$TMP/out" || fail "interpreter instxtexit message"
if grep -q 'This must not appear' "$TMP/out"; then fail "interpreter continued after instxtexit"; fi
pass "interpreter instxtexit prints and terminates"

CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/error-exit-app" "$TMP/error-exit.shay" || fail "compile instxtexit program"
if "$TMP/error-exit-app" >"$TMP/out" 2>"$TMP/err"; then fail "compiler instxtexit exit status"; fi
grep -q '^Error!$' "$TMP/out" || fail "compiler instxtexit message"
if grep -q 'This must not appear' "$TMP/out"; then fail "compiler continued after instxtexit"; fi
pass "compiler instxtexit prints and terminates"

 

cat > "$TMP/explicit-function-failure.shaii" <<'EOF'
$declr useme(x, y)
    ifit x*y = 34
        !#useme
    exec
    instxt/>"Function continued after marking failure"
finish
EOF
cat > "$TMP/explicit-function-failure.shay" <<'EOF'
$shaiiport [explicit-function-failure.shaii]
insval/>useme(17, 2)
ifit !#useme
    instxtexit/>"Error!"
exec
instxt/>"This must not appear"
EOF
if "$SHAII" "$TMP/explicit-function-failure.shay" >"$TMP/out" 2>"$TMP/err"; then fail "interpreter explicit function failure exit status"; fi
grep -q '^Error!$' "$TMP/out" || fail "interpreter explicit function failure was not visible to caller"
grep -q 'Function continued after marking failure' "$TMP/out" || fail "interpreter stopped function execution after marker"
if grep -q 'This must not appear' "$TMP/out"; then fail "interpreter continued after explicit function failure"; fi
pass "interpreter propagates an explicit !#function failure to its caller"

CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/explicit-function-failure-app" "$TMP/explicit-function-failure.shay" || fail "compile explicit function failure"
if "$TMP/explicit-function-failure-app" >"$TMP/out" 2>"$TMP/err"; then fail "compiler explicit function failure exit status"; fi
grep -q '^Error!$' "$TMP/out" || fail "compiler explicit function failure was not visible to caller"
grep -q 'Function continued after marking failure' "$TMP/out" || fail "compiler stopped function execution after marker"
if grep -q 'This must not appear' "$TMP/out"; then fail "compiler continued after explicit function failure"; fi
pass "compiler propagates an explicit !#function failure to its caller"

cat > "$TMP/invalid-function-failure.shay" <<'EOF'
!#useme
EOF
if "$SHAII" --check "$TMP/invalid-function-failure.shay" >"$TMP/out" 2>"$TMP/err"; then fail "interpreter accepted top-level !# statement"; fi
grep -q "only valid inside" "$TMP/err" || fail "interpreter top-level !# diagnostic"
if "$GSHAI" -o "$TMP/invalid-function-failure-app" "$TMP/invalid-function-failure.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted top-level !# statement"; fi
grep -q "only valid inside" "$TMP/err" || fail "compiler top-level !# diagnostic"
pass "!#function statements are restricted to function bodies"

cat > "$TMP/log-files.shay" <<'EOF'
$logindxfl <.txt>
$logindxnm "diagnostic"
logit/>"Something went wrong in LN:5
instxt-\"hi\"
------^
Unexpected Symbol"
EOF
(cd "$TMP" && "$SHAII" "$TMP/log-files.shay") || fail "interpreter log output"
printf 'Something went wrong in LN:5\ninstxt-"hi"\n------^\nUnexpected Symbol\n' > "$TMP/expected-log.txt"
cmp "$TMP/expected-log.txt" "$TMP/diagnostic.txt" || fail "multiline interpreter log content"
pass "interpreter writes multiline .txt log files"

(cd "$TMP" && "$SHAII" "$TMP/log-files.shay") || fail "interpreter log append"
cat "$TMP/expected-log.txt" "$TMP/expected-log.txt" > "$TMP/expected-appended.txt"
cmp "$TMP/expected-appended.txt" "$TMP/diagnostic.txt" || fail "interpreter log append behavior"
pass "logit appends without overwriting existing files"

CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/log-files-app" "$TMP/log-files.shay" || fail "compile log-file program"
(cd "$TMP" && "$TMP/log-files-app") || fail "compiled log output"
cat "$TMP/expected-appended.txt" "$TMP/expected-log.txt" > "$TMP/expected-compiled.txt"
cmp "$TMP/expected-compiled.txt" "$TMP/diagnostic.txt" || fail "compiled multiline log content"
pass "gshai generates matching multiline log-file output"

cat > "$TMP/log-shl.shay" <<'EOF'
$logindxfl <.shl>
$logindxnm "debug"
logit/>"Shay debug entry"
EOF
(cd "$TMP" && "$SHAII" "$TMP/log-shl.shay") || fail "interpreter .shl log output"
grep -q '^Shay debug entry$' "$TMP/debug.shl" || fail ".shl extension selection"
pass "logindxfl supports alternate extensions such as .shl"

cat > "$TMP/log-missing-both.shay" <<'EOF'
logit/>"This must fail"
EOF
if "$SHAII" "$TMP/log-missing-both.shay" >"$TMP/out" 2>"$TMP/err"; then fail "interpreter accepted logit without configuration"; fi
grep -q "requires" "$TMP/err" || fail "missing log configuration interpreter diagnostic"
if "$GSHAI" -o "$TMP/log-missing-both" "$TMP/log-missing-both.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted logit without configuration"; fi
grep -q "requires" "$TMP/err" || fail "missing log configuration compiler diagnostic"
pass "logit requires at least one log configuration directive"

cat > "$TMP/log-name-only.shay" <<'EOF'
$logindxnm "only-name"
logit/>"Name-only configuration"
EOF
(cd "$TMP" && "$SHAII" "$TMP/log-name-only.shay") || fail "interpreter filename-only log configuration"
grep -q '^Name-only configuration$' "$TMP/only-name.log" || fail "filename-only default extension"
"$GSHAI" -o "$TMP/log-name-only-app" "$TMP/log-name-only.shay" || fail "compile filename-only log configuration"
(cd "$TMP" && "$TMP/log-name-only-app") || fail "compiled filename-only log configuration"
grep -q '^Name-only configuration$' "$TMP/only-name.log" || fail "compiled filename-only log output"
pass "logindxnm works with the default .log extension"

cat > "$TMP/log-extension-only.shay" <<'EOF'
$logindxfl <.txt>
logit/>"Extension-only configuration"
EOF
(cd "$TMP" && "$SHAII" "$TMP/log-extension-only.shay") || fail "interpreter extension-only log configuration"
grep -q '^Extension-only configuration$' "$TMP/shay.txt" || fail "extension-only default filename"
"$GSHAI" -o "$TMP/log-extension-only-app" "$TMP/log-extension-only.shay" || fail "compile extension-only log configuration"
(cd "$TMP" && "$TMP/log-extension-only-app") || fail "compiled extension-only log configuration"
grep -q '^Extension-only configuration$' "$TMP/shay.txt" || fail "compiled extension-only log output"
pass "logindxfl works with the default shay filename"


cat > "$TMP/mixed-return.shaii" <<'EOF'
$declr useme(x, y)
    ifit x*y = 34
        !#useme
    elseit
        instxt/>"Result:"
        forw x*y
    exec
finish
EOF
cat > "$TMP/mixed-return.shay" <<'EOF'
$shaiiport [mixed-return.shaii]
insval/>useme(5, 6)
ifit !#useme
    instxt/>"Failure branch"
exec
insval/>useme(17, 2)
ifit !#useme
    instxt/>"Failure detected"
exec
EOF
out=$("$SHAII" "$TMP/mixed-return.shay") || fail "interpreter mixed function return"
printf '%s\n' "$out" | grep -q '^Result:$' || fail "interpreter mixed function output"
printf '%s\n' "$out" | grep -q '^30$' || fail "interpreter mixed function return value"
printf '%s\n' "$out" | grep -q '^Failure detected$' || fail "interpreter mixed function failure path"
if printf '%s\n' "$out" | grep -q '^34$'; then fail "interpreter returned value on marked failure"; fi
pass "interpreter supports output, conditional failure, and explicit returns in one function"

CXX="${CXX:-c++}" "$GSHAI" -o "$TMP/mixed-return-app" "$TMP/mixed-return.shay" || fail "compile mixed function return"
out=$("$TMP/mixed-return-app") || fail "run compiled mixed function return"


cat > "$TMP/object-link.shay" <<'EOF'
instxt/>"Linked directly from a .sho object"
EOF
"$GSHAI" -ob "$TMP/object-link.sho" "$TMP/object-link.shay" || fail "compile source into linkable .sho"
"$GSHAI" -o "$TMP/object-link-app" "$TMP/object-link.sho" || fail "link existing .sho into executable"
out=$("$TMP/object-link-app") || fail "run executable linked from .sho"
printf '%s\n' "$out" | grep -q '^Linked directly from a .sho object$' || fail "linked .sho executable output"
pass "gshai links an existing .sho object into a standalone executable"

"$GSHAI" -o "$TMP/object-link.sho" || fail "default output name when linking .sho"
test -x "$TMP/object-link" || fail "default .sho link output is executable"
out=$("$TMP/object-link") || fail "run default .sho-linked executable"
printf '%s\n' "$out" | grep -q '^Linked directly from a .sho object$' || fail "default .sho-linked executable output"
pass "gshai derives an executable name when linking a .sho without an explicit output"




cat > "$TMP/object-link-second.shay" <<'EOF'
instxt/>"Second object contributes code"
EOF
"$GSHAI" -ob "$TMP/object-link-second.sho" "$TMP/object-link-second.shay" || fail "compile second source into .sho"
"$GSHAI" -o "$TMP/multi-object-app" "$TMP/object-link.sho" "$TMP/object-link-second.sho" || fail "link multiple .sho objects"
out=$("$TMP/multi-object-app") || fail "run executable linked from multiple .sho objects"
printf '%s\n' "$out" > "$TMP/multi-object-actual.txt"
printf 'Linked directly from a .sho object\nSecond object contributes code\n' > "$TMP/multi-object-expected.txt"
cmp "$TMP/multi-object-expected.txt" "$TMP/multi-object-actual.txt" || fail "multi-object execution order and output"
pass "gshai links multiple .sho objects and executes their code in input order"

cat > "$TMP/project-first.shay" <<'EOF'
brew project_message = "Shared project variable"
instxt/>"First source executed"
EOF
cat > "$TMP/project-second.shay" <<'EOF'
insval/>project_message
instxt/>"Second source executed"
EOF
"$GSHAI" -o "$TMP/multi-source-app" "$TMP/project-first.shay" "$TMP/project-second.shay" || fail "compile multiple Shay source files"
out=$("$TMP/multi-source-app") || fail "run multiple-source executable"
printf '%s\n' "$out" > "$TMP/multi-source-actual.txt"
printf 'First source executed\nShared project variable\nSecond source executed\n' > "$TMP/multi-source-expected.txt"
cmp "$TMP/multi-source-expected.txt" "$TMP/multi-source-actual.txt" || fail "multi-source execution order and shared variables"
pass "gshai compiles multiple .shay roots into one executable with shared program state"

"$GSHAI" -ob "$TMP/multi-source-project.sho" "$TMP/project-first.shay" "$TMP/project-second.shay" || fail "compile multiple Shay sources into one .sho object"
"$GSHAI" -o "$TMP/multi-source-object-app" "$TMP/multi-source-project.sho" || fail "link multi-source .sho object"
out=$("$TMP/multi-source-object-app") || fail "run multi-source .sho executable"
printf '%s\n' "$out" > "$TMP/multi-source-object-actual.txt"
cmp "$TMP/multi-source-expected.txt" "$TMP/multi-source-object-actual.txt" || fail "multi-source .sho output"
pass "gshai emits one linkable .sho object from multiple source files"


cat > "$TMP/log-variable-settings.shay" <<'EOF'
instxt/>"Log extension (.shl):"
inpvarforw/>cmd
instxt/>"Log filename:"
inpvarforw/>nmin
$logindxfl cmd
$logindxnm nmin
logit/>"Variable-selected log output"
EOF
(cd "$TMP" && printf '.shl\nmy-custom-log\n' | "$SHAII" "$TMP/log-variable-settings.shay") || fail "interpreter variable log settings"
test -f "$TMP/my-custom-log.shl" || fail "interpreter variable-selected log path"
grep -q '^Variable-selected log output$' "$TMP/my-custom-log.shl" || fail "interpreter variable-selected log content"
"$GSHAI" -o "$TMP/log-variable-settings-app" "$TMP/log-variable-settings.shay" || fail "compile variable log settings"
(cd "$TMP" && printf '.txt\ncompiled-custom-log\n' | "$TMP/log-variable-settings-app") || fail "run compiled variable log settings"
test -f "$TMP/compiled-custom-log.txt" || fail "compiled variable-selected log path"
grep -q '^Variable-selected log output$' "$TMP/compiled-custom-log.txt" || fail "compiled variable-selected log content"
pass "logindxfl and logindxnm accept runtime variables in the interpreter and compiler"

cat > "$TMP/log-unknown-variable.shay" <<'EOF'
$logindxnm missing_name
EOF
if "$GSHAI" -o "$TMP/log-unknown-variable" "$TMP/log-unknown-variable.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted unknown log configuration variable"; fi
grep -q "log configuration references unknown variable 'missing_name'" "$TMP/err" || fail "unknown log variable diagnostic"
pass "compiler validates variables used by log configuration directives"

cat > "$TMP/classes.shaii" <<'EOF'
#% Class and method declarations belong in .shaii files.
$declr announce()
    instxt/>"Standalone function executed"
finish

$declr initialize()
    instxt/>"Initialization executed"
finish

$declr startup()
    dofunc initialize
    instxt/>"Startup executed"
finish

$classfunc !library_boot
    $declrmthd boot_notice
        instxt/>"Library class executed"
    finish
finish

$doclass !library_boot

$classfunc !memory
    $declrmthd mem_slot_make
        instxt/>"Memory slot created"
    finish

    $declrmthd mem_slot_delete
        instxt/>"Memory slot deleted"
    finish
finish

dofunc announce
EOF
cat > "$TMP/classes-main.shay" <<'EOF'
#% Runtime calls work in .shay files too.
$shaiiport [classes.shaii]
dofunc startup
$doclass !memory
EOF
out=$("$SHAII" "$TMP/classes-main.shay") || fail "interpreter class and dofunc execution"
printf '%s\n' "$out" | grep -q "Standalone function executed$" || fail "dofunc inside .shaii"
printf '%s\n' "$out" | grep -q "Library class executed$" || fail "doclass inside .shaii"
printf '%s\n' "$out" | grep -q "Initialization executed$" || fail "nested dofunc call"
printf '%s\n' "$out" | grep -q "Startup executed$" || fail "top-level dofunc call"
printf '%s\n' "$out" | grep -q "Memory slot created$" || fail "first class method"
printf '%s\n' "$out" | grep -q "Memory slot deleted$" || fail "second class method"
"$GSHAI" -o "$TMP/classes-app" "$TMP/classes-main.shay" || fail "compile class and dofunc program"
out=$("$TMP/classes-app") || fail "run compiled class and dofunc program"
printf '%s\n' "$out" | grep -q "Standalone function executed$" || fail "compiled dofunc inside .shaii"
printf '%s\n' "$out" | grep -q "Library class executed$" || fail "compiled doclass inside .shaii"
printf '%s\n' "$out" | grep -q "Initialization executed$" || fail "compiled nested dofunc"
printf '%s\n' "$out" | grep -q "Startup executed$" || fail "compiled top-level dofunc"
printf '%s\n' "$out" | grep -q "Memory slot created$" || fail "compiled first class method"
printf '%s\n' "$out" | grep -q "Memory slot deleted$" || fail "compiled second class method"
pass "dofunc works in .shay and .shaii; doclass executes every method in declaration order"

cat > "$TMP/dofunc-arguments.shay" <<'EOF'
dofunc announce()
EOF
if "$SHAII" --check "$TMP/dofunc-arguments.shay" >"$TMP/out" 2>"$TMP/err"; then fail "dofunc accepted arguments"; fi
pass "dofunc rejects argument syntax"

cat > "$TMP/illegal-declr.shay" <<'EOF'
$declr forbidden()
    instxt/>"This must not run"
finish
EOF
if "$SHAII" --check "$TMP/illegal-declr.shay" >"$TMP/out" 2>"$TMP/err"; then fail "\$declr was accepted in .shay"; fi
grep -q "'\$declr' is only allowed in .shaii files" "$TMP/err" || fail "declr file-type restriction diagnostic"
if "$GSHAI" -o "$TMP/illegal-declr" "$TMP/illegal-declr.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted \$declr in .shay"; fi
grep -q "'\$declr' is only allowed in .shaii files" "$TMP/err" || fail "compiler declr file-type restriction diagnostic"
pass "\$declr is restricted to .shaii files"


cat > "$TMP/list-literals.shay" <<'EOF'
brew names = ["Ada", "Grace", "Linus"]
brew scores = [10, 20, -3, true]
brew empty = []
insval/>names
insval/>scores
insval/>empty
EOF
expected=$(printf 'Ada\nGrace\nLinus\n10\n20\n-3\ntrue')
out=$("$SHAII" "$TMP/list-literals.shay") || fail "interpreter list literals"
[ "$out" = "$expected" ] || fail "interpreter list literal output and order"
pass "shaii parses and outputs string, numeric, boolean, and empty lists"

"$GSHAI" -o "$TMP/list-literals-app" "$TMP/list-literals.shay" || fail "compile list literals"
out=$("$TMP/list-literals-app") || fail "run compiled list literals"
[ "$out" = "$expected" ] || fail "compiler list literal output and order"
pass "gshai compiles list literals with matching interpreter output"


cat > "$TMP/list-mutation.shay" <<'EOF'
brew names = ["Ada", "Grace"]
insval/>names[1]
listadd/>names, "Linus"
listset/>names, 0, "Katherine"
listremove/>names, 1
insval/>names
EOF
expected=$(printf 'Grace\nKatherine\nLinus')
out=$("$SHAII" "$TMP/list-mutation.shay") || fail "interpreter list indexing and mutation"
[ "$out" = "$expected" ] || fail "interpreter list indexing and mutation result"
pass "shaii indexes, appends, replaces, and removes list items"

"$GSHAI" -o "$TMP/list-mutation-app" "$TMP/list-mutation.shay" || fail "compile list indexing and mutation"
out=$("$TMP/list-mutation-app") || fail "run compiled list indexing and mutation"
[ "$out" = "$expected" ] || fail "compiler list indexing and mutation result"
pass "gshai compiles list indexing and mutation consistently"

cat > "$TMP/list-bounds.shay" <<'EOF'
brew names = ["Ada"]
insval/>names[1]
EOF
if "$SHAII" "$TMP/list-bounds.shay" >"$TMP/out" 2>"$TMP/err"; then fail "interpreter accepted out-of-range list index"; fi
grep -q "list index 1 is out of range for 'names'" "$TMP/err" || fail "interpreter list bounds diagnostic"
"$GSHAI" -o "$TMP/list-bounds-app" "$TMP/list-bounds.shay" || fail "compile list bounds test"
if "$TMP/list-bounds-app" >"$TMP/out" 2>"$TMP/err"; then fail "compiled app accepted out-of-range list index"; fi
grep -q "list index out of range for 'names'" "$TMP/err" || fail "compiled list bounds diagnostic"
pass "interpreter and compiled programs reject out-of-range list indices"


cat > "$TMP/counter.shui" <<'EOF'
$app "Counter" {
 window main {
  title: "Counter"
  size: 480 x 320
  layout: column
  text heading {
   value: "Counter"
   style: title
  }
  text count {
   bind: counter
  }
  button increment {
   label: "Add one"
   on_click: increment_counter
  }
  text_input name {
   label: "Name"
   bind: user_name
   placeholder: "Your name"
  }
 }
 logic {
  brew counter = 0
  $declr increment_counter()
   counter = counter + 1
  finish
 }
}
EOF
"$SHUI" validate "$TMP/counter.shui" >"$TMP/out" 2>"$TMP/err" || { cat "$TMP/out" "$TMP/err" >&2; fail ".shui validation"; }
grep -q 'is valid' "$TMP/out" || fail ".shui validation success message"
"$SHUI" build "$TMP/counter.shui" --target web -o "$TMP/counter.html" >"$TMP/out" 2>"$TMP/err" || { cat "$TMP/out" "$TMP/err" >&2; fail ".shui browser build"; }
grep -q '<main id="main"' "$TMP/counter.html" || fail "browser renderer window root"
grep -q 'data-shay-action="increment_counter"' "$TMP/counter.html" || fail "browser renderer button action binding"
grep -q 'data-shay-bind="counter"' "$TMP/counter.html" || fail "browser renderer text binding"
grep -q "shay:action" "$TMP/counter.html" || fail "browser renderer event bridge"
pass "shui validates hybrid UI files and renders browser HTML with Shay event hooks"

cat > "$TMP/invalid.shui" <<'EOF'
$app "Invalid" {
 window main {
  button broken {
   label: "No handler"
  }
 }
}
EOF
if "$SHUI" validate "$TMP/invalid.shui" >"$TMP/out" 2>"$TMP/err"; then fail "shui accepted a button without an action handler"; fi
grep -q "requires 'on_click'" "$TMP/err" || fail "shui button validation diagnostic"
pass "shui reports invalid UI elements with source locations"

if "$SHUI" run "$ROOT/examples/counter.shui" --target desktop >"$TMP/out" 2>"$TMP/err"; then
    fail "non-Windows shui unexpectedly launched the desktop backend"
fi
grep -q "desktop target requires Windows" "$TMP/err" || fail "shui desktop platform diagnostic"
pass "shui clearly gates the native desktop renderer to Windows"



cat > "$TMP/maps-sets.shay" <<'EOF'
brew user = {"name": "Ada", "role": "Compiler"}
insval/>user["name"]
mapset/>user, "role", "Language designer"
mapremove/>user, "name"
insval/>user
brew tags = set["systems", "compiler", "systems"]
setadd/>tags, "tools"
setremove/>tags, "systems"
setcontains/>tags, "compiler"
setcontains/>tags, "systems"
insval/>tags
EOF
expected=$(printf 'Ada\nrole = Language designer\ntrue\nfalse\ncompiler\ntools')
out=$("$SHAII" "$TMP/maps-sets.shay") || fail "interpreter maps and sets"
[ "$out" = "$expected" ] || fail "interpreter map/set output and mutation"
pass "shaii creates, indexes, updates, removes, and queries maps and sets"

"$GSHAI" -o "$TMP/maps-sets-app" "$TMP/maps-sets.shay" || fail "compile maps and sets"
out=$("$TMP/maps-sets-app") || fail "run compiled maps and sets"
[ "$out" = "$expected" ] || fail "compiler map/set output and mutation"
pass "gshai compiles maps and sets consistently with the interpreter"

cat > "$TMP/map-missing.shay" <<'EOF'
brew user = {"name": "Ada"}
insval/>user["missing"]
EOF
if "$SHAII" "$TMP/map-missing.shay" >"$TMP/out" 2>"$TMP/err"; then fail "interpreter accepted a missing map key"; fi
grep -q "unknown map key 'missing'" "$TMP/err" || fail "map missing-key diagnostic"
pass "map key lookup reports missing keys"


cat > "$TMP/typed-values.shay" <<'EOF'
brew age: number = -12
brew name: string = "Ada"
brew enabled: boolean = true
insval/>age
insval/>name
insval/>enabled
EOF
expected=$(printf '%s\n' '-12' 'Ada' 'true')
out=$("$SHAII" "$TMP/typed-values.shay") || fail "interpreter optional scalar types"
[ "$out" = "$expected" ] || fail "interpreter typed scalar output"
"$GSHAI" -o "$TMP/typed-values-app" "$TMP/typed-values.shay" || fail "compile optional scalar types"
out=$("$TMP/typed-values-app") || fail "run compiled optional scalar types"
[ "$out" = "$expected" ] || fail "compiler typed scalar output"
pass "optional number, string, and boolean annotations preserve runtime values"

cat > "$TMP/type-mismatch.shay" <<'EOF'
brew age: number = "twelve"
EOF
if "$SHAII" "$TMP/type-mismatch.shay" >"$TMP/out" 2>"$TMP/err"; then fail "interpreter accepted a typed initializer mismatch"; fi
grep -q "type mismatch for 'age': expected number, got string" "$TMP/err" || fail "typed initializer mismatch diagnostic"
if "$GSHAI" -o "$TMP/type-mismatch-app" "$TMP/type-mismatch.shay" >"$TMP/out" 2>"$TMP/err"; then fail "compiler accepted a typed initializer mismatch"; fi
grep -q "type mismatch for 'age': expected number, got string" "$TMP/err" || fail "compiler typed initializer mismatch diagnostic"
pass "interpreter and compiler reject incompatible typed initializers"


cat > "$TMP/try-catch.shay" <<'EOF'
tryit
    throwit/>validation, "bad input"
catchit problem
    insval/>problem
endtry
tryit
    tryit
        throwit/>inner, "first failure"
    catchit inner_problem
        throwit/>outer, "second failure"
    endtry
catchit outer_problem
    insval/>outer_problem
endtry
EOF
expected=$(printf 'validation: bad input\nouter: second failure')
out=$("$SHAII" "$TMP/try-catch.shay") || fail "interpreter structured try/catch"
[ "$out" = "$expected" ] || fail "interpreter structured error values"
"$GSHAI" -o "$TMP/try-catch-app" "$TMP/try-catch.shay" || fail "compile structured try/catch"
out=$("$TMP/try-catch-app") || fail "run compiled structured try/catch"
[ "$out" = "$expected" ] || fail "compiler structured error values"
pass "tryit/catchit handles categorized errors and nested catches in both runtimes"

cat > "$TMP/unhandled-error.shay" <<'EOF'
throwit/>io, "file missing"
EOF
if "$SHAII" "$TMP/unhandled-error.shay" >"$TMP/out" 2>"$TMP/err"; then fail "interpreter accepted an unhandled structured error"; fi
grep -q "io: file missing" "$TMP/err" || fail "interpreter unhandled error diagnostic"
"$GSHAI" -o "$TMP/unhandled-error-app" "$TMP/unhandled-error.shay" || fail "compile unhandled structured error"
if "$TMP/unhandled-error-app" >"$TMP/out" 2>"$TMP/err"; then fail "compiled app accepted an unhandled structured error"; fi
grep -q "io: file missing" "$TMP/err" || fail "compiler unhandled error diagnostic"
pass "unhandled structured errors preserve their category and message"

printf '\nAll Shay toolchain tests passed.\n'
