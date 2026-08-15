#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

MAKE_CMD="${MAKE_CMD:-make}"
TEST_DIR="tests/regression"
TMP_DIR="${CPPP_TEST_TMP_DIR:-tests/tmp}"
export CPPP_TEST_TMP_DIR="$TMP_DIR"
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

TOTAL_STEPS=39
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
assert_not_contains "${primitives_case%.cppp}.cpp" "shared_ptr<pair<A,B>>" "Pair storage is pointer-backed without shared_ptr"
assert_not_contains "${primitives_case%.cppp}.cpp" "shared_ptr<vector<T>>" "List storage is pointer-backed without shared_ptr"
assert_not_contains "${primitives_case%.cppp}.cpp" "shared_ptr<set<T>>" "Set storage is pointer-backed without shared_ptr"
assert_not_contains "${primitives_case%.cppp}.cpp" "shared_ptr<map<K,V>>" "Map storage is pointer-backed without shared_ptr"
assert_contains "${primitives_case%.cppp}.cpp" "function<bool(const T&, const T&)> value" "Set stores its ordering comparator without shared_ptr"
assert_contains "${primitives_case%.cppp}.cpp" "function<bool(const K&, const K&)> value" "Map stores its ordering comparator without shared_ptr"
assert_not_contains "${primitives_case%.cppp}.cpp" "shared_ptr" "generated runtime uses no shared_ptr storage"
assert_not_contains "${primitives_case%.cppp}.cpp" "make_shared" "generated runtime uses raw allocation"
pass "primitive declarations still compile and lower correctly"

progress "pointer-backed container aliases"
container_aliases_case="$(stage_case "$TEST_DIR/container_pointer_aliases.cppp")"
run_program_ok "$container_aliases_case" "$LOG_DIR/container_pointer_aliases.log" "pointer-backed container aliases run"
assert_contains "$LOG_DIR/container_pointer_aliases.log" "2 1 3 1 {(1,0), (2,0)} 6" "containers and objects alias while pairs remain inline values"
assert_contains "${container_aliases_case%.cppp}.cpp" "cppp_smart_pointer<Node> objectFirst = cppp_smart_pointer<Node>::make(5, nullptr);" "struct construction uses CP++ reference-counted handles"
assert_contains "${container_aliases_case%.cppp}.cpp" "if (block && --block->refcount == 0) delete block;" "composite handles reclaim their final acyclic owner"
assert_not_contains "${container_aliases_case%.cppp}.cpp" "shared_ptr" "object programs contain no shared_ptr"
assert_not_contains "${container_aliases_case%.cppp}.cpp" "make_shared" "object programs contain no make_shared"
pass "reference-counted containers and objects alias correctly"

progress "indexed class method receivers"
indexed_class_method_case="$(stage_case "$TEST_DIR/indexed_class_method.cppp")"
run_program_ok "$indexed_class_method_case" "$LOG_DIR/indexed_class_method.log" "indexed class method receiver runs"
assert_contains "$LOG_DIR/indexed_class_method.log" "1" "indexed class method mutates the list element"
run_submit_ok "$indexed_class_method_case" "$LOG_DIR/indexed_class_method_submit.log" "indexed class method submit builds"
assert_contains "${indexed_class_method_case%.cppp}.cpp" "auto&__cppp_list" "method receiver uses mutable list access"
pass "indexed class method receivers use mutable access"

progress "custom class constructors"
custom_constructor_case="$(stage_case "$TEST_DIR/custom_constructor.cppp")"
run_program_ok "$custom_constructor_case" "$LOG_DIR/custom_constructor.log" "custom constructor program runs"
assert_contains "$LOG_DIR/custom_constructor.log" "7" "custom constructor initializes fields"
run_submit_ok "$custom_constructor_case" "$LOG_DIR/custom_constructor_submit.log" "custom constructor submit builds"
assert_contains "${custom_constructor_case%.cppp}.cpp" "Box(long long count,long long value)" "custom constructor is emitted"
pass "custom constructors parse, analyze, and emit"

progress "class copy helper submit retention"
class_copy_submit_case="$(stage_case "$TEST_DIR/class_copy_submit.cppp")"
run_submit_ok "$class_copy_submit_case" "$LOG_DIR/class_copy_submit.log" "class copy helper submit builds with unused methods pruned"
assert_contains "${class_copy_submit_case%.cppp}.cpp" "CPPPCopy" "class copy helper is retained for generated class copy support"
pass "submit retains class-level copy helpers independently of methods"

progress "type and variable name suggestions"
type_name_suggestion_case="$(stage_case "$TEST_DIR/type_name_suggestion.cppp")"
run_failure "$type_name_suggestion_case" "$LOG_DIR/type_name_suggestion.log" "misspelled type should fail"
assert_contains "$LOG_DIR/type_name_suggestion.log" "did you mean 'List'?" "misspelled type suggests the closest type"
variable_name_suggestion_case="$(stage_case "$TEST_DIR/variable_name_suggestion.cppp")"
run_failure "$variable_name_suggestion_case" "$LOG_DIR/variable_name_suggestion.log" "misspelled variable should fail"
assert_contains "$LOG_DIR/variable_name_suggestion.log" "did you mean 'count'?" "misspelled variable suggests a visible variable"
assert_not_contains "$LOG_DIR/variable_name_suggestion.log" "to unknown" "misspelled assignment does not produce an unknown-type cascade"
pass "malformed type and variable names receive edit-distance suggestions"

progress "missing semicolon recovery before for"
missing_semicolon_for_case="$(stage_case "$TEST_DIR/missing_semicolon_before_for.cppp")"
run_failure "$missing_semicolon_for_case" "$LOG_DIR/missing_semicolon_before_for.log" "missing semicolon before for should fail locally"
assert_contains "$LOG_DIR/missing_semicolon_before_for.log" "error: missing semicolon" "missing semicolon is reported on the preceding statement"
assert_not_contains "$LOG_DIR/missing_semicolon_before_for.log" "unexpected token in expression" "for header is not swallowed into the declaration"
assert_not_contains "$LOG_DIR/missing_semicolon_before_for.log" "use of undeclared variable" "recovered loop preserves its scope"
assert_not_contains "$LOG_DIR/missing_semicolon_before_for.log" "unmatched closing brace" "recovered loop preserves block structure"
missing_semicolon_error_count="$(grep -c '^error:' "$LOG_DIR/missing_semicolon_before_for.log")"
[[ "$missing_semicolon_error_count" == "1" ]] || fail "missing semicolon recovery emits one diagnostic"
pass "missing semicolon recovery preserves the following for loop"

progress "partial function standard-name collision"
partial_std_name_collision_case="$(stage_case "$TEST_DIR/partial_std_name_collision.cppp")"
run_program_ok "$partial_std_name_collision_case" "$LOG_DIR/partial_std_name_collision.log" "partial function named get runs"
assert_contains "$LOG_DIR/partial_std_name_collision.log" "99" "partial application captures the List alias"
run_submit_ok "$partial_std_name_collision_case" "$LOG_DIR/partial_std_name_collision_submit.log" "partial function named get submit builds"
assert_contains "${partial_std_name_collision_case%.cppp}.cpp" "static_cast<long long(*)(CPPPList<long long>,long long)>(&get)" "partial function resolves the declared get overload"
pass "partial functions disambiguate standard-library name collisions"

progress "nested runtime source columns"
nested_runtime_source_column_case="$(stage_case "$TEST_DIR/nested_runtime_source_column.cppp")"
run_runtime_error "$nested_runtime_source_column_case" "$LOG_DIR/nested_runtime_source_column.log" "empty heap top reports a runtime error"
assert_contains "$LOG_DIR/nested_runtime_source_column.log" ":4:17" "nested heap top reports its actual source column"
run_compile_ok "$nested_runtime_source_column_case" "$LOG_DIR/nested_runtime_source_column_compile.log" "nested runtime column fixture compiles"
assert_contains "${nested_runtime_source_column_case%.cppp}.cpp" ".top(4, 17)" "generated heap check uses the canonical source column"
pass "nested runtime checks retain canonical source columns"

progress "container clear"
container_clear_case="$(stage_case "$TEST_DIR/container_clear.cppp")"
run_program_ok "$container_clear_case" "$LOG_DIR/container_clear.log" "container clear program runs"
assert_contains "$LOG_DIR/container_clear.log" "0 0 0 0 0 0 0" "clear empties every container type"
run_submit_ok "$container_clear_case" "$LOG_DIR/container_clear_submit.log" "container clear submit builds"
assert_contains "${container_clear_case%.cppp}.cpp" ".clear()" "submit output retains clear methods"
pass "clear empties every container type"

progress "class and inline struct behavior"
class_struct_case="$(stage_case "$TEST_DIR/class_struct_split.cppp")"
run_program_ok "$class_struct_case" "$LOG_DIR/class_struct_split.log" "class and inline struct program runs"
assert_contains "$LOG_DIR/class_struct_split.log" "2 3 4 5 5" "classes alias while inline structs copy"
inline_struct_error_case="$(stage_case "$TEST_DIR/inline_struct_custom_field_error.cppp")"
run_failure "$inline_struct_error_case" "$LOG_DIR/inline_struct_custom_field_error.log" "inline struct cycles are rejected"
assert_contains "$LOG_DIR/inline_struct_custom_field_error.log" "recursive struct value cycle" "inline struct cycle diagnostic"
inline_struct_fields_case="$(stage_case "$TEST_DIR/inline_struct_custom_fields.cppp")"
run_program_ok "$inline_struct_fields_case" "$LOG_DIR/inline_struct_custom_fields.log" "acyclic inline struct custom fields run"
assert_contains "$LOG_DIR/inline_struct_custom_fields.log" "4 8 0" "inline structs contain structs, classes, and List<Self>"
pass "classes and inline structs have distinct ownership rules"

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
submit_body_lines="$(grep -vc '^#' "${int_only_case%.cppp}.cpp")"
[[ "$submit_body_lines" == "1" ]] || fail "submit output compacts non-preprocessor code onto one line"
if grep -Eq '^[[:blank:]]|[[:blank:]]$|[[:blank:]]{2,}' "${int_only_case%.cppp}.cpp"; then
    fail "submit output removes nonessential horizontal whitespace"
fi
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
assert_contains "${submit_dead_code_case%.cppp}.cpp" "long long keep(){" "submit keeps the used method"
assert_not_contains "${submit_dead_code_case%.cppp}.cpp" "long long discard(){" "submit prunes an unused sibling method"
assert_not_contains "${submit_dead_code_case%.cppp}.cpp" "struct Unused" "submit prunes an unused struct"
assert_not_contains "${submit_dead_code_case%.cppp}.cpp" "unusedFunction" "submit prunes an unreachable function"
assert_contains "${submit_dead_code_case%.cppp}.cpp" "class CPPPPair{" "submit keeps the used Pair runtime class"
assert_contains "${submit_dead_code_case%.cppp}.cpp" "class CPPPList{" "submit keeps the used List runtime class"
assert_not_contains "${submit_dead_code_case%.cppp}.cpp" "class CPPPSet{" "submit prunes the unused Set runtime class"
assert_not_contains "${submit_dead_code_case%.cppp}.cpp" "class CPPPMap{" "submit prunes the unused Map runtime class"
pass "submit mode prunes unused functions, structs, methods, and runtime classes"

progress "linear data structure submit pruning"
list_only_linear_case="$(stage_case "$TEST_DIR/list_only_prunes_linear.cppp")"
run_submit_ok "$list_only_linear_case" "$LOG_DIR/list_only_prunes_linear.log" "List-only submit builds"
assert_contains "${list_only_linear_case%.cppp}.cpp" "class CPPPList{" "List-only submit keeps List runtime class"
assert_not_contains "${list_only_linear_case%.cppp}.cpp" "CPPPStack" "List-only submit prunes Stack runtime support"
assert_not_contains "${list_only_linear_case%.cppp}.cpp" "CPPPQueue" "List-only submit prunes Queue runtime support"
assert_not_contains "${list_only_linear_case%.cppp}.cpp" "CPPPDeque" "List-only submit prunes Deque runtime support"
pass "List-only submit prunes linear data structure support"

progress "list literal truthiness"
list_truthiness_case="$(stage_case "$TEST_DIR/list_truthiness.cppp")"
run_submit_ok "$list_truthiness_case" "$LOG_DIR/list_truthiness.log" "list truthiness submit builds"
assert_contains "${list_truthiness_case%.cppp}.cpp" "if((!(even).empty()))" "named list truthiness lowers to emptiness check"
assert_contains "${list_truthiness_case%.cppp}.cpp" "if((!(CPPPList<long long>{1}).empty()))" "list literal truthiness lowers inline"
assert_contains "${list_truthiness_case%.cppp}.cpp" "CPPPList<CPPPList<long long>>grid=CPPPList<CPPPList<long long>>{CPPPList<long long>{1,2},CPPPList<long long>{3}};" "nested list literal lowers correctly"
pass "list literals and truthiness regressions are covered"

progress "list literal mismatch"
list_literal_error_case="$(stage_case "$TEST_DIR/list_literal_error.cppp")"
run_failure "$list_literal_error_case" "$LOG_DIR/list_literal_error.log" "mismatched list literal should fail"
assert_contains "$LOG_DIR/list_literal_error.log" "cannot implicitly convert float to int in list literal" "list literal type diagnostic"
pass "list literal type mismatch is rejected"

progress "expression behavior"
expression_behavior_case="$(stage_case "$TEST_DIR/expression_behavior.cppp")"
run_program_ok "$expression_behavior_case" "$LOG_DIR/expression_behavior.log" "expression behavior program runs"
run_submit_ok "$expression_behavior_case" "$LOG_DIR/expression_behavior_submit.log" "expression behavior submit builds"
assert_contains "$LOG_DIR/expression_behavior.log" "14 20 9 30 2" "expression behavior output"
assert_contains "$LOG_DIR/expression_behavior.log" $'1 1 1 1 0\n0 1' "arithmetic-and-bitwise-before-equality and logical-after-equality output"
pass "arithmetic and bitwise precedence, logical precedence, variables, calls, and nested indexing are preserved"

progress "range printing"
print_range_case="$(stage_case "$TEST_DIR/print_range.cppp")"
run_program_ok "$print_range_case" "$LOG_DIR/print_range.log" "ranges print in ordinary mode"
run_submit_ok "$print_range_case" "$LOG_DIR/print_range_submit.log" "ranges print in submit mode"
assert_contains "$LOG_DIR/print_range.log" $'[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]\n[5, 4, 3, 2]\n[2, 5, 8]' "range printing output"
assert_contains "${print_range_case%.cppp}.cpp" "CPPPPrintValue(ostream&output,const CPPPRange&values)" "submit output keeps range printing helper"
pass "ranges print as list-style integer sequences"

progress "NULL literal equality"
null_literal_equality_case="$(stage_case "$TEST_DIR/null_literal_equality.cppp")"
run_program_ok "$null_literal_equality_case" "$LOG_DIR/null_literal_equality.log" "NULL literal equality runs"
run_submit_ok "$null_literal_equality_case" "$LOG_DIR/null_literal_equality_submit.log" "NULL literal equality submit builds"
assert_contains "$LOG_DIR/null_literal_equality.log" "1 0 1 0" "NULL compares equal to null class handles and literals"
assert_contains "${null_literal_equality_case%.cppp}.cpp" "==nullptr" "submit lowering checks class handles against null"
pass "NULL compares equal to null class handles and literals"

progress "split sublist submit helper dependencies"
split_sublist_submit_case="$(stage_case "$TEST_DIR/split_sublist_submit.cppp")"
run_submit_ok "$split_sublist_submit_case" "$LOG_DIR/split_sublist_submit.log" "sublist split submit builds"
assert_contains "${split_sublist_submit_case%.cppp}.cpp" "CPPPList(It first,It last)" "sublist split submit keeps List iterator constructor"
pass "sublist split submit keeps its internal List constructor dependency"

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

progress "run-mode input safety"
input_safety_case="$(stage_case "$TEST_DIR/input_safety.cppp")"
printf '4\n1 3 5 7\n' | "$COMPILER" --cppp "$input_safety_case" --run >"$LOG_DIR/input_safety.log" 2>&1 || fail "valid typed input program runs"
assert_contains "$LOG_DIR/input_safety.log" "2" "typed input binary search output"
input_safety_negative_case="$(stage_case "$TEST_DIR/input_safety_negative.cppp")"
if "$COMPILER" --cppp "$input_safety_negative_case" --run >"$LOG_DIR/input_safety_negative.log" 2>&1; then fail "negative List input size should fail"; fi
assert_contains "$LOG_DIR/input_safety_negative.log" "input() List size cannot be negative" "negative List input diagnostic"
input_safety_malformed_case="$(stage_case "$TEST_DIR/input_safety_malformed.cppp")"
if printf '1 x 3\n' | "$COMPILER" --cppp "$input_safety_malformed_case" --run >"$LOG_DIR/input_safety_malformed.log" 2>&1; then fail "malformed List input should fail"; fi
assert_contains "$LOG_DIR/input_safety_malformed.log" "List contains a value of the wrong type" "malformed List input diagnostic"
pass "run-mode input rejects malformed values and sizes"

progress "canonical whole-source tokenizer"
token_stream_happy_case="$(stage_case "tests/token_stream_happy.cppp")"
run_program_ok "$token_stream_happy_case" "$LOG_DIR/token_stream_happy.log" "token stream happy path runs"
assert_contains "$LOG_DIR/token_stream_happy.log" "6" "token stream happy path output"
token_stream_error_case="$(stage_case "tests/token_stream_error.cppp")"
run_failure "$token_stream_error_case" "$LOG_DIR/token_stream_error.log" "unknown source token is rejected"
assert_contains "$LOG_DIR/token_stream_error.log" "unrecognized token '@'" "unknown source token diagnostic"
if "$PYTHON_CMD" tests/tokenizer_catalog_test.py; then
    pass "98 tokenizer catalog cases and 13 exact stream snapshots pass"
else
    fail "canonical tokenizer catalog coverage"
fi

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
