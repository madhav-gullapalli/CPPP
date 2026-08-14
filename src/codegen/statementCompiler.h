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

enum class CodegenMode {
    Run,
    Submit
};

// Codegen pass over the analyzed recursive ProgramAst.
void compileProgramAst(
    CompileContext& context,
    const AnalyzedProgramAst& program,
    CodegenMode mode
);
