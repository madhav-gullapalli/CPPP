#pragma once

#include "errors.h"
#include "tokenizer.h"

#include <map>
#include <set>
#include <string>
#include <vector>

struct ExpressionEmitResult {
    bool ok;
    std::string generatedExpression;
    std::vector<SourceRange> sourceRanges;
};

ExpressionEmitResult emitExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::set<std::string>& declaredVariables
);

bool hasArithmeticOperator(const std::vector<Token>& tokens);
