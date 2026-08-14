#!/usr/bin/env python3

"""Exercise the analyzed-AST reachability and clone/filter boundary."""

import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
COMPILER = ROOT / "build" / "cppp"
WORK_DIR = ROOT / "tests" / "tmp" / "submit_pruning_invariants"

SOURCE = """\
class Alpha {
    int value;
    Alpha(int start) { value = start; }
    int shared() { Alpha next = Alpha(value); return next.live(); }
    int live() { Alpha next = Alpha(value); return next.helper(); }
    int helper() { Alpha next = Alpha(value); return value + next.recursive(0); }
    int recursive(int count) {
        if(count == 0) { return 0; }
        Alpha next = Alpha(value);
        return next.recursive(count - 1);
    }
    int dead() {
        List<int> uniqueDeadList = [3, 2, 1];
        uniqueDeadList.reverse();
        return len(uniqueDeadList);
    }
}
class Beta {
    int value;
    int shared() { return value + 100; }
}
int callAlpha(Alpha item) { return item.shared(); }
int add(int left, int right) { return left + right; }
int unusedFunction() { return 99; }
Alpha item = Alpha(7);
Beta other = Beta(2);
var addFive = add(5);
print(callAlpha(item), addFive(3));
"""


def run(source: Path, action: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(COMPILER), "--cppp", str(source.relative_to(ROOT)), action],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def subtree(serialized: str, header: str) -> str:
    lines = serialized.splitlines()
    start = next(index for index, line in enumerate(lines) if header in line)
    indent = len(lines[start]) - len(lines[start].lstrip())
    end = start + 1
    while end < len(lines):
        next_indent = len(lines[end]) - len(lines[end].lstrip())
        if lines[end].strip() and next_indent <= indent:
            break
        end += 1
    return "\n".join(lines[start:end])


def main() -> int:
    if not COMPILER.exists():
        print(f"error: compiler not found at {COMPILER}", file=sys.stderr)
        return 1
    if WORK_DIR.exists():
        shutil.rmtree(WORK_DIR)
    WORK_DIR.mkdir(parents=True)
    source = WORK_DIR / "reachability.cppp"
    source.write_text(SOURCE, encoding="utf-8")

    try:
        complete = run(source, "--ast")
        first = run(source, "--submit-ast")
        second = run(source, "--submit-ast")
        require(complete.returncode == 0, complete.stderr or complete.stdout)
        require(first.returncode == 0, first.stderr or first.stdout)
        require(second.returncode == 0, second.stderr or second.stdout)
        require(first.stdout == second.stdout, "pruning is not deterministic")
        require("ClassDecl \"Alpha\"" in first.stdout, "reachable class Alpha was pruned")
        require("FunctionDecl \"shared\"" in first.stdout, "reachable Alpha.shared was pruned")
        require("FunctionDecl \"live\"" in first.stdout, "transitively reachable Alpha.live was pruned")
        require("FunctionDecl \"helper\"" in first.stdout, "transitively reachable Alpha.helper was pruned")
        require("FunctionDecl \"recursive\"" in first.stdout, "recursive reachable method was pruned")
        require("FunctionDecl \"dead\"" not in first.stdout, "dead Alpha method survived")
        require("ClassDecl \"Beta\"" in first.stdout, "constructed class Beta was pruned")
        require("FunctionDecl \"shared\"" not in subtree(first.stdout, "ClassDecl \"Beta\""),
                "calling Alpha.shared retained same-named Beta.shared")
        require("FunctionDecl \"callAlpha\"" in first.stdout, "reachable free function was pruned")
        require("FunctionDecl \"add\"" in first.stdout, "function value/partial application target was pruned")
        require("FunctionDecl \"unusedFunction\"" not in first.stdout, "dead free function survived")
        require("ConstructorDecl \"Alpha\"" in first.stdout, "reachable explicit constructor was pruned")
        require("ClassDecl \"Beta\"" in complete.stdout, "complete AST was unexpectedly pruned")
        require("FunctionDecl \"dead\"" in complete.stdout, "complete AST lost a dead method")
        require("FunctionDecl \"shared\"" in complete.stdout, "complete AST lost a method")
        require(subtree(first.stdout, "FunctionDecl \"shared\"") ==
                subtree(complete.stdout, "FunctionDecl \"shared\""),
                "retained method subtree or source spans changed during pruning")
        require(subtree(first.stdout, "FunctionDecl \"callAlpha\"") ==
                subtree(complete.stdout, "FunctionDecl \"callAlpha\""),
                "retained function subtree or source spans changed during pruning")
        run_result = run(source, "--run")
        run_output = run_result.stdout.splitlines()
        require(run_result.returncode == 0, run_result.stderr or run_result.stdout)
        submit_result = run(source, "--submit")
        submit_output = submit_result.stdout.splitlines()
        require(submit_result.returncode == 0, submit_result.stderr or submit_result.stdout)
        run_executable = WORK_DIR / "build" / "reachability"
        executed = subprocess.run([str(run_executable)], text=True, capture_output=True)
        require(executed.returncode == 0, executed.stderr or executed.stdout)
        require(executed.stdout == "7 8\n", f"unexpected submit output: {executed.stdout!r}")
        require(run_output and run_output[0].startswith("Built "), "run codegen did not build")
        require(run_output[-1] == "7 8", f"unexpected run output: {run_output[-1:]!r}")
        require(submit_output and submit_output[0].startswith("Built submit target "), "submit codegen did not build")

        generated = source.with_suffix(".cpp").read_text(encoding="utf-8")
        require("unusedFunction" not in generated, "submit C++ contains dead free function")
        require("uniqueDeadList" not in generated, "dead method runtime-helper body reached submit codegen")
        require("struct Beta" in generated, "submit C++ lost reachable class Beta")
        require(generated.count("long long shared(){") == 1,
                "submit C++ retained a same-named method on the wrong class")
    except AssertionError as error:
        print(f"submit pruning invariant failure: {error}", file=sys.stderr)
        return 1

    print("Submit pruning invariants passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
