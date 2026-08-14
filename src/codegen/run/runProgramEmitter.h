/* Run/default translation-unit emitter entry point. */

#pragma once

#include "compileContext.h"

#include <ostream>

void emitRunProgram(std::ostream& output, CompileContext& context);
