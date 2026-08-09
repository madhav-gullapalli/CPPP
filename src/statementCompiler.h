/*
 * statementCompiler.h
 *
 * Main statement-lowering stage.
 *
 * This stage consumes the canonical whole-file TokenStream, derives temporary
 * statement views, updates scope state, and queues generated C++ lines.
 */

#pragma once

#include "compileContext.h"
#include "programAst.h"

#include <vector>

// Transitional semantic/lowering pass. Syntax has already been parsed into a
// recursive ProgramAst before this stage begins.
void compileProgramAst(CompileContext& context, const ProgramAst& program);
