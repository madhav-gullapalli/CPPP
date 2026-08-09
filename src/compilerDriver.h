/*
 * compilerDriver.h
 *
 * Public entry point for one full compiler invocation.
 *
 * The driver owns the high-level pipeline:
 * - validate CLI arguments
 * - prepare CompileOptions
 * - parse a full-program AST, analyze it, then lower known-valid semantics
 * - emit the translated .cpp file
 * - optionally compile and/or run the result
 */

#pragma once

// Runs one complete compiler invocation from CLI parsing through optional
// native compilation and execution.
int runCompilerDriver(int argc, char* argv[]);
