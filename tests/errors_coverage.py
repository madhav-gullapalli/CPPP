import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional


ROOT = Path(__file__).resolve().parent.parent
ERRORS_TXT = ROOT / "errors.txt"
TMP_DIR = ROOT / "tests" / "tmp" / "errors_catalog"
CASE_DIR = TMP_DIR / "cases"
LOG_DIR = TMP_DIR / "logs"
COMPILER = ROOT / "build" / ("cppp.exe" if os.name == "nt" else "cppp")


@dataclass
class Case:
    key: str
    title: str
    example: str
    diagnostic: Optional[str]
    output: Optional[str]


SECTION_RULES: Dict[str, Dict[str, object]] = {
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
}


def slugify(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def is_title(line: str) -> bool:
    stripped = line.strip()
    if not stripped or line.startswith(" ") or line.startswith("=") or line.startswith("-"):
        return False
    if stripped in {
        "CP++ Error Documentation",
        "General syntax",
        "Primitive type errors",
        "Print errors",
        "Expression and assignment errors",
        "Operator support",
        "Control flow",
        "Input",
        "Runtime errors",
        "Whitespace-insensitive parsing",
        "Multiple statements per line",
    }:
        return False
    return True


def parse_cases() -> List[Case]:
    lines = ERRORS_TXT.read_text(encoding="utf-8").splitlines()
    cases: List[Case] = []
    title_counts: Dict[str, int] = {}
    i = 0
    while i < len(lines):
        line = lines[i]
        if not is_title(line):
            i += 1
            continue
        title = line.strip()
        j = i + 1
        example_lines: List[str] = []
        diagnostic: Optional[str] = None
        output_lines: List[str] = []
        while j < len(lines) and not is_title(lines[j]):
            if lines[j].strip() == "Example:":
                j += 1
                while j < len(lines):
                    current = lines[j]
                    if current.strip() in {"Diagnostic:", "Output:"} or is_title(current):
                        break
                    if current.startswith("    "):
                        example_lines.append(current[4:])
                    elif current.strip() == "":
                        example_lines.append("")
                    else:
                        break
                    j += 1
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
                while j < len(lines):
                    current = lines[j]
                    if is_title(current):
                        break
                    if current.startswith("    "):
                        output_lines.append(current[4:])
                    elif current.strip() == "":
                        output_lines.append("")
                    else:
                        break
                    j += 1
                continue
            j += 1

        if example_lines:
            title_counts[title] = title_counts.get(title, 0) + 1
            suffix = f"_{title_counts[title]}" if title_counts[title] > 1 else ""
            cases.append(
                Case(
                    key=slugify(title) + suffix,
                    title=title,
                    example="\n".join(example_lines).strip("\n"),
                    diagnostic=diagnostic,
                    output="\n".join(output_lines).strip("\n") if output_lines else None,
                )
            )
        i = j
    return cases


def choose_mode(case: Case) -> str:
    if case.title in SECTION_RULES and "mode" in SECTION_RULES[case.title]:
        return str(SECTION_RULES[case.title]["mode"])
    if case.diagnostic:
        return "run_runtime_error" if "runtime error:" in case.diagnostic else "compile_fail"
    if case.output:
        return "run_ok"
    return "compile_ok"


def write_case(case: Case) -> Path:
    override = SECTION_RULES.get(case.title, {})
    text = str(override.get("example", case.example)).rstrip() + "\n"
    path = CASE_DIR / f"{case.key}.cppp"
    path.write_text(text, encoding="utf-8")
    return path


def run_command(args: List[str], log_path: Path) -> int:
    result = subprocess.run(args, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
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


def main() -> int:
    if not COMPILER.exists():
        raise SystemExit(f"Compiler not found at {COMPILER}")

    if TMP_DIR.exists():
        for child in sorted(TMP_DIR.rglob("*"), reverse=True):
            if child.is_file():
                child.unlink()
            elif child.is_dir():
                child.rmdir()
    CASE_DIR.mkdir(parents=True, exist_ok=True)
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    cases = parse_cases()
    print(f"Running errors.txt coverage for {len(cases)} documented examples...")

    for case in cases:
        mode = choose_mode(case)
        source = write_case(case)
        log = LOG_DIR / f"{case.key}.log"
        args = [str(COMPILER), "--cppp", str(source.relative_to(ROOT))]
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
            code = run_command(args, log)
            if code != 0:
                raise AssertionError(f"{case.title}: expected run success\n{log.read_text(encoding='utf-8')}")
            expected_output = str(SECTION_RULES.get(case.title, {}).get("expect_output", case.output or "")).strip()
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

    print("errors.txt documented examples are covered.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
