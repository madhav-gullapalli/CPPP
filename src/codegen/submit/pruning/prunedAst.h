/* Owning analyzed AST produced exclusively for submit codegen. */

#pragma once

#include "semanticAst.h"

#include <memory>
#include <set>
#include <string>

struct PrunedAnalyzedProgramAst {
    std::unique_ptr<ProgramAst> ownedProgram;
    AnalyzedProgramAst analyzed;
    std::set<std::string> reachableFunctions;
    std::set<std::string> reachableAggregates;
    std::set<std::string> reachableMethods;

    PrunedAnalyzedProgramAst() = default;
    PrunedAnalyzedProgramAst(PrunedAnalyzedProgramAst&&) = default;
    PrunedAnalyzedProgramAst& operator=(PrunedAnalyzedProgramAst&&) = default;
    PrunedAnalyzedProgramAst(const PrunedAnalyzedProgramAst&) = delete;
    PrunedAnalyzedProgramAst& operator=(const PrunedAnalyzedProgramAst&) = delete;
};

bool validatePrunedAnalyzedProgramAst(
    const PrunedAnalyzedProgramAst& program,
    const AnalyzedProgramAst& original,
    std::string& invariantError
);
