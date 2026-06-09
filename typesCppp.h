#pragma once

#include "errors.h"

#include <map>
#include <set>
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
    std::set<std::string>& declaredVariables
);
