#include "runCodegen.h"

#include "statementCompiler.h"

void generateRunProgram(CompileContext& context, const AnalyzedProgramAst& program) {
    compileProgramAst(context, program, CodegenMode::Run);
}
