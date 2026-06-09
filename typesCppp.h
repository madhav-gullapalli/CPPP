#pragma once

#include "errors.h"
#include "expressions.h"

#include <map>
#include <string>
#include <vector>

struct TypeEmitResult {
    bool matched;
    bool ok;
    std::string generatedStatement;
    std::vector<SourceRange> sourceRanges;
};

std::vector<std::string> typeSupportPreamble();

TypeEmitResult emitTypeDeclaration(
    const std::string& inputFile,
    int lineNumber,
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    std::map<std::string, CpppType>& declaredVariables
);
