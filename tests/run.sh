#!/usr/bin/env sh
set -eu
SHAII="${1:-./build/shaii}"
GSHAI="${2:-./build/gshai}"
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
case "$SHAII" in /*) ;; *) SHAII="$ROOT/$SHAII";; esac
case "$GSHAI" in /*) ;; *) GSHAI="$ROOT/$GSHAI";; esac
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
instxt-"hi"
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

CXX="$CXX" "$GSHAI" -o "$TMP/log-files-app" "$TMP/log-files.shay" || fail "compile log-file program"
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
printf '%s\n' "$out" | grep -q '^Result:$' || fail "compiler mixed function output"
printf '%s\n' "$out" | grep -q '^30$' || fail "compiler mixed function return value"
printf '%s\n' "$out" | grep -q '^Failure detected$' || fail "compiler mixed function failure path"
if printf '%s\n' "$out" | grep -q '^34$'; then fail "compiler returned value on marked failure"; fi
pass "compiler supports output, conditional failure, and explicit returns in one function"
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
printf '\nAll Shay toolchain tests passed.\n'

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
printf '%s\n' "$out" | grep -q '^Result:$' || fail "compiler mixed function output"
printf '%s\n' "$out" | grep -q '^30$' || fail "compiler mixed function return value"
printf '%s\n' "$out" | grep -q '^Failure detected$' || fail "compiler mixed function failure path"
if printf '%s\n' "$out" | grep -q '^34$'; then fail "compiler returned value on marked failure"; fi
pass "compiler supports output, conditional failure, and explicit returns in one function"
 "$TMP/debug.shl" || fail ".shl extension selection"
pass "logindxfl supports alternate extensions such as .shl"

printf '\nAll Shay toolchain tests passed.\n'

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
printf '%s\n' "$out" | grep -q '^Result:$' || fail "compiler mixed function output"
printf '%s\n' "$out" | grep -q '^30$' || fail "compiler mixed function return value"
printf '%s\n' "$out" | grep -q '^Failure detected$' || fail "compiler mixed function failure path"
if printf '%s\n' "$out" | grep -q '^34$'; then fail "compiler returned value on marked failure"; fi
pass "compiler supports output, conditional failure, and explicit returns in one function"
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
printf '\nAll Shay toolchain tests passed.\n'

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
printf '%s\n' "$out" | grep -q '^Result:$' || fail "compiler mixed function output"
printf '%s\n' "$out" | grep -q '^30$' || fail "compiler mixed function return value"
printf '%s\n' "$out" | grep -q '^Failure detected$' || fail "compiler mixed function failure path"
if printf '%s\n' "$out" | grep -q '^34$'; then fail "compiler returned value on marked failure"; fi
pass "compiler supports output, conditional failure, and explicit returns in one function"
