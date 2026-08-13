#!/usr/bin/env python3
"""Compile every documented valid program through submit-mode pruning."""

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from errors_coverage import parse_cases


ROOT = Path(__file__).resolve().parent.parent
COMPILER = ROOT / "build" / ("cppp.exe" if os.name == "nt" else "cppp")
CATALOG = ROOT / "correct.txt"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compile correct.txt examples in submit mode without updating snapshots."
    )
    parser.add_argument("--start", type=int, default=1, help="one-based catalog case index")
    parser.add_argument("--count", type=int, default=None, help="number of cases to compile")
    args = parser.parse_args()

    if not COMPILER.exists():
        raise SystemExit(f"Compiler not found: {COMPILER}")
    if args.start < 1:
        raise SystemExit("--start must be at least 1")

    cases = parse_cases(CATALOG)
    selected = cases[args.start - 1:]
    if args.count is not None:
        if args.count < 1:
            raise SystemExit("--count must be at least 1")
        selected = selected[:args.count]
    if not selected:
        raise SystemExit("no catalog cases selected")

    tmp_root = ROOT / "tests" / "tmp"
    tmp_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="submit_catalog_", dir=tmp_root) as temporary:
        case_dir = Path(temporary)
        for offset, case in enumerate(selected, start=args.start):
            source = case_dir / f"{offset:03d}_{case.key}.cppp"
            source.write_text(case.example.rstrip() + "\n", encoding="utf-8")
            result = subprocess.run(
                [str(COMPILER), "--cppp", str(source.relative_to(ROOT)), "--submit"],
                cwd=ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            if result.returncode != 0:
                print(
                    f"submit catalog failure [{offset}/{len(cases)}] "
                    f"{case.title} (correct.txt:{case.title_line}):\n{result.stdout}",
                    file=sys.stderr,
                )
                return 1
            print(f"[{offset}/{len(cases)}] submit compiled {case.title}", flush=True)

    print(f"Submit catalog compiled {len(selected)} correct.txt example(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
