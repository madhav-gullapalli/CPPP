#include "submitCodegen.h"

#include "statementCompiler.h"
#include "submitLoopPruner.h"

void generateSubmitProgram(CompileContext& context, const PrunedAnalyzedProgramAst& program) {
    compileProgramAst(context, program.analyzed, CodegenMode::Submit);
    pruneUnusedSubmitLoopHelpers(context);
}
