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

// Serializes the fully lowered program from CompileContext into one .cpp file.
void emitTranslatedProgram(std::ostream& output, CompileContext& context);
