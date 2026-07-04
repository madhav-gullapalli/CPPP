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

AssignmentEmitResult emitAssignmentStatement(
    const std::string& inputFile,
    int lineNumber,
    int statementColumn,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks = false
);
