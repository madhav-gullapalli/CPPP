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

if [[ -n "${PYTHON_CMD:-}" ]]; then
    :
elif command -v python3 >/dev/null 2>&1; then
    PYTHON_CMD="python3"
elif command -v python >/dev/null 2>&1; then
    PYTHON_CMD="python"
elif command -v python.exe >/dev/null 2>&1; then
    PYTHON_CMD="python.exe"
else
    echo "Could not find a usable Python interpreter for tests/errors_coverage.py" >&2
    exit 1
fi

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

TOTAL_STEPS=24
CURRENT_STEP=0

progress() {
    local label="$1"
    CURRENT_STEP=$((CURRENT_STEP + 1))
    local width=28
    local filled=$((CURRENT_STEP * width / TOTAL_STEPS))
    local empty=$((width - filled))
    local bar
    bar="$(printf '%*s' "$filled" '' | tr ' ' '#')$(printf '%*s' "$empty" '' | tr ' ' '-')"
    echo "[${CURRENT_STEP}/${TOTAL_STEPS}] [${bar}] ${label}"
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

run_program_ok() {
    local source="$1"
    local log="$2"
    local label="$3"

    if ! "$COMPILER" --cppp "$source" --run >"$log" 2>&1; then
        cat "$log" >&2
        fail "$label"
    fi
}

run_runtime_error() {
    local source="$1"
    local log="$2"
    local label="$3"

    if "$COMPILER" --cppp "$source" --run >"$log" 2>&1; then
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

progress "primitive declarations"
primitives_case="$(stage_case "$TEST_DIR/primitives.cppp")"
run_compile_ok "$primitives_case" "$LOG_DIR/primitives.log" "primitive declarations compile"
assert_contains "${primitives_case%.cppp}.cpp" "long long a = 3;" "int lowers to long long"
assert_contains "${primitives_case%.cppp}.cpp" "long double b = 2.5L;" "float literal gets long-double suffix"
assert_contains "${primitives_case%.cppp}.cpp" "CPPPChar c = CPPPChar('x');" "char lowers to CPPPChar"
assert_contains "${primitives_case%.cppp}.cpp" "bool ok = true;" "bool lowers to bool"
pass "primitive declarations still compile and lower correctly"

progress "primitive generic misuse"
bad_generic_case="$(stage_case "$TEST_DIR/bad_generic.cppp")"
run_failure "$bad_generic_case" "$LOG_DIR/bad_generic.log" "int<int> should fail"
assert_contains "$LOG_DIR/bad_generic.log" "int expects 0 subtypes" "bad primitive generic diagnostic"
pass "primitive generic misuse is rejected"

progress "submit pruning int-only"
int_only_case="$(stage_case "$TEST_DIR/int_only.cppp")"
run_submit_ok "$int_only_case" "$LOG_DIR/int_only.log" "int-only submit builds"
assert_contains "${int_only_case%.cppp}.cpp" "CPPPInputInt" "submit keeps int input helper"
assert_not_contains "${int_only_case%.cppp}.cpp" "struct CPPPChar" "submit prunes char core for int-only program"
assert_not_contains "${int_only_case%.cppp}.cpp" "CPPPInputChar" "submit prunes char input helper"
assert_not_contains "${int_only_case%.cppp}.cpp" "CPPPInputFloat" "submit prunes float input helper"
assert_not_contains "${int_only_case%.cppp}.cpp" "CPPPInputBool" "submit prunes bool input helper"
pass "submit mode prunes unused helpers for int-only program"

progress "submit helper dependencies"
char_used_case="$(stage_case "$TEST_DIR/char_used.cppp")"
run_submit_ok "$char_used_case" "$LOG_DIR/char_used.log" "char submit builds"
assert_contains "${char_used_case%.cppp}.cpp" "struct CPPPChar" "submit keeps char core when char is used"
assert_contains "${char_used_case%.cppp}.cpp" "CPPPInputChar" "submit keeps char input helper"
assert_not_contains "${char_used_case%.cppp}.cpp" "CPPPInputFloat" "submit prunes unrelated float helper"
pass "submit mode keeps helper dependencies"

progress "typed bool helper pruning"
int_to_bool_case="$(stage_case "$TEST_DIR/int_to_bool.cppp")"
run_submit_ok "$int_to_bool_case" "$LOG_DIR/int_to_bool.log" "int-to-bool submit builds"
assert_contains "${int_to_bool_case%.cppp}.cpp" "CPPPToBoolInt" "submit keeps int-to-bool helper"
assert_not_contains "${int_to_bool_case%.cppp}.cpp" "CPPPToBoolFloat" "submit prunes unused float-to-bool helper"
assert_not_contains "${int_to_bool_case%.cppp}.cpp" "CPPPToBoolChar" "submit prunes unused char-to-bool helper"
pass "submit mode keeps only required typed bool helper"

progress "submit dead-code pruning"
submit_dead_code_case="$(stage_case "$TEST_DIR/submit_dead_code.cppp")"
run_submit_ok "$submit_dead_code_case" "$LOG_DIR/submit_dead_code.log" "submit dead-code case builds"
assert_contains "${submit_dead_code_case%.cppp}.cpp" "long long keep()" "submit keeps the used method"
assert_not_contains "${submit_dead_code_case%.cppp}.cpp" "long long discard()" "submit prunes an unused sibling method"
assert_not_contains "${submit_dead_code_case%.cppp}.cpp" "struct Unused" "submit prunes an unused struct"
assert_not_contains "${submit_dead_code_case%.cppp}.cpp" "unusedFunction" "submit prunes an unreachable function"
assert_contains "${submit_dead_code_case%.cppp}.cpp" "class CPPPPair {" "submit keeps the used Pair runtime class"
assert_contains "${submit_dead_code_case%.cppp}.cpp" "class CPPPList {" "submit keeps the used List runtime class"
assert_not_contains "${submit_dead_code_case%.cppp}.cpp" "class CPPPSet {" "submit prunes the unused Set runtime class"
assert_not_contains "${submit_dead_code_case%.cppp}.cpp" "class CPPPMap {" "submit prunes the unused Map runtime class"
pass "submit mode prunes unused functions, structs, methods, and runtime classes"

progress "list literal truthiness"
list_truthiness_case="$(stage_case "$TEST_DIR/list_truthiness.cppp")"
run_submit_ok "$list_truthiness_case" "$LOG_DIR/list_truthiness.log" "list truthiness submit builds"
assert_contains "${list_truthiness_case%.cppp}.cpp" "if ((!(even).empty()))" "named list truthiness lowers to emptiness check"
assert_contains "${list_truthiness_case%.cppp}.cpp" "if ((!(CPPPList<long long>{1}).empty()))" "list literal truthiness lowers inline"
assert_contains "${list_truthiness_case%.cppp}.cpp" "CPPPList<CPPPList<long long>> grid = CPPPList<CPPPList<long long>>{CPPPList<long long>{1, 2}, CPPPList<long long>{3}};" "nested list literal lowers correctly"
pass "list literals and truthiness regressions are covered"

progress "list literal mismatch"
list_literal_error_case="$(stage_case "$TEST_DIR/list_literal_error.cppp")"
run_failure "$list_literal_error_case" "$LOG_DIR/list_literal_error.log" "mismatched list literal should fail"
assert_contains "$LOG_DIR/list_literal_error.log" "cannot implicitly convert float to int in list literal" "list literal type diagnostic"
pass "list literal type mismatch is rejected"

progress "expression behavior"
expression_behavior_case="$(stage_case "$TEST_DIR/expression_behavior.cppp")"
run_program_ok "$expression_behavior_case" "$LOG_DIR/expression_behavior.log" "expression behavior program runs"
assert_contains "$LOG_DIR/expression_behavior.log" "14 20 9 30 2" "expression behavior output"
pass "expression precedence, variables, calls, and nested indexing are preserved"

progress "expression type errors"
expression_error_case="$(stage_case "$TEST_DIR/expression_error.cppp")"
run_failure "$expression_error_case" "$LOG_DIR/expression_error.log" "invalid expression types should fail"
assert_contains "$LOG_DIR/expression_error.log" "cannot use '&' with float and int" "invalid expression type diagnostic"
pass "invalid expression type errors are preserved"

progress "list remove behavior"
list_remove_case="$(stage_case "$TEST_DIR/list_remove.cppp")"
run_program_ok "$list_remove_case" "$LOG_DIR/list_remove.log" "list remove program runs"
assert_contains "$LOG_DIR/list_remove.log" "1 2" "list remove output"
pass "list remove behavior is preserved"

progress "list remove invalid calls"
list_remove_error_case="$(stage_case "$TEST_DIR/list_remove_error.cppp")"
run_failure "$list_remove_error_case" "$LOG_DIR/list_remove_error.log" "invalid list remove call should fail"
assert_contains "$LOG_DIR/list_remove_error.log" "remove() expects no arguments or index" "list remove arity diagnostic"
pass "invalid remove() calls are rejected"

progress "list remove runtime errors"
list_remove_runtime_error_case="$(stage_case "$TEST_DIR/list_remove_runtime_error.cppp")"
run_runtime_error "$list_remove_runtime_error_case" "$LOG_DIR/list_remove_runtime_error.log" "empty list remove reports runtime error"
assert_contains "$LOG_DIR/list_remove_runtime_error.log" "runtime error: cannot remove from empty list" "list remove runtime diagnostic"
pass "remove() runtime errors are preserved"

progress "list slicing"
list_slice_case="$(stage_case "$TEST_DIR/list_slice.cppp")"
run_program_ok "$list_slice_case" "$LOG_DIR/list_slice.log" "list slicing program runs"
assert_contains "$LOG_DIR/list_slice.log" "5" "negative list index output"
assert_contains "$LOG_DIR/list_slice.log" "[2, 3, 4]" "list slice output"
assert_contains "$LOG_DIR/list_slice.log" "[]" "empty slice output"
pass "list slicing and negative indexing are preserved"

progress "negative index runtime errors"
list_slice_error_case="$(stage_case "$TEST_DIR/list_slice_error.cppp")"
run_runtime_error "$list_slice_error_case" "$LOG_DIR/list_slice_error.log" "out of range negative index reports runtime error"
assert_contains "$LOG_DIR/list_slice_error.log" "runtime error: invalid list index" "list slice runtime diagnostic"
pass "negative index runtime errors are preserved"

progress "sublist membership"
list_sublist_case="$(stage_case "$TEST_DIR/list_sublist.cppp")"
run_program_ok "$list_sublist_case" "$LOG_DIR/list_sublist.log" "sublist membership program runs"
assert_contains "$LOG_DIR/list_sublist.log" $'1\n1\n0\n0' "sublist membership output"
pass "scalar and sublist membership are preserved"

progress "invalid in-operator rhs"
list_sublist_error_case="$(stage_case "$TEST_DIR/list_sublist_error.cppp")"
run_failure "$list_sublist_error_case" "$LOG_DIR/list_sublist_error.log" "non-list right operand for in should fail"
assert_contains "$LOG_DIR/list_sublist_error.log" "right side of 'in' must be a List" "in operator right-side diagnostic"
pass "invalid in-operator right side is rejected"

progress "nested list membership"
nested_list_in_case="$(stage_case "$TEST_DIR/nested_list_in.cppp")"
run_program_ok "$nested_list_in_case" "$LOG_DIR/nested_list_in.log" "nested list membership program runs"
assert_contains "$LOG_DIR/nested_list_in.log" "1" "nested list membership output"
pass "list membership against List<List<T>> is preserved"

progress "nested indexed lvalues"
exotic_lvalues_case="$(stage_case "$TEST_DIR/exotic_lvalues.cppp")"
run_program_ok "$exotic_lvalues_case" "$LOG_DIR/exotic_lvalues.log" "nested indexed lvalue program runs"
assert_contains "$LOG_DIR/exotic_lvalues.log" "[[4, 7], [4, 1]]" "nested indexed lvalue output"
pass "nested indexed lvalues behave like ordinary mutable variables"

progress "nested indexed input"
exotic_lvalues_input_case="$(stage_case "$TEST_DIR/exotic_lvalues_input.cppp")"
printf '9\n' | "$COMPILER" --cppp "$exotic_lvalues_input_case" --run >"$LOG_DIR/exotic_lvalues_input.log" 2>&1 || fail "nested indexed input program runs"
assert_contains "$LOG_DIR/exotic_lvalues_input.log" "[[1, 9], [3, 4]]" "nested indexed input output"
pass "input() assigns directly into nested indexed lvalues"

progress "nested indexed diagnostics"
exotic_lvalues_error_case="$(stage_case "$TEST_DIR/exotic_lvalues_error.cppp")"
run_failure "$exotic_lvalues_error_case" "$LOG_DIR/exotic_lvalues_error.log" "invalid nested indexed lvalue should fail"
assert_contains "$LOG_DIR/exotic_lvalues_error.log" "list index must be int" "nested indexed lvalue diagnostic"
pass "nested indexed lvalues preserve index type diagnostics"

catalog_failed=0

progress "correct.txt catalog coverage"
if "$PYTHON_CMD" tests/errors_coverage.py correct.txt; then
    pass "correct.txt documented examples are covered"
else
    catalog_failed=1
fi

progress "errors.txt catalog coverage"
if "$PYTHON_CMD" tests/errors_coverage.py errors.txt; then
    pass "errors.txt documented examples are covered"
else
    catalog_failed=1
fi

if [[ "$catalog_failed" -ne 0 ]]; then
    fail "documented example coverage"
fi

echo
echo "All CP++ regression tests passed."
