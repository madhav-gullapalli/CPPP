#pragma once

#include "errors.h"
#include "expressions.h"
#include "typesCppp.h"

#include <map>
#include <string>
#include <vector>

struct ListEmitResult {
    bool matched;
    bool ok;
    std::string generatedStatement;
    std::vector<SourceRange> sourceRanges;
};

std::vector<RuntimeHelper> listRuntimeHelpers();

ListEmitResult emitListStatement(
    const std::string& inputFile,
    int lineNumber,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks
);
