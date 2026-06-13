#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

MAKE_CMD="${MAKE_CMD:-make}"
TEST_DIR="tests/regression"
TMP_DIR="tests/tmp"
LOG_DIR="$TMP_DIR/logs"
CASE_DIR="$TMP_DIR/cases"
COMPILER="./build/cppp"

UNAME_S="$(uname -s 2>/dev/null || true)"
if [[ "${OS:-}" == "Windows_NT" || "$UNAME_S" == MINGW* || "$UNAME_S" == MSYS* ]]; then
    COMPILER="./build/cppp.exe"
fi

rm -rf "$TMP_DIR"
mkdir -p "$LOG_DIR"
mkdir -p "$CASE_DIR"

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

    if ! grep -Fq "$needle" "$file"; then
        echo "Expected to find: $needle" >&2
        echo "In file: $file" >&2
        echo "---- file contents ----" >&2
        cat "$file" >&2 || true
        echo "-----------------------" >&2
        fail "$label"
    fi
}

assert_not_contains() {
    local file="$1"
    local needle="$2"
    local label="$3"

    if grep -Fq "$needle" "$file"; then
        echo "Did not expect to find: $needle" >&2
        echo "In file: $file" >&2
        echo "---- file contents ----" >&2
        cat "$file" >&2 || true
        echo "-----------------------" >&2
        fail "$label"
    fi
}

run_compile_ok() {
    local source="$1"
    local log="$2"
    local label="$3"

    if ! "$COMPILER" --cppp "$source" --compile >"$log" 2>&1; then
        cat "$log" >&2
        fail "$label"
    fi
}

run_submit_ok() {
    local source="$1"
    local log="$2"
    local label="$3"

    if ! "$COMPILER" --cppp "$source" --submit >"$log" 2>&1; then
        cat "$log" >&2
        fail "$label"
    fi
}

run_failure() {
    local source="$1"
    local log="$2"
    local label="$3"

    if "$COMPILER" --cppp "$source" >"$log" 2>&1; then
        cat "$log" >&2
        fail "$label"
    fi
}

stage_case() {
    local fixture="$1"
    local staged="$CASE_DIR/$(basename "$fixture")"
    cp "$fixture" "$staged"
    printf '%s\n' "$staged"
}

echo "Building CP++ compiler..."
"$MAKE_CMD" >/dev/null

primitives_case="$(stage_case "$TEST_DIR/primitives.cppp")"
run_compile_ok "$primitives_case" "$LOG_DIR/primitives.log" "primitive declarations compile"
assert_contains "${primitives_case%.cppp}.cpp" "long long a = 3;" "int lowers to long long"
assert_contains "${primitives_case%.cppp}.cpp" "long double b = 2.5L;" "float literal gets long-double suffix"
assert_contains "${primitives_case%.cppp}.cpp" "CPPPChar c = CPPPChar('x');" "char lowers to CPPPChar"
assert_contains "${primitives_case%.cppp}.cpp" "bool ok = true;" "bool lowers to bool"
pass "primitive declarations still compile and lower correctly"

bad_generic_case="$(stage_case "$TEST_DIR/bad_generic.cppp")"
run_failure "$bad_generic_case" "$LOG_DIR/bad_generic.log" "int<int> should fail"
assert_contains "$LOG_DIR/bad_generic.log" "int expects 0 subtypes" "bad primitive generic diagnostic"
pass "primitive generic misuse is rejected"

int_only_case="$(stage_case "$TEST_DIR/int_only.cppp")"
run_submit_ok "$int_only_case" "$LOG_DIR/int_only.log" "int-only submit builds"
assert_contains "${int_only_case%.cppp}.cpp" "CPPPInputInt" "submit keeps int input helper"
assert_not_contains "${int_only_case%.cppp}.cpp" "struct CPPPChar" "submit prunes char core for int-only program"
assert_not_contains "${int_only_case%.cppp}.cpp" "CPPPInputChar" "submit prunes char input helper"
assert_not_contains "${int_only_case%.cppp}.cpp" "CPPPInputFloat" "submit prunes float input helper"
assert_not_contains "${int_only_case%.cppp}.cpp" "CPPPInputBool" "submit prunes bool input helper"
pass "submit mode prunes unused helpers for int-only program"

char_used_case="$(stage_case "$TEST_DIR/char_used.cppp")"
run_submit_ok "$char_used_case" "$LOG_DIR/char_used.log" "char submit builds"
assert_contains "${char_used_case%.cppp}.cpp" "struct CPPPChar" "submit keeps char core when char is used"
assert_contains "${char_used_case%.cppp}.cpp" "CPPPInputChar" "submit keeps char input helper"
assert_not_contains "${char_used_case%.cppp}.cpp" "CPPPInputFloat" "submit prunes unrelated float helper"
pass "submit mode keeps helper dependencies"

int_to_bool_case="$(stage_case "$TEST_DIR/int_to_bool.cppp")"
run_submit_ok "$int_to_bool_case" "$LOG_DIR/int_to_bool.log" "int-to-bool submit builds"
assert_contains "${int_to_bool_case%.cppp}.cpp" "CPPPToBoolInt" "submit keeps int-to-bool helper"
assert_not_contains "${int_to_bool_case%.cppp}.cpp" "CPPPToBoolFloat" "submit prunes unused float-to-bool helper"
assert_not_contains "${int_to_bool_case%.cppp}.cpp" "CPPPToBoolChar" "submit prunes unused char-to-bool helper"
pass "submit mode keeps only required typed bool helper"

list_truthiness_case="$(stage_case "$TEST_DIR/list_truthiness.cppp")"
run_submit_ok "$list_truthiness_case" "$LOG_DIR/list_truthiness.log" "list truthiness submit builds"
assert_contains "${list_truthiness_case%.cppp}.cpp" "if ((!(even).empty()))" "named list truthiness lowers to emptiness check"
assert_contains "${list_truthiness_case%.cppp}.cpp" "if ((!(vector<long long>{1}).empty()))" "list literal truthiness lowers inline"
assert_contains "${list_truthiness_case%.cppp}.cpp" "vector<vector<long long>> grid = vector<vector<long long>>{vector<long long>{1, 2}, vector<long long>{3}};" "nested list literal lowers correctly"
pass "list literals and truthiness regressions are covered"

list_literal_error_case="$(stage_case "$TEST_DIR/list_literal_error.cppp")"
run_failure "$list_literal_error_case" "$LOG_DIR/list_literal_error.log" "mismatched list literal should fail"
assert_contains "$LOG_DIR/list_literal_error.log" "cannot implicitly convert float to int in list literal" "list literal type diagnostic"
pass "list literal type mismatch is rejected"

list_remove_case="$(stage_case "$TEST_DIR/list_remove.cppp")"
run_submit_ok "$list_remove_case" "$LOG_DIR/list_remove.log" "list remove submit builds"
assert_contains "${list_remove_case%.cppp}.cpp" "values.pop_back();" "submit remove-from-end stays compact"
assert_contains "${list_remove_case%.cppp}.cpp" "values.erase(values.begin() + 0);" "submit remove-at stays compact"
pass "list remove regression is covered"

list_remove_error_case="$(stage_case "$TEST_DIR/list_remove_error.cppp")"
run_failure "$list_remove_error_case" "$LOG_DIR/list_remove_error.log" "bad remove arity should fail"
assert_contains "$LOG_DIR/list_remove_error.log" "remove() expects no arguments or index" "compile-time remove arity diagnostic"
pass "list remove misuse is rejected"

list_remove_runtime_case="$(stage_case "$TEST_DIR/list_remove_runtime_error.cppp")"
if "$COMPILER" --cppp "$list_remove_runtime_case" --run >"$LOG_DIR/list_remove_runtime_error.log" 2>&1; then
    cat "$LOG_DIR/list_remove_runtime_error.log" >&2
    fail "empty list remove should fail at runtime"
fi
assert_contains "$LOG_DIR/list_remove_runtime_error.log" "runtime error: cannot remove from empty list" "runtime empty-remove diagnostic"
pass "empty list remove reports a runtime error"
echo
echo "All CP++ regression tests passed."
