#include "runProgramEmitter.h"

#include "programEmitter.h"
#include "typesCppp.h"

void emitRunProgram(std::ostream& output, CompileContext& context) {
    emitLoweredProgram(output, context, typeSupportPreamble(), context.options.shouldRun);
}
