import os
import difflib
import json
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional


ROOT = Path(__file__).resolve().parent.parent
COMPILER = ROOT / "build" / ("cppp.exe" if os.name == "nt" else "cppp")
ERROR_SNAPSHOTS = ROOT / "tests" / "errors_full_output.json"
MAKE_CMD = os.environ.get("MAKE_CMD")
if not MAKE_CMD:
    if os.name == "nt" and shutil.which("mingw32-make"):
        MAKE_CMD = "mingw32-make"
    else:
        MAKE_CMD = shutil.which("make") or "make"


@dataclass
class RunScenario:
    input_text: str
    output_text: str


@dataclass
class Case:
    key: str
    section: str
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
    current_section = ""
    i = 0
    while i < len(lines):
        if i + 1 < len(lines):
            underline = lines[i + 1].strip()
            if lines[i].strip() and underline and set(underline) <= {"=", "-"}:
                current_section = lines[i].strip()
                i += 2
                continue

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
        while j < len(lines):
            if is_title(lines, j):
                break
            if j + 1 < len(lines):
                underline = lines[j + 1].strip()
                if lines[j].strip() and underline and set(underline) <= {"=", "-"}:
                    break
            if lines[j].strip() == "Example:":
                j += 1
                while j < len(lines):
                    current = lines[j]
                    if current.strip() in {"Diagnostic:", "Output:"} or is_title(lines, j):
                        break
                    if j + 1 < len(lines):
                        underline = lines[j + 1].strip()
                        if lines[j].strip() and underline and set(underline) <= {"=", "-"}:
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
                    if j + 1 < len(lines):
                        underline = lines[j + 1].strip()
                        if lines[j].strip() and underline and set(underline) <= {"=", "-"}:
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
                    if j + 1 < len(lines):
                        underline = lines[j + 1].strip()
                        if lines[j].strip() and underline and set(underline) <= {"=", "-"}:
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
                    section=current_section,
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


def write_case_variant(case: Case, case_dir: Path, rules: Dict[str, Dict[str, object]], suffix: str) -> Path:
    override = rules.get(case.title, {})
    text = str(override.get("example", case.example)).rstrip() + "\n"
    path = case_dir / f"{case.key}{suffix}.cppp"
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


def executable_for(source: Path) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    return source.parent / "build" / f"{source.stem}{suffix}"


def run_subrun(source: Path, log_path: Path, stdin_text: Optional[str] = None) -> int:
    result = subprocess.run(
        [MAKE_CMD, "subrun", f"INPUT={source.relative_to(ROOT)}"],
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


def normalized_error_output(text: str) -> str:
    normalized: List[str] = []
    for line in filtered_output(text).splitlines():
        if re.match(r"^.*\.cppp:\d+:\d+: error:", line):
            line = re.sub(r"^.*\.cppp(?=:\d+:\d+: error:)", "<source>", line)
        elif re.match(r"^\s*(?:-->|:::)\s+.*\.cppp:\d+:\d+$", line):
            line = re.sub(
                r"^(\s*(?:-->|:::)\s+).+\.cppp(?=:\d+:\d+$)",
                r"\1<source>",
                line,
            )
        normalized.append(line.rstrip())
    return "\n".join(normalized).strip()


def expect_full_error_output(
    actual_text: str,
    expected_text: str,
    label: str,
) -> None:
    actual = normalized_error_output(actual_text)
    expected = expected_text.strip()
    if actual == expected:
        return
    difference = "\n".join(
        difflib.unified_diff(
            expected.splitlines(),
            actual.splitlines(),
            fromfile="expected",
            tofile="actual",
            lineterm="",
        )
    )
    raise AssertionError(f"{label}: full diagnostic output differed\n{difference}")


def expect_contains(haystack: str, needle: str, label: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"{label}: expected to find {needle!r}\nActual output:\n{haystack}")


def print_progress(current: int, total: int, label: str) -> None:
    width = 28
    filled = 0 if total == 0 else current * width // total
    bar = "#" * filled + "-" * (width - filled)
    print(f"[{current}/{total}] [{bar}] {label}", flush=True)


def main() -> int:
    doc_arg: Optional[str] = None
    start_line: Optional[int] = None
    subrun_only = False
    update_snapshots = False
    args = sys.argv[1:]
    for flag in ("--subrun-only", "--update-snapshots"):
        if flag in args:
            if flag == "--subrun-only":
                subrun_only = True
            else:
                update_snapshots = True
            args.remove(flag)

    if len(args) == 1:
        if args[0].isdigit():
            start_line = int(args[0])
        else:
            doc_arg = args[0]
    elif len(args) == 2:
        doc_arg = args[0]
        if not args[1].isdigit():
            raise SystemExit("start-line must be an integer")
        start_line = int(args[1])
    elif len(args) > 2:
        raise SystemExit(
            "usage: errors_coverage.py [doc-path|start-line] [start-line] "
            "[--subrun-only] [--update-snapshots]"
        )

    doc_path = ROOT / (doc_arg or "errors.txt")
    if not doc_path.exists():
        raise SystemExit(f"Documentation file not found: {doc_path}")

    rules = SECTION_RULES.get(doc_path.name, {})
    error_snapshots: Dict[str, str] = {}
    if doc_path.name == "errors.txt":
        if not ERROR_SNAPSHOTS.exists() and not update_snapshots:
            raise SystemExit(f"Full error snapshot file not found: {ERROR_SNAPSHOTS}")
        if ERROR_SNAPSHOTS.exists():
            error_snapshots = json.loads(ERROR_SNAPSHOTS.read_text(encoding="utf-8"))
    if update_snapshots and (doc_path.name != "errors.txt" or start_line is not None):
        raise SystemExit("--update-snapshots requires a complete errors.txt run")
    tmp_dir = ROOT / "tests" / "tmp" / f"{doc_path.stem}_catalog_{os.getpid()}"
    case_dir = tmp_dir / "cases"
    log_dir = tmp_dir / "logs"

    if not COMPILER.exists():
        raise SystemExit(f"Compiler not found at {COMPILER}")

    case_dir.mkdir(parents=True, exist_ok=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    cases = parse_cases(doc_path)
    if doc_path.name == "errors.txt" and start_line is None and not update_snapshots:
        failing_case_keys = {
            case.key
            for case in cases
            if choose_mode(case, rules) in {"compile_fail", "run_runtime_error"}
        }
        snapshot_keys = set(error_snapshots)
        missing_snapshots = sorted(failing_case_keys - snapshot_keys)
        extra_snapshots = sorted(snapshot_keys - failing_case_keys)
        if missing_snapshots or extra_snapshots:
            details: List[str] = []
            if missing_snapshots:
                details.append(f"missing: {', '.join(missing_snapshots)}")
            if extra_snapshots:
                details.append(f"unused: {', '.join(extra_snapshots)}")
            raise SystemExit(f"Full error snapshots do not match errors.txt ({'; '.join(details)})")
    if start_line is not None:
        cases = [case for case in cases if case.title_line >= start_line]
        mode_label = " subrun-only" if subrun_only else ""
        print(f"Running {doc_path.name}{mode_label} coverage from line {start_line} for {len(cases)} documented examples...")
    else:
        mode_label = " subrun-only" if subrun_only else ""
        print(f"Running {doc_path.name}{mode_label} coverage for {len(cases)} documented examples...")
    failures: List[str] = []
    updated_snapshots: Dict[str, str] = {}
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
                if subrun_only and doc_path.name == "correct.txt" and case.section == "Synthesis tests":
                    if case.scenarios:
                        for index, scenario in enumerate(case.scenarios, start=1):
                            subrun_source = write_case_variant(case, case_dir, rules, "_subrun")
                            subrun_log = log_dir / f"{case.key}_{index}_subrun.log"
                            subrun_code = run_subrun(
                                subrun_source,
                                subrun_log,
                                scenario.input_text + "\n",
                            )
                            if subrun_code != 0:
                                raise AssertionError(
                                    f"{case.title} scenario {index}: expected subrun success\n{subrun_log.read_text(encoding='utf-8')}"
                                )
                            expect_contains(
                                filtered_output(subrun_log.read_text(encoding="utf-8")),
                                scenario.output_text.strip(),
                                f"{case.title} scenario {index} subrun",
                            )
                    else:
                        subrun_source = write_case_variant(case, case_dir, rules, "_subrun")
                        subrun_log = log_dir / f"{case.key}_subrun.log"
                        subrun_code = run_subrun(subrun_source, subrun_log)
                        if subrun_code != 0:
                            raise AssertionError(f"{case.title}: expected subrun success\n{subrun_log.read_text(encoding='utf-8')}")
                        expected_output = str(rules.get(case.title, {}).get("expect_output", case.output or "")).strip()
                        if expected_output:
                            expect_contains(
                                filtered_output(subrun_log.read_text(encoding="utf-8")),
                                expected_output,
                                f"{case.title} subrun",
                            )
                    continue
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
                        if doc_path.name == "correct.txt" and case.section == "Synthesis tests":
                            run_cpp_size = source.with_suffix(".cpp").stat().st_size
                            subrun_source = write_case_variant(case, case_dir, rules, "_subrun")
                            subrun_log = log_dir / f"{case.key}_{index}_subrun.log"
                            subrun_code = run_subrun(
                                subrun_source,
                                subrun_log,
                                scenario.input_text + "\n",
                            )
                            if subrun_code != 0:
                                raise AssertionError(
                                    f"{case.title} scenario {index}: expected subrun success\n{subrun_log.read_text(encoding='utf-8')}"
                                )
                            submit_cpp_size = subrun_source.with_suffix(".cpp").stat().st_size
                            if submit_cpp_size >= run_cpp_size:
                                raise AssertionError(
                                    f"{case.title} scenario {index}: expected subrun submit output to be smaller than run output "
                                    f"for helper pruning ({submit_cpp_size} >= {run_cpp_size})"
                                )
                            expect_contains(
                                filtered_output(subrun_log.read_text(encoding="utf-8")),
                                scenario.output_text.strip(),
                                f"{case.title} scenario {index} subrun",
                            )
                else:
                    code = run_command(args, log)
                    if code != 0:
                        raise AssertionError(f"{case.title}: expected run success\n{log.read_text(encoding='utf-8')}")
                    expected_output = str(rules.get(case.title, {}).get("expect_output", case.output or "")).strip()
                    if expected_output:
                        expect_contains(filtered_output(log.read_text(encoding="utf-8")), expected_output, case.title)
                    if doc_path.name == "correct.txt" and case.section == "Synthesis tests":
                        run_cpp_size = source.with_suffix(".cpp").stat().st_size
                        subrun_source = write_case_variant(case, case_dir, rules, "_subrun")
                        subrun_log = log_dir / f"{case.key}_subrun.log"
                        subrun_code = run_subrun(subrun_source, subrun_log)
                        if subrun_code != 0:
                            raise AssertionError(f"{case.title}: expected subrun success\n{subrun_log.read_text(encoding='utf-8')}")
                        submit_cpp_size = subrun_source.with_suffix(".cpp").stat().st_size
                        if submit_cpp_size >= run_cpp_size:
                            raise AssertionError(
                                f"{case.title}: expected subrun submit output to be smaller than run output "
                                f"for helper pruning ({submit_cpp_size} >= {run_cpp_size})"
                            )
                        if expected_output:
                            expect_contains(
                                filtered_output(subrun_log.read_text(encoding="utf-8")),
                                expected_output,
                                f"{case.title} subrun",
                            )
            elif mode == "compile_fail":
                code = run_command(args, log)
                if code == 0:
                    raise AssertionError(f"{case.title}: expected compile failure")
                actual_output = log.read_text(encoding="utf-8")
                if update_snapshots:
                    updated_snapshots[case.key] = normalized_error_output(actual_output)
                else:
                    if case.key not in error_snapshots:
                        raise AssertionError(f"{case.title}: missing full diagnostic snapshot for key {case.key!r}")
                    expect_full_error_output(
                        actual_output,
                        error_snapshots[case.key],
                        case.title,
                    )
            elif mode == "run_runtime_error":
                args.append("--run")
                code = run_command(args, log)
                if code == 0:
                    raise AssertionError(f"{case.title}: expected runtime failure")
                actual_output = log.read_text(encoding="utf-8")
                if update_snapshots:
                    updated_snapshots[case.key] = normalized_error_output(actual_output)
                else:
                    if case.key not in error_snapshots:
                        raise AssertionError(f"{case.title}: missing full diagnostic snapshot for key {case.key!r}")
                    expect_full_error_output(
                        actual_output,
                        error_snapshots[case.key],
                        case.title,
                    )
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

    if update_snapshots:
        ERROR_SNAPSHOTS.write_text(
            json.dumps(updated_snapshots, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(f"Updated {len(updated_snapshots)} full error snapshots.")

    print(f"{doc_path.name} documented examples are covered.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
