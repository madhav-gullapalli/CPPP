/* Dedicated semantic-analysis stage for ProgramAst. */

#pragma once

#include "compileContext.h"
#include "semanticAst.h"

AnalyzedProgramAst analyzeProgramAst(CompileContext& context, ProgramAst& program);

bool validateAnalyzedProgramAst(
    const AnalyzedProgramAst& program,
    std::string& invariantError
);
