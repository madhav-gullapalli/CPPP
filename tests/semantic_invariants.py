#!/usr/bin/env python3

import argparse
import random
import shutil
import subprocess
import sys
from pathlib import Path

from errors_coverage import parse_cases


ROOT = Path(__file__).resolve().parent.parent
CATALOG = ROOT / "correct.txt"
COMPILER = ROOT / "build" / "cppp"
WORK_DIR = ROOT / "tests" / "tmp" / "semantic_invariants"


def main() -> int:
    parser = argparse.ArgumentParser(description="Stress the analyzed ProgramAst pass.")
    parser.add_argument("--count", type=int, default=100)
    parser.add_argument("--seed", type=int, default=0x5E6A)
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()

    cases = parse_cases(CATALOG)
    if args.count < 1 or args.count > len(cases):
        print(f"error: --count must be between 1 and {len(cases)}", file=sys.stderr)
        return 1
    if not COMPILER.exists():
        print(f"error: compiler not found at {COMPILER}", file=sys.stderr)
        return 1

    if WORK_DIR.exists():
        shutil.rmtree(WORK_DIR)
    WORK_DIR.mkdir(parents=True)

    chosen = random.Random(args.seed).sample(list(enumerate(cases)), args.count)
    chosen.sort(key=lambda item: item[0])
    failures = []
    for progress, (_, case) in enumerate(chosen, start=1):
        source = WORK_DIR / f"{case.key}.cppp"
        source.write_text(case.example.rstrip() + "\n", encoding="utf-8")
        command = [str(COMPILER), "--cppp", str(source.relative_to(ROOT)), "--semantic"]
        first = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        second = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        generated = source.with_suffix(".cpp")
        problem = ""
        if first.returncode != 0:
            problem = first.stderr or first.stdout or f"exit status {first.returncode}"
        elif second.returncode != 0:
            problem = second.stderr or second.stdout or f"second exit status {second.returncode}"
        elif first.stdout != second.stdout:
            problem = "semantic serialization changed between identical analyses"
        elif not first.stdout.startswith("AnalyzedProgram valid=true"):
            problem = "semantic output has no valid analyzed-program root"
        elif "type: unknown" in first.stdout:
            problem = "valid semantic output contains an unknown type"
        elif generated.exists():
            problem = f"--semantic emitted {generated.relative_to(ROOT)}"
        if problem:
            failures.append(f"{case.title} (correct.txt:{case.title_line}): {problem}")
        if not args.quiet:
            print(f"[{progress}/{args.count}] {case.title}", flush=True)

    if failures:
        print(f"\nSemantic invariant stress found {len(failures)} failure(s):", file=sys.stderr)
        for failure in failures:
            print(f"\n{failure}", file=sys.stderr)
        return 1

    print(f"All {args.count} deterministic semantic cases passed (seed={args.seed}).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
