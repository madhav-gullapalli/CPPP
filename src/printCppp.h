#pragma once

#include "errors.h"
#include "expressions.h"

#include <map>
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
    const std::map<std::string, CpppType>& declaredVariables
);

PrintEmitResult emitDescribeStatement(
    const std::string& inputFile,
    int lineNumber,
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, CpppType>& declaredVariables
);
