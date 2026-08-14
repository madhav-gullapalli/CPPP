/* Run/default codegen entry point. */

#pragma once

#include "compileContext.h"
#include "semanticAst.h"

void generateRunProgram(
    CompileContext& context,
    const AnalyzedProgramAst& program
);
