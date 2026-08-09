/*
 * statementCompiler.h
 *
 * Main statement-lowering stage.
 *
 * This stage recursively consumes ProgramAst nodes, updates semantic scope
 * state, and queues generated C++ lines.
 */

#pragma once

#include "compileContext.h"
#include "programAst.h"

#include <vector>

// Semantic/codegen pass over the recursive ProgramAst. It never reparses
// statement syntax or reconstructs parser fragments.
void compileProgramAst(CompileContext& context, const ProgramAst& program);
