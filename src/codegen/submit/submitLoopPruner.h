/* Submit-only cleanup of unused lowered loop-completion flags. */

#pragma once

#include "compileContext.h"

void pruneUnusedSubmitLoopHelpers(CompileContext& context);
