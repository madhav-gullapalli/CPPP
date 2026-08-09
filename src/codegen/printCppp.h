/*
 * printCppp.h
 *
 * Declares the print-emission result structures and helpers.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "errors.h"
#include "expressions.h"

#include <map>
#include <string>
#include <vector>

// PrintEmitResult prints the relevant diagnostic or output text.
struct PrintEmitResult {
    bool ok;
    std::string generatedStatement;
    std::vector<SourceRange> sourceRanges;
};

PrintEmitResult emitPrintStatement(
    const std::string& inputFile,
    int lineNumber,
    int statementStartColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::vector<Token>& sourceTokens
);

PrintEmitResult emitDescribeStatement(
    const std::string& inputFile,
    int lineNumber,
    int statementStartColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::vector<Token>& sourceTokens
);
