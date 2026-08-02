/*
 * listsCppp.h
 *
 * Declares list-related emission results and helper interfaces.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "errors.h"
#include "expressions.h"
#include "typesCppp.h"

#include <map>
#include <string>
#include <vector>

// ListEmitResult handles list-specific behavior for the compiler or runtime.
struct ListEmitResult {
    bool matched;
    bool ok;
    std::string generatedStatement;
    std::vector<SourceRange> sourceRanges;
};

struct ComparatorEmitResult {
    bool ok = false;
    std::string expression;
};

ComparatorEmitResult emitCollectionComparator(
    const std::string& inputFile,
    int lineNumber,
    const std::string& text,
    int column,
    const Type& itemType,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables
);

// listRuntimeHelpers handles list-specific behavior for the compiler or runtime.
std::vector<RuntimeHelper> listRuntimeHelpers();

ListEmitResult emitListStatement(
    const std::string& inputFile,
    int lineNumber,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks
);
