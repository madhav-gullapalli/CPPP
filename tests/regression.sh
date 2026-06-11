#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

MAKE_CMD="${MAKE_CMD:-make}"
COMPILER="./build/cppp"

UNAME_S="$(uname -s 2>/dev/null || true)"
if [[ "${OS:-}" == "Windows_NT" || "$UNAME_S" == MINGW* || "$UNAME_S" == MSYS* ]]; then
COMPILER="./build/cppp.exe"
fi

TMP_DIR="tests/tmp"
rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

pass() {
echo "[PASS] $1"
}

fail() {
echo "[FAIL] $1" >&2
exit 1
}

assert_contains() {
local file="$1"
local needle="$2"
local label="$3"

```
if ! grep -Fq "$needle" "$file"; then
    echo "Expected to find: $needle" >&2
    echo "In file: $file" >&2
    echo "---- file contents ----" >&2
    cat "$file" >&2 || true
    echo "-----------------------" >&2
    fail "$label"
fi
```

}

assert_not_contains() {
local file="$1"
local needle="$2"
local label="$3"

```
if grep -Fq "$needle" "$file"; then
    echo "Did not expect to find: $needle" >&2
    echo "In file: $file" >&2
    echo "---- file contents ----" >&2
    cat "$file" >&2 || true
    echo "-----------------------" >&2
    fail "$label"
fi
```

}

echo "Building CP++ compiler..."
$MAKE_CMD >/dev/null

# ------------------------------------------------------------

# 1. Primitive declarations still compile after type-tree refactor

# ------------------------------------------------------------

cat > "$TMP_DIR/primitives.cppp" <<'CPPP'
int a = 3;
float b = 2.5;
char c = 'x';
bool ok = true;
print(a);
print(b);
print(c);
print(ok);
CPPP

if ! "$COMPILER" --cppp "$TMP_DIR/primitives.cppp" --compile >/tmp/cppp_test_primitives.log 2>&1; then
cat /tmp/cppp_test_primitives.log >&2
fail "primitive declarations compile"
fi

assert_contains "$TMP_DIR/primitives.cpp" "long long a = 3;" "int lowers to long long"
assert_contains "$TMP_DIR/primitives.cpp" "long double b = 2.5L;" "float literal gets long-double suffix"
assert_contains "$TMP_DIR/primitives.cpp" "CPPPChar c = CPPPChar('x');" "char lowers to CPPPChar"
assert_contains "$TMP_DIR/primitives.cpp" "bool ok = true;" "bool lowers to bool"

pass "primitive declarations still compile and lower correctly"

# ------------------------------------------------------------

# 2. Malformed generic syntax is rejected for current primitives

# ------------------------------------------------------------

cat > "$TMP_DIR/bad_generic.cppp" <<'CPPP'
int<int> x;
CPPP

if "$COMPILER" --cppp "$TMP_DIR/bad_generic.cppp" >/tmp/cppp_test_bad_generic.log 2>&1; then
cat /tmp/cppp_test_bad_generic.log >&2
fail "int<int> should fail"
fi

assert_contains /tmp/cppp_test_bad_generic.log "int expects 0 subtypes" "bad primitive generic diagnostic"
pass "primitive generic misuse is rejected"

# ------------------------------------------------------------

# 3. Submit mode prunes unused helpers for int-only program

# ------------------------------------------------------------

cat > "$TMP_DIR/int_only.cppp" <<'CPPP'
int x = input();
print(x);
CPPP

if ! "$COMPILER" --cppp "$TMP_DIR/int_only.cppp" --submit >/tmp/cppp_test_int_only.log 2>&1; then
cat /tmp/cppp_test_int_only.log >&2
fail "int-only submit builds"
fi

assert_contains "$TMP_DIR/int_only.cpp" "CPPPInputInt" "submit keeps int input helper"
assert_not_contains "$TMP_DIR/int_only.cpp" "struct CPPPChar" "submit prunes char core for int-only program"
assert_not_contains "$TMP_DIR/int_only.cpp" "CPPPInputChar" "submit prunes char input helper"
assert_not_contains "$TMP_DIR/int_only.cpp" "CPPPInputFloat" "submit prunes float input helper"
assert_not_contains "$TMP_DIR/int_only.cpp" "CPPPInputBool" "submit prunes bool input helper"

pass "submit mode prunes unused helpers for int-only program"

# ------------------------------------------------------------

# 4. Submit mode keeps char helper dependency when char is used

# ------------------------------------------------------------

cat > "$TMP_DIR/char_used.cppp" <<'CPPP'
char c = input();
print(c);
CPPP

if ! "$COMPILER" --cppp "$TMP_DIR/char_used.cppp" --submit >/tmp/cppp_test_char_used.log 2>&1; then
cat /tmp/cppp_test_char_used.log >&2
fail "char submit builds"
fi

assert_contains "$TMP_DIR/char_used.cpp" "struct CPPPChar" "submit keeps char core when char is used"
assert_contains "$TMP_DIR/char_used.cpp" "CPPPInputChar" "submit keeps char input helper"
assert_not_contains "$TMP_DIR/char_used.cpp" "CPPPInputFloat" "submit prunes unrelated float helper"

pass "submit mode keeps helper dependencies"

# ------------------------------------------------------------

# 5. Bool cast helpers are specialized after pruning

# ------------------------------------------------------------

cat > "$TMP_DIR/int_to_bool.cppp" <<'CPPP'
int x = input();
bool ok = (bool)x;
print(ok);
CPPP

if ! "$COMPILER" --cppp "$TMP_DIR/int_to_bool.cppp" --submit >/tmp/cppp_test_int_to_bool.log 2>&1; then
cat /tmp/cppp_test_int_to_bool.log >&2
fail "int-to-bool submit builds"
fi

assert_contains "$TMP_DIR/int_to_bool.cpp" "CPPPToBoolInt" "submit keeps int-to-bool helper"
assert_not_contains "$TMP_DIR/int_to_bool.cpp" "CPPPToBoolFloat" "submit prunes unused float-to-bool helper"
assert_not_contains "$TMP_DIR/int_to_bool.cpp" "CPPPToBoolChar" "submit prunes unused char-to-bool helper"

pass "submit mode keeps only required typed bool helper"

echo
echo "All CP++ regression tests passed."
