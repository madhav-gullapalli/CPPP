/* Submit-only reachability and analyzed-AST pruning stage. */

#pragma once

#include "prunedAst.h"

PrunedAnalyzedProgramAst pruneAnalyzedProgramForSubmit(
    const AnalyzedProgramAst& analyzed
);
