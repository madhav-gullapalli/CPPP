#pragma once

#include "errors.h"
#include "tokenizer.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

enum class PrimitiveType {
    Unknown,
    Bool,
    Char,
    Int,
    Float,
    List
};

struct Type {
    PrimitiveType primitive = PrimitiveType::Unknown;
    std::vector<Type> subtypes;

    Type() = default;
    Type(PrimitiveType primitive) : primitive(primitive) {}
    Type(PrimitiveType primitive, std::vector<Type> subtypes) :
        primitive(primitive),
        subtypes(std::move(subtypes)) {}
};

inline bool operator==(const Type& left, const Type& right) {
    return left.primitive == right.primitive && left.subtypes == right.subtypes;
}

inline bool operator!=(const Type& left, const Type& right) {
    return !(left == right);
}

inline bool operator==(const Type& left, PrimitiveType right) {
    return left.primitive == right && left.subtypes.empty();
}

inline bool operator!=(const Type& left, PrimitiveType right) {
    return !(left == right);
}

struct ExpressionEmitResult {
    bool ok;
    std::string generatedExpression;
    Type type;
    bool explicitCast;
    std::vector<SourceRange> sourceRanges;
};

struct LvalueEmitResult {
    bool ok;
    std::string generatedExpression;
    Type type;
    int sourceColumn;
};

struct InputArgument {
    std::string text;
    int column;
};

int primitiveArity(PrimitiveType primitive);
std::string cpppTypeName(const Type& type);
bool isStringType(const Type& type);
bool isImplicitlyConvertible(const Type& from, const Type& to);
std::string castExpressionTo(const std::string& expression, const Type& to);
std::string castExpressionTo(const std::string& expression, const Type& from, const Type& to);
Type declaredTypeForName(const std::string& name);
bool isInputCall(const std::vector<Token>& tokens);
std::string inputFunctionForType(const Type& type);
bool parseInputCall(const std::string& text, int startColumn, std::vector<InputArgument>& arguments);
bool emitInputCallForType(
    const std::string& inputFile,
    int lineNumber,
    const std::string& inputText,
    int inputColumn,
    const Type& targetType,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    std::string& emittedExpression
);
void setExpressionRuntimeChecksEnabled(bool enabled);

ExpressionEmitResult emitExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks = false
);

LvalueEmitResult emitLvalueExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks = false
);

bool hasArithmeticOperator(const std::vector<Token>& tokens);
