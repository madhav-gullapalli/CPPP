#pragma once

#include "errors.h"
#include "expressions.h"

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

struct RuntimeHelper {
    std::string name;
    std::vector<std::string> code;
    std::vector<std::string> deps;
    std::vector<std::string> triggers;
};

std::vector<RuntimeHelper> runtimeHelpers();
std::vector<std::string> typeSupportPreamble();
std::vector<std::string> typeSupportPreambleForSubmit(const std::set<std::string>& requiredHelpers);
void clearRequiredRuntimeHelpers();
void requireRuntimeHelper(const std::string& helperName);
const std::set<std::string>& requiredRuntimeHelpers();

TypeEmitResult emitTypeDeclaration(
    const std::string& inputFile,
    int lineNumber,
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    std::map<std::string, Type>& declaredVariables
);
