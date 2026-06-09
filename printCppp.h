#pragma once

#include "errors.h"

#include <map>
#include <set>
#include <string>
#include <vector>

struct PrintEmitResult {
    bool ok;
    std::string generatedStatement;
    std::vector<SourceRange> sourceRanges;
};

PrintEmitResult emitPrintStatement(
    const std::string& inputFile,
    int lineNumber,
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::set<std::string>& declaredVariables
);
