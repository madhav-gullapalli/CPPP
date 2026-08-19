# CP++ Source Overview

This is the quickest source-tree map for the current compiler. It is organized
around the real execution flow, not just a flat list of files.

For the full stage-by-stage walkthrough, start with
[docs/compiler_pipeline.md](../compiler_pipeline.md). Use this file when you
already know roughly which stage you need and want the shortest jump to the
right module.

The source tree is organized into `src/tokenize`, `src/parse`,
`src/semantic_analyze`, and `src/codegen`; the two root-level driver files are
the CLI entry/orchestration layer.

## Pipeline In One Screen

The compiler pipeline today is:

1. `src/cppp.cpp` calls `runCompilerDriver(...)`.
2. `src/compilerDriver.cpp` validates CLI options, opens the input file, creates
   one `CompileContext`, and tokenizes the complete source once.
3. `src/parse/astParser.cpp` uses parser-internal token-backed views to parse the
   canonical `TokenStream` into one recursive, syntax-only `ProgramAst`.
4. `src/semantic_analyze/semanticAnalyzer.cpp` resolves names and types, validates scopes,
   conversions, calls, returns, loops, and aggregates, and produces an
   `AnalyzedProgramAst` view over the enriched tree.
5. Run mode consumes the full analyzed program. Submit mode first creates a
   `PrunedAnalyzedProgramAst` in `src/codegen/submit/pruning`.
6. The run or submit frontend calls `src/codegen/statementCompiler.cpp`, which
   lowers the selected analyzed representation into generated C++ lines.
7. The mode-specific program emitter selects a runtime preamble. Submit emission
   prunes unused support and optionally compacts the completed translation unit;
   `--readable` skips only compaction.
8. `src/compilerDriver.cpp` optionally invokes `g++` and optionally runs the
   produced executable.

If you only read two files first, make them:

- `src/compilerDriver.cpp` for orchestration
- `src/codegen/statementCompiler.cpp` for actual statement-by-statement lowering

## [src/cppp.cpp](../../src/cppp.cpp)
This is a tiny CLI entry point. It just forwards `main(...)` to
`runCompilerDriver(...)`.

## [src/compilerDriver.cpp](../../src/compilerDriver.cpp) and [src/compilerDriver.h](../../src/compilerDriver.h)
These files own one end-to-end compiler invocation. The driver validates CLI
flags, builds `CompileOptions`, opens the input file, creates the shared
`CompileContext`, parses the canonical token stream into a `ProgramAst`, runs
semantic analysis, calls lowering, emits the output `.cpp`, and optionally
invokes `g++` or runs the executable. `--ast` prints syntax; `--semantic`
prints the deterministic analyzed tree and exits before lowering.

## [src/codegen/compileContext.h](../../src/codegen/compileContext.h)
This header defines the shared state passed through the entire pipeline. It is
where source mapping, symbol tables, block/function state, and generated output
buffers live.

## [src/tokenize/tokenizer.cpp](../../src/tokenize/tokenizer.cpp) and [src/tokenize/tokenizer.h](../../src/tokenize/tokenizer.h)
These files implement the canonical whole-source `TokenStream`, token kinds,
literal/comment lexing, byte offsets, and source spans. The `--tokens` driver
mode prints this stream for inspection and snapshot tests.

## [src/parse/sourceSplitter.cpp](../../src/parse/sourceSplitter.cpp) and [src/parse/sourceSplitter.h](../../src/parse/sourceSplitter.h)
This parser-internal stage groups an already-tokenized file into statement-sized
views. It handles top-level terminators, continuation lines, comments, and block
braces without scanning raw source again. Its `SourceFragment` objects do not
cross the `ProgramAst` boundary.

## `src/parse/programAst.h`, `src/parse/astParser.*`, and `src/parse/astPrinter.*`
These files own the recursive full-program syntax tree, construct it without
semantic symbol tables or C++ emission, validate its structural invariants, and
provide deterministic `--ast` output. Nested blocks belong directly to their
functions, aggregates, conditionals, and loops.

## `src/semantic_analyze/semanticAst.h`, `src/semantic_analyze/semanticAnalyzer.*`, and `src/semantic_analyze/semanticPrinter.*`
These files form the dedicated semantic stage. The analyzer walks recursive AST
scope, resolves `TypeSyntax` and symbols, annotates expressions and statements,
registers function/aggregate signatures, computes inline-struct dependencies,
and rejects invalid programs before codegen. `--semantic` prints the result.

## [src/parse/statementParser.cpp](../../src/parse/statementParser.cpp) and [src/parse/statementParser.h](../../src/parse/statementParser.h)
These files classify logical statements into a small statement AST. They are
more about recognizing statement shape than doing full lowering.

## [src/parse/stmtAst.h](../../src/parse/stmtAst.h)
This header declares the statement AST node types used during statement-level
classification.

## [src/parse/controlFlow.cpp](../../src/parse/controlFlow.cpp) and [src/parse/controlFlow.h](../../src/parse/controlFlow.h)
These helpers parse control-flow headers like `if (...)`, `while (...)`,
classic `for (...)`, and CP++ `for (T x in iterable)`. Full block lowering is
still coordinated by `statementCompiler.cpp`.

## [src/codegen/typeDeclarations.cpp](../../src/codegen/typeDeclarations.cpp)
This file validates AST-resolved declaration metadata and emits its generated
C++ form. Initializer expressions still enter through token-slice adapters.

## [src/codegen/assignmentCppp.cpp](../../src/codegen/assignmentCppp.cpp) and [src/codegen/assignmentCppp.h](../../src/codegen/assignmentCppp.h)
These files lower AST-parsed assignments, including compound assignments and
assignment-specific rewrites. Lvalue and value expressions remain token-backed.

## [src/codegen/printCppp.cpp](../../src/codegen/printCppp.cpp) and [src/codegen/printCppp.h](../../src/codegen/printCppp.h)
This pair parses `print(...)` and emits the generated output logic, including
delimiter and list-aware behavior.

## [src/parse/expressions.cpp](../../src/parse/expressions.cpp) and [src/parse/expressions.h](../../src/parse/expressions.h)
These files provide the shared expression/type utility layer used across the
compiler: shared type helpers, semantic conversion predicates, runtime-helper
tracking, and token-slice compatibility adapters used by specialized emitters.
They are not the canonical expression pipeline.

## [src/parse/exprAst.h](../../src/parse/exprAst.h)
This header contains the expression AST used during expression analysis and
emission.

## [src/parse/expressionParser.cpp](../../src/parse/expressionParser.cpp) and [src/parse/expressionParser.h](../../src/parse/expressionParser.h)
This module parses expression syntax into `Expr` AST nodes. It does not perform
semantic analysis or generate C++.

## [src/semantic_analyze/expressionAnalyzer.cpp](../../src/semantic_analyze/expressionAnalyzer.cpp) and [src/semantic_analyze/expressionAnalyzer.h](../../src/semantic_analyze/expressionAnalyzer.h)
This module analyzes an existing `Expr` AST, resolving types, conversions, and
semantic validity without reparsing tokens.

## [src/codegen/expressionCodegen.cpp](../../src/codegen/expressionCodegen.cpp) and [src/codegen/expressionCodegen.h](../../src/codegen/expressionCodegen.h)
This module emits C++ from an analyzed `Expr` AST. Its token-based lvalue entry
point is retained only as a localized compatibility adapter.

## [src/semantic_analyze/functions.cpp](../../src/semantic_analyze/functions.cpp) and [src/semantic_analyze/functions.h](../../src/semantic_analyze/functions.h)
These files provide function-signature and call-description metadata. Function
declarations themselves are parsed by `astParser.cpp` and lowered directly by
`statementCompiler.cpp`.

## [src/codegen/typesCppp.cpp](../../src/codegen/typesCppp.cpp) and [src/codegen/typesCppp.h](../../src/codegen/typesCppp.h)
This module provides emitted runtime support and type-emission machinery used by
the generated C++ program.

## [src/codegen/statementCompiler.cpp](../../src/codegen/statementCompiler.cpp) and [src/codegen/statementCompiler.h](../../src/codegen/statementCompiler.h)
This is the direct code-generation stage. It recursively dispatches on concrete
nodes through `AnalyzedProgramAst`, uses owned `BlockAst` relationships, and
routes expression-level work to assignment, list, print, and type helpers before
queueing generated C++ lines into `CompileContext`.

## Codegen frontends and program emitters
`src/codegen/run/` consumes the full analyzed AST and selects checked run
support. `src/codegen/submit/pruning/` creates the submit-only pruned analyzed
AST. `src/codegen/submit/` consumes that representation, selects only required
runtime/container support, and optionally compacts it. Shared lowering and
serialization remain in `statementCompiler.*` and `programEmitter.*`.

## [src/codegen/submit/submitPostProcessor.cpp](../../src/codegen/submit/submitPostProcessor.cpp) and [src/codegen/submit/submitPostProcessor.h](../../src/codegen/submit/submitPostProcessor.h)
This post-emission pass is used only by compact submit mode. It receives the
complete readable C++ translation unit, removes comments and nonessential
whitespace without changing literals or preprocessor boundaries, and returns
the compact text. It does not participate in parsing, lowering, reachability,
or helper pruning.

## [src/semantic_analyze/errors.cpp](../../src/semantic_analyze/errors.cpp) and [src/semantic_analyze/errors.h](../../src/semantic_analyze/errors.h)
These files collect diagnostics and render user-facing source-mapped errors. If
an error message is confusing or a new source-level diagnostic needs to exist,
this is the place to update.

## [src/codegen/listsCppp.cpp](../../src/codegen/listsCppp.cpp) and [src/codegen/listsCppp.h](../../src/codegen/listsCppp.h)
This module handles list-specific parsing, literal support, and generated
helper machinery for list operations.

## Fast Orientation Symbols

### `src/compilerDriver.cpp`
- `runCompilerDriver(...)` is the top-level orchestration function and chooses
  the run or submit backend after shared semantic analysis.

### `src/parse/sourceSplitter.cpp`
- `splitTokenStream(...)` supplies parser-internal statement views.
- `splitPhysicalFragments(...)` handles token-level boundaries.
- `mergeLogicalFragments(...)` rejoins multi-line logical statements.

### `src/parse/astParser.cpp`
- `parseProgramAst(...)` builds the recursive, syntax-only program tree.
- `validateProgramAst(...)` checks spans, ownership, mandatory children, and
  block/closing-span attribution.

### `src/codegen/statementCompiler.cpp`
- `compileProgramAst(...)` is the analyzed-tree codegen entry point.
- `AstLowerer::compileStatement(...)` dispatches concrete statement nodes.
- `compileOwnedBlock(...)` owns scope entry, child traversal, and scope exit.

### `src/parse/controlFlow.cpp`
- `parseConditionHeaderDetailed(...)` produces source-aware diagnostics for
  `if`/`while`/`else if` headers.
- `parseForHeaderDetailed(...)` validates classic `for` syntax.
- `parseForEachHeader(...)` parses CP++ `for-in` headers with precise offsets.

### `src/codegen/programEmitter.cpp`
- `emitLoweredProgram(...)` serializes the final translation unit.
- `emitGeneratedLines(...)` writes queued generated lines while preserving
  source mappings.

### `src/codegen/submit/submitPostProcessor.cpp`
- `compactSubmitCpp(...)` performs lexical compaction on a completed readable
  translation unit; no lowering or code generation happens in this pass.
