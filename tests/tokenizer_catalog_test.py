#!/usr/bin/env python3
"""Exercise the canonical tokenizer against a deterministic random catalog sample."""

import hashlib
import importlib.util
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Tuple


ROOT = Path(__file__).resolve().parent.parent
COMPILER = ROOT / "build" / ("cppp.exe" if os.name == "nt" else "cppp")
SNAPSHOTS = ROOT / "tests" / "tokenizer_integration_snapshots.json"
SAMPLE_SEED = "cppp-token-stream-v1"
CASES_PER_CATALOG = 49
INTEGRATION_CASES = 13


def load_catalog_module():
    path = ROOT / "tests" / "errors_coverage.py"
    spec = importlib.util.spec_from_file_location("cppp_errors_coverage", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def case_source(module, catalog: str, case) -> str:
    override = module.SECTION_RULES.get(catalog, {}).get(case.title, {})
    return str(override.get("example", case.example)).rstrip() + "\n"


def selected_cases(module) -> List[Tuple[str, object, str]]:
    selected: List[Tuple[str, object, str]] = []
    for catalog in ("correct.txt", "errors.txt"):
        cases = []
        for case in module.parse_cases(ROOT / catalog):
            source = case_source(module, catalog, case)
            if source.strip():
                score = hashlib.sha256(
                    f"{SAMPLE_SEED}:{catalog}:{case.key}".encode("utf-8")
                ).hexdigest()
                cases.append((score, case, source))
        cases.sort(key=lambda item: item[0])
        if len(cases) < CASES_PER_CATALOG:
            raise AssertionError(
                f"{catalog} only has {len(cases)} tokenizable examples; "
                f"need {CASES_PER_CATALOG}"
            )
        selected.extend((catalog, case, source) for _, case, source in cases[:CASES_PER_CATALOG])
    return selected


def normalized_compiler_source(source: str) -> bytes:
    # The driver reads with getline() and rejoins physical lines without a final
    # newline, matching SourceManager's canonical contents.
    return "\n".join(source.splitlines()).encode("utf-8")


def validate_stream(label: str, source: str, output: str) -> List[Dict[str, object]]:
    try:
        tokens = [json.loads(line) for line in output.splitlines() if line]
    except json.JSONDecodeError as error:
        raise AssertionError(f"{label}: invalid JSON token output: {error}") from error

    if not tokens:
        raise AssertionError(f"{label}: tokenizer emitted no EOF token")
    required = {
        "kind", "text", "line", "column", "endLine", "endColumn",
        "startOffset", "endOffset",
    }
    canonical = normalized_compiler_source(source)
    previous_end = 0
    eof_count = 0
    for index, token in enumerate(tokens):
        missing = required - set(token)
        if missing:
            raise AssertionError(f"{label}: token {index} is missing {sorted(missing)}")
        start = int(token["startOffset"])
        end = int(token["endOffset"])
        if start < previous_end or end < start or end > len(canonical):
            raise AssertionError(
                f"{label}: token {index} has invalid/non-monotonic offsets {start}..{end}"
            )
        if token["kind"] == "EndOfFile":
            eof_count += 1
            if index + 1 != len(tokens) or start != end:
                raise AssertionError(f"{label}: EOF must be one empty final token")
        else:
            actual = canonical[start:end].decode("utf-8")
            if actual != token["text"]:
                raise AssertionError(
                    f"{label}: source slice {start}..{end} is {actual!r}, "
                    f"token text is {token['text']!r}"
                )
        previous_end = end
    if eof_count != 1:
        raise AssertionError(f"{label}: expected exactly one EOF token, got {eof_count}")
    return tokens


def run_tokenizer(path: Path) -> str:
    result = subprocess.run(
        [str(COMPILER), "--cppp", str(path.relative_to(ROOT)), "--tokens"],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if result.returncode != 0:
        raise AssertionError(f"tokenizer command failed for {path}:\n{result.stdout}")
    return result.stdout


def main() -> int:
    update = sys.argv[1:] == ["--update-snapshots"]
    if sys.argv[1:] and not update:
        raise SystemExit("usage: tokenizer_catalog_test.py [--update-snapshots]")
    if not COMPILER.exists():
        raise SystemExit(f"compiler not found at {COMPILER}")

    module = load_catalog_module()
    cases = selected_cases(module)
    tmp_root = Path(os.environ.get("CPPP_TEST_TMP_DIR", str(ROOT / "tests" / "tmp")))
    if not tmp_root.is_absolute():
        tmp_root = ROOT / tmp_root
    case_dir = tmp_root / "tokenizer_catalog"
    case_dir.mkdir(parents=True, exist_ok=True)

    expected = {}
    if SNAPSHOTS.exists() and not update:
        expected = json.loads(SNAPSHOTS.read_text(encoding="utf-8"))
    actual_snapshots: Dict[str, str] = {}

    # Every selected case invokes the real compiler driver. Seven correct and
    # six error examples are also exact integration snapshots, not merely
    # tokenizer invariants.
    for index, (catalog, case, source) in enumerate(cases, start=1):
        path = case_dir / f"{catalog[:-4]}_{case.key}.cppp"
        # Keep snapshot byte offsets identical on Windows and POSIX.
        path.write_bytes(source.encode("utf-8"))
        output = run_tokenizer(path)
        label = f"{catalog}:{case.key}"
        validate_stream(label, source, output)
        if run_tokenizer(path) != output:
            raise AssertionError(f"{label}: token output is not deterministic")
        integration_case = index <= 7 or CASES_PER_CATALOG < index <= CASES_PER_CATALOG + 6
        if integration_case:
            actual_snapshots[label] = output
            if not update and expected.get(label) != output:
                raise AssertionError(f"{label}: tokenizer integration snapshot changed")
        print(f"[{index}/{len(cases)}] {label}")

    if update:
        SNAPSHOTS.write_text(
            json.dumps(actual_snapshots, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    elif set(expected) != set(actual_snapshots):
        missing = sorted(set(actual_snapshots) - set(expected))
        extra = sorted(set(expected) - set(actual_snapshots))
        raise AssertionError(f"tokenizer snapshot keys differ; missing={missing}, extra={extra}")

    if len(actual_snapshots) != INTEGRATION_CASES:
        raise AssertionError(
            f"expected {INTEGRATION_CASES} integration snapshots, got {len(actual_snapshots)}"
        )

    print(
        f"Tokenizer catalog coverage passed: {len(cases)} sampled cases, "
        f"{len(actual_snapshots)} exact integration snapshots."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
