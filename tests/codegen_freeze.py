#!/usr/bin/env python3

import argparse
import difflib
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List

from errors_coverage import Case, parse_cases


ROOT = Path(__file__).resolve().parent.parent
CATALOG = ROOT / "correct.txt"
COMPILER = ROOT / "build" / ("cppp.exe" if os.name == "nt" else "cppp")
DEFAULT_SNAPSHOT_DIR = ROOT / "tests" / "codegen_snapshots"
DEFAULT_WORK_DIR = ROOT / "tests" / "tmp" / "codegen_freeze"
MANIFEST_NAME = "manifest.json"
MANIFEST_VERSION = 1


def digest(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def case_source(case: Case) -> str:
    return case.example.rstrip() + "\n"


def snapshot_name(index: int, case: Case) -> str:
    return f"{index:03d}_{case.key}.cpp"


def build_compiler() -> None:
    make_command = os.environ.get("MAKE_CMD")
    if not make_command:
        make_command = "mingw32-make" if os.name == "nt" and shutil.which("mingw32-make") else "make"
    result = subprocess.run(
        [make_command],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"CP++ build failed:\n{result.stdout}")


def prepare_work_dir(work_dir: Path) -> Path:
    if work_dir.exists():
        shutil.rmtree(work_dir)
    case_dir = work_dir / "cases"
    case_dir.mkdir(parents=True)
    return case_dir


def transpile(case: Case, case_dir: Path) -> str:
    source = case_dir / f"{case.key}.cppp"
    source.write_text(case_source(case), encoding="utf-8")
    result = subprocess.run(
        [str(COMPILER), "--cppp", str(source.relative_to(ROOT))],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"{case.title} (correct.txt:{case.title_line}) failed to transpile:\n{result.stdout}"
        )
    generated = source.with_suffix(".cpp")
    if not generated.exists():
        raise RuntimeError(f"{case.title}: transpile succeeded without producing {generated}")
    return generated.read_text(encoding="utf-8")


def manifest_cases(cases: List[Case]) -> List[Dict[str, object]]:
    return [
        {
            "key": case.key,
            "title": case.title,
            "title_line": case.title_line,
            "source_sha256": digest(case_source(case)),
            "snapshot": snapshot_name(index, case),
        }
        for index, case in enumerate(cases, start=1)
    ]


def print_progress(index: int, total: int, case: Case, action: str) -> None:
    print(f"[{index}/{total}] {action} {case.title}", flush=True)


def record(cases: List[Case], snapshot_dir: Path, work_dir: Path) -> None:
    case_dir = prepare_work_dir(work_dir)
    temporary_snapshots = snapshot_dir.with_name(snapshot_dir.name + ".new")
    if temporary_snapshots.exists():
        shutil.rmtree(temporary_snapshots)
    temporary_snapshots.mkdir(parents=True)

    try:
        for index, case in enumerate(cases, start=1):
            print_progress(index, len(cases), case, "recording")
            generated = transpile(case, case_dir)
            (temporary_snapshots / snapshot_name(index, case)).write_text(generated, encoding="utf-8")

        manifest = {
            "version": MANIFEST_VERSION,
            "catalog": "correct.txt",
            "mode": "default transpile",
            "cases": manifest_cases(cases),
        }
        (temporary_snapshots / MANIFEST_NAME).write_text(
            json.dumps(manifest, indent=2) + "\n",
            encoding="utf-8",
        )
        if snapshot_dir.exists():
            shutil.rmtree(snapshot_dir)
        temporary_snapshots.rename(snapshot_dir)
    except Exception:
        shutil.rmtree(temporary_snapshots, ignore_errors=True)
        raise

    print(f"Recorded {len(cases)} codegen snapshots in {snapshot_dir.relative_to(ROOT)}")


def load_manifest(snapshot_dir: Path) -> Dict[str, object]:
    manifest_path = snapshot_dir / MANIFEST_NAME
    if not manifest_path.exists():
        raise RuntimeError(
            "Codegen baseline is missing. Run `make codegen-freeze-record` before checking."
        )
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("version") != MANIFEST_VERSION or manifest.get("catalog") != "correct.txt":
        raise RuntimeError("Codegen baseline manifest is incompatible; record it again.")
    return manifest


def unified_difference(expected: str, actual: str, snapshot: str) -> str:
    lines = list(
        difflib.unified_diff(
            expected.splitlines(),
            actual.splitlines(),
            fromfile=f"baseline/{snapshot}",
            tofile=f"current/{snapshot}",
            lineterm="",
        )
    )
    limit = 160
    if len(lines) > limit:
        lines = lines[:limit] + [f"... diff truncated after {limit} lines"]
    return "\n".join(lines)


def check(cases: List[Case], snapshot_dir: Path, work_dir: Path) -> None:
    manifest = load_manifest(snapshot_dir)
    expected_cases = manifest_cases(cases)
    if manifest.get("cases") != expected_cases:
        raise RuntimeError(
            "correct.txt examples no longer match the recorded baseline manifest. "
            "Review the catalog change, then run `make codegen-freeze-record` intentionally."
        )

    expected_files = {str(entry["snapshot"]) for entry in expected_cases}
    actual_files = {path.name for path in snapshot_dir.glob("*.cpp")}
    if actual_files != expected_files:
        missing = sorted(expected_files - actual_files)
        extra = sorted(actual_files - expected_files)
        details = []
        if missing:
            details.append("missing: " + ", ".join(missing))
        if extra:
            details.append("unexpected: " + ", ".join(extra))
        raise RuntimeError("Snapshot directory does not match its manifest (" + "; ".join(details) + ")")

    case_dir = prepare_work_dir(work_dir)
    failures: List[str] = []
    for index, case in enumerate(cases, start=1):
        print_progress(index, len(cases), case, "checking")
        generated = transpile(case, case_dir)
        name = snapshot_name(index, case)
        expected = (snapshot_dir / name).read_text(encoding="utf-8")
        if generated != expected:
            failures.append(
                f"{case.title} (correct.txt:{case.title_line}) changed codegen:\n"
                + unified_difference(expected, generated, name)
            )

    if failures:
        print(f"\nCodegen freeze detected {len(failures)} changed case(s):", file=sys.stderr)
        for failure in failures:
            print(f"\n{failure}", file=sys.stderr)
        raise RuntimeError(
            "Generated C++ changed. Preserve the old codegen or explicitly record a new baseline after review."
        )

    print(f"All {len(cases)} correct.txt codegen snapshots match.")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Record or check the local correct.txt generated-C++ freeze."
    )
    parser.add_argument("action", choices=("record", "check"), nargs="?", default="check")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument(
        "--snapshot-dir",
        type=Path,
        default=Path(os.environ.get("CPPP_CODEGEN_SNAPSHOT_DIR", DEFAULT_SNAPSHOT_DIR)),
    )
    parser.add_argument(
        "--work-dir",
        type=Path,
        default=Path(os.environ.get("CPPP_CODEGEN_WORK_DIR", DEFAULT_WORK_DIR)),
    )
    args = parser.parse_args()

    snapshot_dir = args.snapshot_dir if args.snapshot_dir.is_absolute() else ROOT / args.snapshot_dir
    work_dir = args.work_dir if args.work_dir.is_absolute() else ROOT / args.work_dir

    try:
        if not args.skip_build:
            build_compiler()
        if not COMPILER.exists():
            raise RuntimeError(f"Compiler not found at {COMPILER}")
        cases = parse_cases(CATALOG)
        if not cases:
            raise RuntimeError("correct.txt contains no extractable examples")
        if args.action == "record":
            record(cases, snapshot_dir, work_dir)
        else:
            check(cases, snapshot_dir, work_dir)
    except (OSError, RuntimeError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
