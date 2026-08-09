/*
 * assignmentCppp.h
 *
 * Declares the assignment emission helpers and result types used by the assignment lowering pass.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "errors.h"
#include "expressions.h"

#include <map>
#include <string>
#include <vector>

// AssignmentEmitResult implements the AssignmentEmitResult behavior for the assignmentCppp.h module.
struct AssignmentEmitResult {
    bool matched;
    bool ok;
    std::string generatedStatement;
    std::vector<SourceRange> sourceRanges;
};

// Emits an assignment whose statement structure was already established by
// ProgramAst. Token slices are expression-level adapters, not statement input.
AssignmentEmitResult emitParsedAssignment(
    const std::string& inputFile,
    int lineNumber,
    int statementColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    bool emitRuntimeChecks,
    const std::string& operation,
    const Token& operationToken,
    const std::vector<std::vector<Token>>& targetTokens,
    const std::vector<size_t>& targetOffsets,
    const std::vector<std::vector<Token>>& valueTokens,
    const std::vector<size_t>& valueOffsets
);
