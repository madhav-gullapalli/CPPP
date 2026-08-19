# Compiler Tooling

This document is for CP++ compiler contributors. It describes inspection and
regression tools rather than the CP++ language itself.

## Inspection modes

```sh
build/cppp --cppp file.cppp --tokens
build/cppp --cppp file.cppp --ast
build/cppp --cppp file.cppp --semantic
build/cppp --cppp file.cppp --submit-ast
```

`--tokens` prints the canonical whole-file token stream as JSON lines, with
token kind, text, source location, and byte offsets. It does not generate C++.

`--ast` prints the recursive syntax tree after parsing and before semantic
analysis. It includes node kinds, important fields, expressions, and source
spans. It does not generate C++.

`--semantic` prints the semantic view: resolved types, symbols, calls,
lvalue state, and conversions. It exits before C++ code generation.

`--submit-ast` runs the submit-only analyzed-AST pruning stage and prints the
remaining program tree. It is useful for inspecting reachability before submit
codegen and does not generate C++.

See [compiler_pipeline.md](compiler_pipeline.md) for the stage boundaries and
representation ownership rules.

## Submit-mode implementation notes

Submit mode prunes unreachable functions, unused container classes, runtime
helpers, and individual container methods before compacting the C++ output.
Reachability is tracked at function/method granularity. Requirements inside an
unreachable function do not retain methods or classes, and using one container
method does not retain unrelated methods.

The compact output removes comments, indentation, blank lines, and unnecessary
token separators while preserving required preprocessor newlines and literal
contents. `--readable` uses the same pruning pass without compaction. Some
diagnostic checks enabled by `--run` are intentionally absent from submit
output.

## Regression and snapshot checks

```sh
make test
make ast-invariants
make semantic-invariants
make codegen-freeze
```

`make codegen-freeze` compares both ordinary and submit-mode C++ generated for
every documented `correct.txt` example against the local snapshot baseline.
Do not rerecord snapshots merely to hide a regression. After reviewing an
intentional codegen change, update the local baseline with:

```sh
make codegen-freeze-record
```

The baseline directory is ignored and is not part of the source tree. The
freeze suite intentionally covers `correct.txt`, while error behavior is
covered by the normal regression suite.
