# CP++ Compiler Pipeline

This document explains the current compiler pipeline in execution order. It is
meant to answer two practical questions quickly:

1. Where does this feature get handled?
2. If I change this stage, what data does the next stage depend on?

## End-To-End Flow

For one invocation such as:

```sh
build/cppp --cppp in.cppp --run
```

the code path is:

1. [`src/cppp.cpp`](../src/cppp.cpp) calls `runCompilerDriver(...)`.
2. [`src/compilerDriver.cpp`](../src/compilerDriver.cpp) validates CLI mode and
   builds `CompileOptions`.
3. [`src/compilerDriver.cpp`](../src/compilerDriver.cpp) reads and registers the
   complete source file, creates one `CompileContext`, and asks
   [`src/tokenizer.cpp`](../src/tokenizer.cpp) for one canonical `TokenStream`.
4. [`src/astParser.cpp`](../src/astParser.cpp) parses the complete stream into a
   recursive `ProgramAst`. It uses token-backed statement views internally but
   performs no symbol lookup, type checking, or C++ emission.
5. [`src/semanticAnalyzer.cpp`](../src/semanticAnalyzer.cpp) consumes the syntax
   AST, resolves and validates its meaning, and returns `AnalyzedProgramAst`.
6. [`src/statementCompiler.cpp`](../src/statementCompiler.cpp) consumes that
   valid analyzed representation through `compileProgramAst(...)`.
7. [`src/programEmitter.cpp`](../src/programEmitter.cpp) turns its output buffers
   into one readable generated C++ translation unit.
8. Compact `--submit` passes the complete unit through
   [`src/submitPostProcessor.cpp`](../src/submitPostProcessor.cpp); `--readable`
   skips this pass.
9. [`src/compilerDriver.cpp`](../src/compilerDriver.cpp) optionally invokes
   `g++`, prints compile diagnostics if that fails, and optionally runs the
   produced executable.

Most implementation details hang off one of those eight steps.

## Stage 1: CLI Entry

[`src/cppp.cpp`](../src/cppp.cpp) is intentionally tiny. It only forwards
`main(...)` to `runCompilerDriver(...)`.

Practical takeaway: if you want to understand real compiler behavior, skip
`cppp.cpp` after confirming that it is just a trampoline.

## Stage 2: Driver Setup

[`src/compilerDriver.cpp`](../src/compilerDriver.cpp) owns the overall workflow.
It decides:

- which mode is active: token inspection, AST inspection, transpile only,
  compile, run, or submit
- where the generated `.cpp` should be written
- where the compiled executable should live
- whether expression/runtime checks should stay enabled

Important driver responsibilities:

- clear stale diagnostic/runtime-helper state before each invocation
- populate `CompileOptions`
- create the shared `CompileContext`
- construct the canonical token stream and full-program AST, then call lowering
- emit the final C++ file
- optionally call `g++`
- optionally execute the produced binary

`--submit` has one extra cleanup step: `pruneSubmitLoopHelpers(...)` removes
loop helper artifacts that are useful during richer lowering but unnecessary in
the final contest-style output.

## Stage 3: Canonical Tokenization and Full-Program Parsing

[`src/tokenizer.cpp`](../src/tokenizer.cpp) scans the complete source file once.
The resulting `TokenStream` owns the normalized source text and an ordered token
sequence ending in exactly one `EndOfFile` token. Every token records its kind,
text, line/column range, byte offsets, and canonical diagnostic `SourceSpan`.

Whitespace is trivia and is not emitted. Line comments and block braces are
explicit tokens. The `--tokens` mode prints this representation as JSON lines.

[`src/sourceSplitter.cpp`](../src/sourceSplitter.cpp) then groups tokens into
temporary parser-internal `SourceFragment` views. It does not lex raw text, and
these views do not survive into `ProgramAst` or semantic lowering.

`SourceFragment` carries:

- the original source line number
- the original starting column
- a rebased token view ending in `EndOfFile`
- text reconstructed from those tokens for parser classification and recovery
- the canonical source span

Key splitter responsibilities:

- group at top-level `;` and block-brace tokens
- avoid grouping boundaries inside literals and nested delimiters
- merge continuation lines back into one logical statement
- preserve comments in a form later stages can still emit or diagnose cleanly

[`src/astParser.cpp`](../src/astParser.cpp) consumes those views and constructs
one recursive [`ProgramAst`](../src/programAst.h). Functions, aggregates,
conditionals, loops, and their continuation branches own nested `BlockAst`
objects; declarations, assignments, returns, and expressions have dedicated
statement nodes. Syntactic types remain unresolved `TypeSyntax` values.

AST construction does not read `CompileContext` symbol tables and suppresses
normal semantic diagnostics. Malformed syntax is retained in recovery metadata
so direct lowering can still issue user-facing errors. The driver
validates ownership/span/coverage invariants after parsing. `--ast` then prints
the tree through [`src/astPrinter.cpp`](../src/astPrinter.cpp) and exits without
creating a generated `.cpp` file.

## Stage 4: Dedicated Semantic Analysis

[`src/semanticAnalyzer.cpp`](../src/semanticAnalyzer.cpp) walks `ProgramAst`
scope directly. It registers aggregate and function signatures, resolves
`TypeSyntax`, attaches inferred types and resolved symbols/callables, records
implicit conversions, validates control-flow context and all-path returns, and
computes aggregate dependency order. Invalid programs stop here. The
deterministic `--semantic` mode prints these annotations without generating C++.

## Stage 5: Analyzed AST Codegen

[`src/statementCompiler.cpp`](../src/statementCompiler.cpp) is the center of
the backend. `compileProgramAst(...)` accepts `AnalyzedProgramAst` and dispatches
on concrete nodes for declarations, assignments, functions, aggregates,
control flow, returns, and simple control statements. Recursive `BlockAst`
ownership drives scope entry and exit. No whole-program flattening or
statement-level reparsing occurs after AST construction.

Returns, conditions, contextual initializers, aggregate order, and `rep` counts
emit from semantic nodes. Assignment, input/comparator declarations, and the
specialized print/list statement emitters still accept AST-attributed token
slices as localized compatibility adapters; this is the remaining migration
debt, not a second statement parser.

This stage is responsible for:

- function and aggregate lowering from their AST nodes
- recursive block-depth and lexical-scope tracking
- scope tracking for declared variables
- AST-owned branch/loop completion handling and scope cleanup
- routing work to specialized helpers
- queueing final generated lines into `CompileContext`

The file relies on helper modules for specific domains:

- [`src/typeDeclarations.cpp`](../src/typeDeclarations.cpp) for declarations
- [`src/assignmentCppp.cpp`](../src/assignmentCppp.cpp) for assignments
- [`src/expressionParser.cpp`](../src/expressionParser.cpp) for expressions
- [`src/controlFlow.cpp`](../src/controlFlow.cpp) for parser-time control-flow headers
- [`src/printCppp.cpp`](../src/printCppp.cpp) for `print(...)`
- [`src/listsCppp.cpp`](../src/listsCppp.cpp) for list-specific syntax/support
- [`src/functions.cpp`](../src/functions.cpp) for function metadata

Practical rule: add syntax structure in `astParser.cpp`, represent it in
`programAst.h`, then lower that node in `compileProgramAst(...)`.

## Stage 6: Shared State via `CompileContext`

[`src/compileContext.h`](../src/compileContext.h) defines the data shared between
direct AST lowering and final emission.

The most important `CompileContext` fields are:

- `sourceLines`: original CP++ source text for diagnostics
- `cppToCpppLine`: generated-C++ line to original-CP++ line mapping
- `sourceRanges`: finer-grained source range mapping used by diagnostics
- `declaredVariables`: current visible variable types
- `declaredFunctions`: known function signatures
- `generatedTopLevelLines`: emitted declarations/support code outside `main`
- `generatedFunctionLines`: emitted function bodies
- `generatedMainLines`: emitted statements that will go inside generated `main`
- block/function tracking fields such as `blockDepth`, `blockKinds`,
  `blockBreakFlags`, `inFunction`, and `outputTarget`

If two stages need to communicate, they almost always do it by mutating or
reading this struct.

## Stage 7: Control-Flow Header Parsing

[`src/controlFlow.cpp`](../src/controlFlow.cpp) handles a narrower but important
job: parsing the header parts of structured control flow.

Examples:

- `if (cond) {`
- `while (cond) {`
- `else if (cond) {`
- `for (init; cond; iter) {`
- `for (int x in values) {`

Why this is split out:

- the syntax rules are specialized enough to deserve focused helpers
- `astParser.cpp` needs source-aware parse results without owning every
  header-token detail itself
- those offsets and token slices are stored on AST nodes for later diagnostics

Important nuance: this module is parser-internal. It does not participate in
semantic lowering; brace emission, scope effects, and generated helper variables
are driven by recursive AST nodes in `statementCompiler.cpp`.

## Stage 8: Final Program Emission

[`src/programEmitter.cpp`](../src/programEmitter.cpp) converts the generated
buffers in `CompileContext` into the final `.cpp` file.

It emits, in order:

1. standard-library includes
2. runtime support preamble from [`src/typesCppp.cpp`](../src/typesCppp.cpp)
3. optional runtime source-line table for `--run`
4. queued top-level generated lines
5. queued generated function definitions
6. generated `main()` scaffolding
7. queued generated main-body lines
8. optional runtime error translation wrapper for `--run`

Emission itself always produces ordinary readable C++. Compact submit mode
buffers that complete translation unit and then passes it through
[`src/submitPostProcessor.cpp`](../src/submitPostProcessor.cpp). This keeps
whitespace compaction separate from parsing, lowering, reachability, helper
pruning, and serialization. `--submit --readable` changes only this final
post-processing decision.

## Stage 9: Native Compile and Run

After emission, [`src/compilerDriver.cpp`](../src/compilerDriver.cpp) may:

- create the output `build/` directory
- invoke `g++` on the generated `.cpp`
- translate native compile errors back to CP++ source locations
- run the produced executable if `--run` was requested

This means the driver is responsible for both "compile CP++ to C++" and "bridge
the generated C++ world back to the original CP++ world when something goes
wrong."

## Where To Start For Common Tasks

- New statement syntax: start in [`src/programAst.h`](../src/programAst.h) and
  [`src/astParser.cpp`](../src/astParser.cpp), then add direct lowering in
  [`src/statementCompiler.cpp`](../src/statementCompiler.cpp).
- New expression/operator behavior: start in
  [`src/expressionParser.cpp`](../src/expressionParser.cpp) and
  [`src/expressions.cpp`](../src/expressions.cpp).
- Wrong compile/runtime diagnostic location: inspect
  [`src/compileContext.h`](../src/compileContext.h),
  [`src/programEmitter.cpp`](../src/programEmitter.cpp), and
  [`src/errors.cpp`](../src/errors.cpp).
- Wrong brace/statement boundary behavior: inspect
  [`src/sourceSplitter.cpp`](../src/sourceSplitter.cpp).
- Wrong generated top-level vs function vs main placement: inspect
  [`src/compileContext.h`](../src/compileContext.h),
  [`src/statementCompiler.cpp`](../src/statementCompiler.cpp), and
  [`src/programEmitter.cpp`](../src/programEmitter.cpp).

## Mental Model

The easiest way to keep the codebase straight is this:

- `compilerDriver.cpp` owns orchestration
- `sourceSplitter.cpp` owns parser-internal statement grouping
- `astParser.cpp` owns recursive program structure
- `semanticAnalyzer.cpp` owns meaning, typing, and semantic diagnostics
- `statementCompiler.cpp` owns analyzed-tree codegen
- helper modules own specialized subproblems
- `CompileContext` is the shared memory between stages
- `programEmitter.cpp` owns final file serialization
- `submitPostProcessor.cpp` owns optional submit-only lexical compaction

If you keep that model in mind, most of the compiler becomes much easier to
navigate.
