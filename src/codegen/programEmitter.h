/*
 * programEmitter.h
 *
 * Final pure-transpilation stage.
 *
 * The emitter takes the already-lowered line buffers stored in CompileContext
 * and wraps them in a complete C++ translation unit: includes, runtime
 * preamble, optional runtime-diagnostic scaffolding, generated functions, and
 * main().
 */

#pragma once

#include "compileContext.h"

#include <ostream>
#include <string>
#include <vector>

// Shared serialization used by the explicit run and submit emitter frontends.
// User-declaration reachability has already been decided before this function.
void emitLoweredProgram(
    std::ostream& output,
    CompileContext& context,
    const std::vector<std::string>& preambleLines,
    bool runtimeDiagnostics
);
