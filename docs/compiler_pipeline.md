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
3. [`src/compilerDriver.cpp`](../src/compilerDriver.cpp) opens the input file
   and creates one `CompileContext`.
4. [`src/sourceSplitter.cpp`](../src/sourceSplitter.cpp) reads raw source and
   returns `vector<SourceFragment>`.
5. [`src/statementCompiler.cpp`](../src/statementCompiler.cpp) walks those
   fragments and fills the generated-output buffers inside `CompileContext`.
6. [`src/programEmitter.cpp`](../src/programEmitter.cpp) turns those buffers
   into one generated `.cpp` file.
7. [`src/compilerDriver.cpp`](../src/compilerDriver.cpp) optionally invokes
   `g++`, prints compile diagnostics if that fails, and optionally runs the
   produced executable.

Most implementation details hang off one of those seven steps.

## Stage 1: CLI Entry

[`src/cppp.cpp`](../src/cppp.cpp) is intentionally tiny. It only forwards
`main(...)` to `runCompilerDriver(...)`.

Practical takeaway: if you want to understand real compiler behavior, skip
`cppp.cpp` after confirming that it is just a trampoline.

## Stage 2: Driver Setup

[`src/compilerDriver.cpp`](../src/compilerDriver.cpp) owns the overall workflow.
It decides:

- which mode is active: transpile only, compile, run, or submit
- where the generated `.cpp` should be written
- where the compiled executable should live
- whether expression/runtime checks should stay enabled

Important driver responsibilities:

- clear stale diagnostic/runtime-helper state before each invocation
- populate `CompileOptions`
- create the shared `CompileContext`
- call the splitter and lowering stages in order
- emit the final C++ file
- optionally call `g++`
- optionally execute the produced binary

`--submit` has one extra cleanup step: `pruneSubmitLoopHelpers(...)` removes
loop helper artifacts that are useful during richer lowering but unnecessary in
the final contest-style output.

## Stage 3: Source Splitting

[`src/sourceSplitter.cpp`](../src/sourceSplitter.cpp) is the first true compiler
stage after file I/O. It does not perform semantic analysis. Its job is to make
later lowering simpler by normalizing raw text into `SourceFragment` records.

`SourceFragment` carries:

- the original source line number
- the original starting column
- the text for one logical statement fragment

Key splitter responsibilities:

- preserve original source lines in `CompileContext::sourceLines`
- split one physical line into multiple fragments at top-level `;`, `{`, and `}`
- avoid splitting inside strings, chars, or parenthesized expressions
- merge continuation lines back into one logical statement
- preserve comments in a form later stages can still emit or diagnose cleanly

## Stage 4: Statement Lowering

[`src/statementCompiler.cpp`](../src/statementCompiler.cpp) is the center of
the transpiler. `compileSourceFragments(...)` iterates through each
`SourceFragment` and decides what kind of statement it is and how it should be
lowered.

This stage is responsible for:

- function declaration detection at top level
- block-depth tracking
- scope tracking for declared variables
- close-brace handling and scope cleanup
- routing work to specialized helpers
- queueing final generated lines into `CompileContext`

The file relies on helper modules for specific domains:

- [`src/typeDeclarations.cpp`](../src/typeDeclarations.cpp) for declarations
- [`src/assignmentCppp.cpp`](../src/assignmentCppp.cpp) for assignments
- [`src/expressionParser.cpp`](../src/expressionParser.cpp) for expressions
- [`src/controlFlow.cpp`](../src/controlFlow.cpp) for control-flow headers
- [`src/printCppp.cpp`](../src/printCppp.cpp) for `print(...)`
- [`src/listsCppp.cpp`](../src/listsCppp.cpp) for list-specific syntax/support
- [`src/functions.cpp`](../src/functions.cpp) for function metadata

Practical rule: if a source feature feels statement-shaped, start in
`compileSourceFragments(...)` and follow the branch it takes.

## Stage 5: Shared State via `CompileContext`

[`src/compileContext.h`](../src/compileContext.h) defines the data shared across
stages. It is the glue between splitting, lowering, and final emission.

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
  `pendingLoopElse`, `inFunction`, and `outputTarget`

If two stages need to communicate, they almost always do it by mutating or
reading this struct.

## Stage 6: Control-Flow Header Parsing

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
- `statementCompiler.cpp` wants source-aware parse results without owning all
  the string/token details itself
- the parser returns offsets that make later diagnostics point at the right part
  of the user's source

Important nuance: this module parses headers, but it does not own the full
block-lowering lifecycle. Brace emission, scope effects, and generated helper
variables are still coordinated by `statementCompiler.cpp`.

## Stage 7: Final Program Emission

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

## Stage 8: Native Compile and Run

After emission, [`src/compilerDriver.cpp`](../src/compilerDriver.cpp) may:

- create the output `build/` directory
- invoke `g++` on the generated `.cpp`
- translate native compile errors back to CP++ source locations
- run the produced executable if `--run` was requested

This means the driver is responsible for both "compile CP++ to C++" and "bridge
the generated C++ world back to the original CP++ world when something goes
wrong."

## Where To Start For Common Tasks

- New statement syntax: start in
  [`src/statementCompiler.cpp`](../src/statementCompiler.cpp), then inspect
  [`src/statementParser.cpp`](../src/statementParser.cpp) and any specialized
  helper module it should delegate to.
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
- `sourceSplitter.cpp` owns statement-sized input fragments
- `statementCompiler.cpp` owns statement lowering
- helper modules own specialized subproblems
- `CompileContext` is the shared memory between stages
- `programEmitter.cpp` owns final file serialization

If you keep that model in mind, most of the compiler becomes much easier to
navigate.
