#pragma once

#include "errors.h"
#include "expressions.h"

#include <map>
#include <string>
#include <vector>

struct AssignmentEmitResult {
    bool matched;
    bool ok;
    std::string generatedStatement;
    std::vector<SourceRange> sourceRanges;
};

AssignmentEmitResult emitAssignmentStatement(
    const std::string& inputFile,
    int lineNumber,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, CpppType>& declaredVariables
);
