/*
 * compilerDriver.h
 *
 * Public entry point for one full compiler invocation.
 *
 * The driver owns the high-level pipeline:
 * - validate CLI arguments
 * - prepare CompileOptions
 * - parse a full-program AST, then lower it into generated C++
 * - emit the translated .cpp file
 * - optionally compile and/or run the result
 */

#pragma once

// Runs one complete compiler invocation from CLI parsing through optional
// native compilation and execution.
int runCompilerDriver(int argc, char* argv[]);
