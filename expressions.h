#pragma once

#include "errors.h"
#include "tokenizer.h"

#include <map>
#include <set>
#include <string>
#include <vector>

enum class CpppType {
    Unknown,
    Bool,
    Char,
    Int,
    BigInt,
    Float,
    BigFloat
};

struct ExpressionEmitResult {
    bool ok;
    std::string generatedExpression;
    CpppType type;
    bool explicitCast;
    std::vector<SourceRange> sourceRanges;
};

std::string cpppTypeName(CpppType type);
bool isImplicitlyConvertible(CpppType from, CpppType to);
std::string castExpressionTo(const std::string& expression, CpppType to);
CpppType declaredTypeForName(const std::string& name);
bool isInputCall(const std::vector<Token>& tokens);
std::string inputFunctionForType(CpppType type);

ExpressionEmitResult emitExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, CpppType>& declaredVariables
);

bool hasArithmeticOperator(const std::vector<Token>& tokens);
