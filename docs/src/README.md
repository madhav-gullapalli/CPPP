# CP++ Source Overview

This is the quickest source-tree map for the current compiler. It is organized
around the real execution flow, not just a flat list of files.

For the full stage-by-stage walkthrough, start with
[docs/compiler_pipeline.md](../compiler_pipeline.md). Use this file when you
already know roughly which stage you need and want the shortest jump to the
right module.

## Pipeline In One Screen

The compiler pipeline today is:

1. `src/cppp.cpp` calls `runCompilerDriver(...)`.
2. `src/compilerDriver.cpp` validates CLI options, opens the input file, creates
   one `CompileContext`, and tokenizes the complete source once.
3. `src/sourceSplitter.cpp` groups the canonical `TokenStream` into token-backed
   logical statement views for the current parser.
4. `src/statementCompiler.cpp` consumes that stream and performs most lowering,
   using helper modules for expressions, types, assignments, print, lists,
   functions, and control-flow headers.
5. `src/programEmitter.cpp` turns the generated line buffers in
   `CompileContext` into one readable C++ translation unit.
6. `src/submitPostProcessor.cpp` optionally compacts the completed translation
   unit for `--submit`; `--readable` skips this post-processing pass.
7. `src/compilerDriver.cpp` optionally invokes `g++` and optionally runs the
   produced executable.

If you only read two files first, make them:

- `src/compilerDriver.cpp` for orchestration
- `src/statementCompiler.cpp` for actual statement-by-statement lowering

## [src/cppp.cpp](../../src/cppp.cpp)
This is a tiny CLI entry point. It just forwards `main(...)` to
`runCompilerDriver(...)`.

## [src/compilerDriver.cpp](../../src/compilerDriver.cpp) and [src/compilerDriver.h](../../src/compilerDriver.h)
These files own one end-to-end compiler invocation. The driver validates CLI
flags, builds `CompileOptions`, opens the input file, creates the shared
`CompileContext`, calls the splitter and lowering stages, emits the output
`.cpp`, and optionally invokes `g++` or runs the executable.

## [src/compileContext.h](../../src/compileContext.h)
This header defines the shared state passed through the entire pipeline. It is
where source mapping, symbol tables, block/function state, and generated output
buffers live.

## [src/tokenizer.cpp](../../src/tokenizer.cpp) and [src/tokenizer.h](../../src/tokenizer.h)
These files implement the canonical whole-source `TokenStream`, token kinds,
literal/comment lexing, byte offsets, and source spans. The `--tokens` driver
mode prints this stream for inspection and snapshot tests.

## [src/sourceSplitter.cpp](../../src/sourceSplitter.cpp) and [src/sourceSplitter.h](../../src/sourceSplitter.h)
This stage groups an already-tokenized file into statement-sized compatibility
views. It handles top-level terminators, continuation lines, comments, and block
braces without scanning raw source again.

## [src/statementParser.cpp](../../src/statementParser.cpp) and [src/statementParser.h](../../src/statementParser.h)
These files classify logical statements into a small statement AST. They are
more about recognizing statement shape than doing full lowering.

## [src/stmtAst.h](../../src/stmtAst.h)
This header declares the statement AST node types used during statement-level
classification.

## [src/controlFlow.cpp](../../src/controlFlow.cpp) and [src/controlFlow.h](../../src/controlFlow.h)
These helpers parse control-flow headers like `if (...)`, `while (...)`,
classic `for (...)`, and CP++ `for (T x in iterable)`. Full block lowering is
still coordinated by `statementCompiler.cpp`.

## [src/typeDeclarations.cpp](../../src/typeDeclarations.cpp)
This file parses and validates variable declarations and type syntax, then emits
their generated C++ form.

## [src/assignmentCppp.cpp](../../src/assignmentCppp.cpp) and [src/assignmentCppp.h](../../src/assignmentCppp.h)
These files handle assignment parsing and lowering, including compound
assignments and assignment-specific rewrites.

## [src/printCppp.cpp](../../src/printCppp.cpp) and [src/printCppp.h](../../src/printCppp.h)
This pair parses `print(...)` and emits the generated output logic, including
delimiter and list-aware behavior.

## [src/expressions.cpp](../../src/expressions.cpp) and [src/expressions.h](../../src/expressions.h)
These files provide the shared expression/type utility layer used across the
compiler: primitive/list type helpers, conversions, casts, and runtime-helper
tracking.

## [src/exprAst.h](../../src/exprAst.h)
This header contains the expression AST used during expression analysis and
emission.

## [src/expressionParser.cpp](../../src/expressionParser.cpp) and [src/expressionParser.h](../../src/expressionParser.h)
This is the main expression-analysis stage. It parses expression syntax, checks
types, inserts conversions, and emits generated C++ expressions plus runtime
checks when enabled.

## [src/functions.cpp](../../src/functions.cpp) and [src/functions.h](../../src/functions.h)
These files manage function signatures, parameter metadata, declaration parsing,
and function-call metadata.

## [src/typesCppp.cpp](../../src/typesCppp.cpp) and [src/typesCppp.h](../../src/typesCppp.h)
This module provides emitted runtime support and type-emission machinery used by
the generated C++ program.

## [src/statementCompiler.cpp](../../src/statementCompiler.cpp) and [src/statementCompiler.h](../../src/statementCompiler.h)
This is the central lowering stage. It receives the canonical `TokenStream`,
iterates through its temporary statement views, tracks block nesting and scope,
recognizes functions/control flow, and
routes specialized work to the expression, assignment, list, print, and type
helpers before queueing generated C++ lines into `CompileContext`.

## [src/programEmitter.cpp](../../src/programEmitter.cpp) and [src/programEmitter.h](../../src/programEmitter.h)
This is the final emission stage. It writes includes, runtime helper preamble,
optional runtime-diagnostic scaffolding, emitted top-level declarations,
emitted function bodies, and generated `main()`.

## [src/submitPostProcessor.cpp](../../src/submitPostProcessor.cpp) and [src/submitPostProcessor.h](../../src/submitPostProcessor.h)
This post-emission pass is used only by compact submit mode. It receives the
complete readable C++ translation unit, removes comments and nonessential
whitespace without changing literals or preprocessor boundaries, and returns
the compact text. It does not participate in parsing, lowering, reachability,
or helper pruning.

## [src/errors.cpp](../../src/errors.cpp) and [src/errors.h](../../src/errors.h)
These files collect diagnostics and render user-facing source-mapped errors. If
an error message is confusing or a new source-level diagnostic needs to exist,
this is the place to update.

## [src/listsCppp.cpp](../../src/listsCppp.cpp) and [src/listsCppp.h](../../src/listsCppp.h)
This module handles list-specific parsing, literal support, and generated
helper machinery for list operations.

## Fast Orientation Symbols

### `src/compilerDriver.cpp`
- `runCompilerDriver(...)` is the top-level orchestration function.
- `pruneSubmitLoopHelpers(...)` strips loop-else helper artifacts that submit
  mode does not need.

### `src/sourceSplitter.cpp`
- `splitTokenStream(...)` groups canonical tokens into statement views.
- `splitPhysicalFragments(...)` handles token-level boundaries.
- `mergeLogicalFragments(...)` rejoins multi-line logical statements.

### `src/statementCompiler.cpp`
- `compileTokenStream(...)` is the main statement-lowering loop.
- `emitConditionHeader(...)` lowers analyzed `if`/`while` conditions.
- `emitForPart(...)` lowers pieces of classic `for (init; cond; iter)` loops.

### `src/controlFlow.cpp`
- `parseConditionHeaderDetailed(...)` produces source-aware diagnostics for
  `if`/`while`/`else if` headers.
- `parseForHeaderDetailed(...)` validates classic `for` syntax.
- `parseForEachHeader(...)` parses CP++ `for-in` headers with precise offsets.

### `src/programEmitter.cpp`
- `emitTranslatedProgram(...)` serializes the final translation unit.
- `emitGeneratedLines(...)` writes queued generated lines while preserving
  source mappings.

### `src/submitPostProcessor.cpp`
- `compactSubmitCpp(...)` performs lexical compaction on a completed readable
  translation unit; no lowering or code generation happens in this pass.
