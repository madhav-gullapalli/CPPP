/*
 * expressions.h
 *
 * Defines expression-related types, primitive types, and helper result structures.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "errors.h"
#include "tokenizer.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

enum class PrimitiveType {
    Unknown,
    Void,
    Bool,
    Char,
    Int,
    Float,
    List,
    Set,
    Map,
    Pair
};

// Type implements the Type behavior for the expressions.h module.
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

// ExpressionEmitResult implements the ExpressionEmitResult behavior for the expressions.h module.
struct ExpressionEmitResult {
    bool ok;
    std::string generatedExpression;
    Type type;
    bool explicitCast;
    std::vector<SourceRange> sourceRanges;
};

// LvalueEmitResult implements the LvalueEmitResult behavior for the expressions.h module.
struct LvalueEmitResult {
    bool ok;
    std::string generatedExpression;
    Type type;
    int sourceColumn;
};

// InputArgument implements the InputArgument behavior for the expressions.h module.
struct InputArgument {
    std::string text;
    int column;
};

// FunctionSignature implements the FunctionSignature behavior for the expressions.h module.
struct FunctionSignature;
// Expr implements the Expr behavior for the expressions.h module.
struct Expr;

// primitiveArity implements the primitiveArity behavior for the expressions.h module.
int primitiveArity(PrimitiveType primitive);
// cpppTypeName implements the cpppTypeName behavior for the expressions.h module.
std::string cpppTypeName(const Type& type);
// isStringType returns whether the supplied input satisfies the relevant condition.
bool isStringType(const Type& type);
bool isListType(const Type& type);
bool isSetType(const Type& type);
bool isMapType(const Type& type);
bool isPairType(const Type& type);
bool isCollectionType(const Type& type);
// isImplicitlyConvertible returns whether the supplied input satisfies the relevant condition.
bool isImplicitlyConvertible(const Type& from, const Type& to);
// castExpressionTo implements the castExpressionTo behavior for the expressions.h module.
std::string castExpressionTo(const std::string& expression, const Type& to);
// castExpressionTo implements the castExpressionTo behavior for the expressions.h module.
std::string castExpressionTo(const std::string& expression, const Type& from, const Type& to);
// declaredTypeForName implements the declaredTypeForName behavior for the expressions.h module.
Type declaredTypeForName(const std::string& name);
// isInputCall returns whether the supplied input satisfies the relevant condition.
bool isInputCall(const std::vector<Token>& tokens);
// inputFunctionForType implements the inputFunctionForType behavior for the expressions.h module.
std::string inputFunctionForType(const Type& type);
// parseInputCall parses inputcall for the compiler pipeline.
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
void setDeclaredFunctionsForExpressions(const std::map<std::string, FunctionSignature>* declaredFunctions);
std::unique_ptr<Expr> parseExpressionAst(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables
);
std::unique_ptr<Expr> parseExpressionAst(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions
);

ExpressionEmitResult emitExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks = false
);

ExpressionEmitResult emitExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
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

LvalueEmitResult emitLvalueExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    bool emitRuntimeChecks = false
);

// hasArithmeticOperator returns whether the supplied input satisfies the relevant condition.
bool hasArithmeticOperator(const std::vector<Token>& tokens);
