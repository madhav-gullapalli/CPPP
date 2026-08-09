/*
 * statementCompiler.h
 *
 * Main statement-lowering stage.
 *
 * This stage recursively consumes an already-valid AnalyzedProgramAst and
 * queues generated C++ lines.
 */

#pragma once

#include "compileContext.h"
#include "semanticAst.h"

#include <vector>

// Codegen pass over the analyzed recursive ProgramAst.
void compileProgramAst(CompileContext& context, const AnalyzedProgramAst& program);
