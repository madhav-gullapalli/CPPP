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

#include <algorithm>
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
    Range,
    List,
    Stack,
    Queue,
    Deque,
    Heap,
    Set,
    Map,
    Pair,
    Function,
    Struct,
    Class
};

// Type implements the Type behavior for the expressions.h module.
struct Type {
    PrimitiveType primitive = PrimitiveType::Unknown;
    std::vector<Type> subtypes;
    // Function parameter modes, one entry per subtype after the return type.
    // A true entry means calls through this function value deep-copy that argument.
    std::vector<bool> functionParameterCopy;
    std::string name;

    Type() = default;
    Type(PrimitiveType primitive) : primitive(primitive) {}
    Type(PrimitiveType primitive, std::vector<Type> subtypes) :
        primitive(primitive),
        subtypes(std::move(subtypes)) {}
    Type(PrimitiveType primitive, std::string name) :
        primitive(primitive),
        name(std::move(name)) {}
};

inline bool operator==(const Type& left, const Type& right) {
    bool functionModesEqual = true;
    if (left.primitive == PrimitiveType::Function && right.primitive == PrimitiveType::Function) {
        const size_t count = std::max(left.functionParameterCopy.size(), right.functionParameterCopy.size());
        for (size_t i = 0; i < count; ++i) {
            const bool leftCopy = i < left.functionParameterCopy.size() && left.functionParameterCopy[i];
            const bool rightCopy = i < right.functionParameterCopy.size() && right.functionParameterCopy[i];
            if (leftCopy != rightCopy) { functionModesEqual = false; break; }
        }
    }
    return left.primitive == right.primitive && left.subtypes == right.subtypes &&
        functionModesEqual && left.name == right.name;
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
    std::vector<Token> tokens;
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
bool isStackType(const Type& type);
bool isQueueType(const Type& type);
bool isDequeType(const Type& type);
bool isHeapType(const Type& type);
bool isLinearDataStructureType(const Type& type);
bool isRangeType(const Type& type);
bool isSetType(const Type& type);
bool isMapType(const Type& type);
bool isPairType(const Type& type);
bool isFunctionType(const Type& type);
Type functionTypeForSignature(const FunctionSignature& signature);
bool isStructType(const Type& type);
bool isClassType(const Type& type);
bool isInlineStructType(const Type& type);
bool isCollectionType(const Type& type);
// isImplicitlyConvertible returns whether the supplied input satisfies the relevant condition.
bool isImplicitlyConvertible(const Type& from, const Type& to);
bool canExplicitlyCastType(const Type& from, const Type& to);
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
bool parseInputCall(const std::vector<Token>& tokens, int startColumn, std::vector<InputArgument>& arguments);
bool emitInputCallForType(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& inputTokens,
    int inputColumn,
    const Type& targetType,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    std::string& emittedExpression
);
void setExpressionRuntimeChecksEnabled(bool enabled);
void setDeclaredFunctionsForExpressions(const std::map<std::string, FunctionSignature>* declaredFunctions);
void setDeclaredStructsForExpressions(const std::map<std::string, std::map<std::string, Type>>* declaredStructs);
void setDeclaredClassNamesForExpressions(const std::set<std::string>* declaredClassNames);
void setDeclaredStructFieldOrdersForExpressions(const std::map<std::string, std::vector<std::string>>* fieldOrders);
void setDeclaredStructMethodsForExpressions(const std::map<std::string, std::map<std::string, FunctionSignature>>* methods);
const std::map<std::string, Type>* declaredStructFieldsForName(const std::string& name);
const std::vector<std::string>* declaredStructFieldOrderForName(const std::string& name);
const FunctionSignature* declaredStructMethodForType(const Type& type, const std::string& name);
std::vector<std::string> declaredCustomTypeNames();
std::vector<std::string> declaredStructMethodNamesForType(const Type& type);
std::unique_ptr<Expr> parseExpressionAst(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& expressionTokens,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables
);
std::unique_ptr<Expr> parseExpressionAst(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& expressionTokens,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions
);

ExpressionEmitResult emitExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& expressionTokens,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks = false
);

ExpressionEmitResult emitExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& expressionTokens,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    bool emitRuntimeChecks = false
);

LvalueEmitResult emitLvalueExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& expressionTokens,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks = false
);

LvalueEmitResult emitLvalueExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& expressionTokens,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    bool emitRuntimeChecks = false
);

// hasArithmeticOperator returns whether the supplied input satisfies the relevant condition.
bool hasArithmeticOperator(const std::vector<Token>& tokens);
