/* Submit codegen entry point; accepts only the result of submit pruning. */

#pragma once

#include "compileContext.h"
#include "pruning/prunedAst.h"

void generateSubmitProgram(
    CompileContext& context,
    const PrunedAnalyzedProgramAst& program
);
