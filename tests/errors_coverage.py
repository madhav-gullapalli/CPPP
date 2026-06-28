import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional


ROOT = Path(__file__).resolve().parent.parent
COMPILER = ROOT / "build" / ("cppp.exe" if os.name == "nt" else "cppp")


@dataclass
class RunScenario:
    input_text: str
    output_text: str


@dataclass
class Case:
    key: str
    title: str
    title_line: int
    example: str
    diagnostic: Optional[str]
    output: Optional[str]
    scenarios: List[RunScenario]


SECTION_RULES: Dict[str, Dict[str, Dict[str, object]]] = {
    "errors.txt": {
        "loop else": {
            "example": "int x = 0;\nwhile (x < 3) {\n    x++;\n} else {\n    print(\"finished\");\n}",
            "mode": "run_ok",
            "expect_output": "finished",
        },
        "submit-mode loop-else optimization": {
            "example": "int x = 0;\nwhile (x < 3) {\n    x++;\n} else {\n    print(\"finished\");\n}",
            "mode": "submit_ok",
        },
        "block brace whitespace": {
            "example": "int n = 0;\nif(true){}\nwhile(n<2)\n{\nn++;\n}",
            "mode": "compile_ok",
        },
        "submit helper pruning": {
            "example": "int x = input();\nprint(x);",
            "mode": "submit_ok",
        },
        "runtime diagnostic model": {
            "mode": "run_runtime_error",
        },
        "input() does not take arguments": {
            "example": "print(input(123));",
            "mode": "compile_fail",
        },
        "division by zero": {
            "mode": "run_runtime_error",
        },
        "modulo by zero": {
            "mode": "run_runtime_error",
        },
        "integer overflow": {
            "mode": "run_runtime_error",
        },
    },
    "correct.txt": {},
}


def slugify(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def is_title(lines: List[str], index: int) -> bool:
    line = lines[index]
    stripped = line.strip()
    if not stripped or line.startswith(" ") or line.startswith("=") or line.startswith("-"):
        return False

    if index + 1 < len(lines):
        next_stripped = lines[index + 1].strip()
        if next_stripped and set(next_stripped) <= {"=", "-"}:
            return False

    if stripped.endswith(":"):
        return False
    return True


def parse_cases(doc_path: Path) -> List[Case]:
    lines = doc_path.read_text(encoding="utf-8").splitlines()
    cases: List[Case] = []
    title_counts: Dict[str, int] = {}
    i = 0
    while i < len(lines):
        line = lines[i]
        if not is_title(lines, i):
            i += 1
            continue
        title = line.strip()
        j = i + 1
        example_lines: List[str] = []
        diagnostic: Optional[str] = None
        output_lines: List[str] = []
        scenarios: List[RunScenario] = []
        pending_input_lines: Optional[List[str]] = None
        while j < len(lines) and not is_title(lines, j):
            if lines[j].strip() == "Example:":
                j += 1
                while j < len(lines):
                    current = lines[j]
                    if current.strip() in {"Diagnostic:", "Output:"} or is_title(lines, j):
                        break
                    if current.startswith("    "):
                        example_lines.append(current[4:])
                    elif current.strip() == "":
                        example_lines.append("")
                    else:
                        break
                    j += 1
                continue
            if lines[j].strip() == "Input:":
                j += 1
                input_lines: List[str] = []
                while j < len(lines):
                    current = lines[j]
                    if current.strip() in {"Diagnostic:", "Output:", "Input:"} or is_title(lines, j):
                        break
                    if current.startswith("    "):
                        input_lines.append(current[4:])
                    elif current.strip() == "":
                        input_lines.append("")
                    else:
                        break
                    j += 1
                pending_input_lines = input_lines
                continue
            if lines[j].strip() == "Diagnostic:":
                j += 1
                while j < len(lines) and lines[j].strip() == "":
                    j += 1
                if j < len(lines) and lines[j].startswith("    "):
                    diagnostic = lines[j].strip()
                continue
            if lines[j].strip() == "Output:":
                j += 1
                current_output_lines: List[str] = []
                while j < len(lines):
                    current = lines[j]
                    if current.strip() == "Input:" or is_title(lines, j):
                        break
                    if current.startswith("    "):
                        current_output_lines.append(current[4:])
                    elif current.strip() == "":
                        current_output_lines.append("")
                    else:
                        break
                    j += 1
                if pending_input_lines is not None:
                    scenarios.append(
                        RunScenario(
                            input_text="\n".join(pending_input_lines).strip("\n"),
                            output_text="\n".join(current_output_lines).strip("\n"),
                        )
                    )
                    pending_input_lines = None
                elif not output_lines:
                    output_lines = current_output_lines
                continue
            j += 1

        if example_lines:
            title_counts[title] = title_counts.get(title, 0) + 1
            suffix = f"_{title_counts[title]}" if title_counts[title] > 1 else ""
            cases.append(
                Case(
                    key=slugify(title) + suffix,
                    title=title,
                    title_line=i + 1,
                    example="\n".join(example_lines).strip("\n"),
                    diagnostic=diagnostic,
                    output="\n".join(output_lines).strip("\n") if output_lines else None,
                    scenarios=scenarios,
                )
            )
        i = j
    return cases


def choose_mode(case: Case, rules: Dict[str, Dict[str, object]]) -> str:
    if case.title in rules and "mode" in rules[case.title]:
        return str(rules[case.title]["mode"])
    if case.diagnostic:
        return "run_runtime_error" if "runtime error:" in case.diagnostic else "compile_fail"
    if case.output or case.scenarios:
        return "run_ok"
    return "compile_ok"


def write_case(case: Case, case_dir: Path, rules: Dict[str, Dict[str, object]]) -> Path:
    override = rules.get(case.title, {})
    text = str(override.get("example", case.example)).rstrip() + "\n"
    path = case_dir / f"{case.key}.cppp"
    path.write_text(text, encoding="utf-8")
    return path


def run_command(args: List[str], log_path: Path, stdin_text: Optional[str] = None) -> int:
    result = subprocess.run(
        args,
        cwd=ROOT,
        input=stdin_text,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    log_path.write_text(result.stdout, encoding="utf-8")
    return result.returncode


def filtered_output(text: str) -> str:
    kept = []
    for line in text.splitlines():
        if line.startswith("Built ") or line.startswith("Built submit target "):
            continue
        kept.append(line)
    return "\n".join(kept).strip()


def expect_contains(haystack: str, needle: str, label: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"{label}: expected to find {needle!r}\nActual output:\n{haystack}")


def print_progress(current: int, total: int, label: str) -> None:
    width = 28
    filled = 0 if total == 0 else current * width // total
    bar = "#" * filled + "-" * (width - filled)
    print(f"[{current}/{total}] [{bar}] {label}", flush=True)


def main() -> int:
    if len(sys.argv) > 3:
        raise SystemExit("usage: errors_coverage.py [doc-path|start-line] [start-line]")

    doc_arg: Optional[str] = None
    start_line: Optional[int] = None
    if len(sys.argv) == 2:
        if sys.argv[1].isdigit():
            start_line = int(sys.argv[1])
        else:
            doc_arg = sys.argv[1]
    elif len(sys.argv) == 3:
        doc_arg = sys.argv[1]
        if not sys.argv[2].isdigit():
            raise SystemExit("start-line must be an integer")
        start_line = int(sys.argv[2])

    doc_path = ROOT / (doc_arg or "errors.txt")
    if not doc_path.exists():
        raise SystemExit(f"Documentation file not found: {doc_path}")

    rules = SECTION_RULES.get(doc_path.name, {})
    tmp_dir = ROOT / "tests" / "tmp" / f"{doc_path.stem}_catalog_{os.getpid()}"
    case_dir = tmp_dir / "cases"
    log_dir = tmp_dir / "logs"

    if not COMPILER.exists():
        raise SystemExit(f"Compiler not found at {COMPILER}")

    case_dir.mkdir(parents=True, exist_ok=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    cases = parse_cases(doc_path)
    if start_line is not None:
        cases = [case for case in cases if case.title_line >= start_line]
        print(f"Running {doc_path.name} coverage from line {start_line} for {len(cases)} documented examples...")
    else:
        print(f"Running {doc_path.name} coverage for {len(cases)} documented examples...")
    failures: List[str] = []
    total_cases = len(cases)

    for index, case in enumerate(cases, start=1):
        print_progress(index, total_cases, case.title)
        mode = choose_mode(case, rules)
        source = write_case(case, case_dir, rules)
        log = log_dir / f"{case.key}.log"
        args = [str(COMPILER), "--cppp", str(source.relative_to(ROOT))]
        try:
            if mode == "compile_ok":
                args.append("--compile")
                code = run_command(args, log)
                if code != 0:
                    raise AssertionError(f"{case.title}: expected compile success\n{log.read_text(encoding='utf-8')}")
            elif mode == "submit_ok":
                args.append("--submit")
                code = run_command(args, log)
                if code != 0:
                    raise AssertionError(f"{case.title}: expected submit success\n{log.read_text(encoding='utf-8')}")
            elif mode == "run_ok":
                args.append("--run")
                if case.scenarios:
                    for index, scenario in enumerate(case.scenarios, start=1):
                        scenario_log = log_dir / f"{case.key}_{index}.log"
                        code = run_command(args, scenario_log, scenario.input_text + "\n")
                        if code != 0:
                            raise AssertionError(
                                f"{case.title} scenario {index}: expected run success\n{scenario_log.read_text(encoding='utf-8')}"
                            )
                        expect_contains(
                            filtered_output(scenario_log.read_text(encoding="utf-8")),
                            scenario.output_text.strip(),
                            f"{case.title} scenario {index}",
                        )
                else:
                    code = run_command(args, log)
                    if code != 0:
                        raise AssertionError(f"{case.title}: expected run success\n{log.read_text(encoding='utf-8')}")
                    expected_output = str(rules.get(case.title, {}).get("expect_output", case.output or "")).strip()
                    if expected_output:
                        expect_contains(filtered_output(log.read_text(encoding="utf-8")), expected_output, case.title)
            elif mode == "compile_fail":
                code = run_command(args, log)
                if code == 0:
                    raise AssertionError(f"{case.title}: expected compile failure")
                expect_contains(log.read_text(encoding="utf-8"), case.diagnostic or "", case.title)
            elif mode == "run_runtime_error":
                args.append("--run")
                code = run_command(args, log)
                if code == 0:
                    raise AssertionError(f"{case.title}: expected runtime failure")
                expect_contains(log.read_text(encoding="utf-8"), case.diagnostic or "", case.title)
            else:
                raise AssertionError(f"Unknown mode {mode} for {case.title}")
        except AssertionError as error:
            failures.append(str(error))

    if failures:
        print()
        print(f"{doc_path.name} had {len(failures)} failing case(s):")
        for index, failure in enumerate(failures, start=1):
            print()
            print(f"[{index}] {failure}")
        return 1

    print(f"{doc_path.name} documented examples are covered.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
