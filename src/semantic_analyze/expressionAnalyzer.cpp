/*
 * expressionAnalyzer.cpp
 *
 * Analyzes existing expression AST nodes and records semantic types and conversions.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "expressionAnalyzer.h"

#include "typesCppp.h"

#include <algorithm>
#include <climits>
#include <memory>

namespace {
Type builtinFunctionType(const std::string& name) {
    const Type integer = PrimitiveType::Int;
    if (name == "sum") {
        return Type(PrimitiveType::Function, {integer, Type(PrimitiveType::List, {integer})});
    }
    if (name == "min" || name == "max") {
        return Type(PrimitiveType::Function, {integer, integer, integer, integer, integer});
    }
    if (name == "abs") {
        return Type(PrimitiveType::Function, {integer, integer});
    }
    return PrimitiveType::Unknown;
}

// ExpressionAnalyzer analyzes existing expression AST nodes and records semantic facts.
class ExpressionAnalyzer {
public:
    ExpressionAnalyzer(
        const std::string& inputFile,
        int lineNumber,
        const std::map<int, std::string>& sourceLines,
        const std::map<std::string, Type>& declaredVariables,
        const std::map<std::string, FunctionSignature>& declaredFunctions,
        const std::map<std::string, int>* futureVariableLines = nullptr
    ) :
        inputFile(inputFile),
        lineNumber(lineNumber),
        sourceLines(sourceLines),
        declaredVariables(declaredVariables),
        declaredFunctions(declaredFunctions),
        futureVariableLines(futureVariableLines) {}

// analyze analyzes the construct and validates its semantics.
    bool analyze(Expr& expr) {
        if (expr.semanticAnalyzed) {
            return expr.semanticValid;
        }
        bool result = false;
        if (auto* literal = dynamic_cast<LiteralExpr*>(&expr)) {
            result = analyzeLiteral(*literal);
        } else if (auto* variable = dynamic_cast<VariableExpr*>(&expr)) {
            result = analyzeVariable(*variable);
        } else if (auto* field = dynamic_cast<FieldExpr*>(&expr)) {
            result = analyzeField(*field);
        } else if (auto* unary = dynamic_cast<UnaryExpr*>(&expr)) {
            result = analyzeUnary(*unary);
        } else if (auto* binary = dynamic_cast<BinaryExpr*>(&expr)) {
            result = analyzeBinary(*binary);
        } else if (auto* cast = dynamic_cast<CastExpr*>(&expr)) {
            result = analyzeCast(*cast);
        } else if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
            result = analyzeCall(*call);
        } else if (auto* index = dynamic_cast<IndexExpr*>(&expr)) {
            result = analyzeIndex(*index);
        } else if (auto* slice = dynamic_cast<SliceExpr*>(&expr)) {
            result = analyzeSlice(*slice);
        } else if (auto* list = dynamic_cast<ListLiteralExpr*>(&expr)) {
            result = analyzeListLiteral(*list);
        } else if (auto* set = dynamic_cast<SetLiteralExpr*>(&expr)) {
            result = analyzeSetLiteral(*set);
        } else if (auto* map = dynamic_cast<MapLiteralExpr*>(&expr)) {
            result = analyzeMapLiteral(*map);
        } else if (auto* pair = dynamic_cast<PairLiteralExpr*>(&expr)) {
            result = analyzePairLiteral(*pair);
        }
        expr.semanticAnalyzed = true;
        expr.semanticValid = result;
        return result;
    }

private:
    const std::string& inputFile;
    int lineNumber;
    const std::map<int, std::string>& sourceLines;
    const std::map<std::string, Type>& declaredVariables;
    const std::map<std::string, FunctionSignature>& declaredFunctions;
    const std::map<std::string, int>* futureVariableLines;

    void report(int column, const std::string& message) const {
        recordSourceError(inputFile, lineNumber, column, message, sourceLines);
    }

    void reportNameSuggestion(
        int column,
        SourceSpan span,
        const std::string& message,
        const std::string& misspelled,
        const std::vector<std::string>& candidates,
        const std::string& fallbackHelp = ""
    ) const {
        Diagnostic diagnostic;
        diagnostic.message = message;
        diagnostic.labels.push_back({
            span.valid()
                ? span
                : sourceTokenSpan(inputFile, sourceLines, lineNumber, column),
            "",
            true
        });
        const std::string closest = closestDiagnosticCandidate(
            misspelled,
            candidates
        );
        if (!closest.empty()) {
            diagnostic.suggestions.push_back({
                diagnostic.labels.front().span,
                closest,
                "did you mean '" + closest + "'?",
                SuggestionApplicability::MaybeIncorrect
            });
        } else if (!fallbackHelp.empty()) {
            diagnostic.helps.push_back(fallbackHelp);
        }
        recordDiagnostic(std::move(diagnostic));
    }

    std::vector<std::string> methodCandidates(Type receiverType) const {
        if (isStructType(receiverType)) {
            return declaredStructMethodNamesForType(receiverType);
        }
        if (isStackType(receiverType) || isQueueType(receiverType)) {
            return {"add", "top", "pop", "clear"};
        }
        if (isDequeType(receiverType)) {
            return {"addFront", "addBack", "front", "back", "popFront", "popBack", "clear"};
        }
        if (isMapType(receiverType)) {
            return {"add", "remove", "clear", "at", "prev", "next", "hasPrev", "hasNext"};
        }
        if (isSetType(receiverType)) {
            return {"add", "remove", "clear", "prev", "next", "hasPrev", "hasNext"};
        }
        if (isListType(receiverType)) {
            return {"add", "remove", "clear", "sort", "reverse", "find", "split"};
        }
        return {};
    }

    std::vector<std::string> callableCandidates() const {
        std::vector<std::string> candidates = {
            "len", "copy", "range", "min", "max", "sum", "abs", "input"
        };
        for (const auto& function : declaredFunctions) {
            candidates.push_back(function.first);
        }
        const std::vector<std::string> typeNames = declaredCustomTypeNames();
        candidates.insert(candidates.end(), typeNames.begin(), typeNames.end());
        return candidates;
    }

// isValueType returns whether the supplied input satisfies the relevant condition.
    bool isValueType(Type type) const {
        return type != PrimitiveType::Unknown;
    }

// isNumericType returns whether the supplied input satisfies the relevant condition.
    bool isNumericType(Type type) const {
        return type == PrimitiveType::Bool ||
            type == PrimitiveType::Char ||
            type == PrimitiveType::Int ||
            type == PrimitiveType::Float;
    }

// isBitwiseType returns whether the supplied input satisfies the relevant condition.
    bool isBitwiseType(Type type) const {
        return type == PrimitiveType::Bool || type == PrimitiveType::Char || type == PrimitiveType::Int;
    }

// isIncrementableType returns whether the supplied input satisfies the relevant condition.
    bool isIncrementableType(Type type) const {
        return type == PrimitiveType::Char ||
            type == PrimitiveType::Int ||
            type == PrimitiveType::Float;
    }

// isLexicographicallyComparable returns whether the supplied input satisfies the relevant condition.
    bool isLexicographicallyComparable(Type type) const {
        if (isNumericType(type)) {
            return true;
        }

        if (isListType(type)) {
            return isLexicographicallyComparable(type.subtypes[0]);
        }

        if (isPairType(type)) {
            return isLexicographicallyComparable(type.subtypes[0]) &&
                isLexicographicallyComparable(type.subtypes[1]);
        }

        if (isSetType(type)) {
            return isLexicographicallyComparable(type.subtypes[0]);
        }

        if (isMapType(type)) {
            return isLexicographicallyComparable(type.subtypes[0]) &&
                isLexicographicallyComparable(type.subtypes[1]);
        }

        return false;
    }

// isComparable returns whether the supplied input satisfies the relevant condition.
    bool isComparable(Type left, Type right) const {
        if (!isValueType(left) || !isValueType(right)) {
            return false;
        }

        if (isCollectionType(left) || isCollectionType(right) || isPairType(left) || isPairType(right)) {
            return left == right && isLexicographicallyComparable(left);
        }

        if (isStructType(left) || isStructType(right)) {
            return left == right;
        }

        return isNumericType(left) && isNumericType(right);
    }

// isFloatType returns whether the supplied input satisfies the relevant condition.
    bool isFloatType(Type type) const {
        return type == PrimitiveType::Float;
    }

// isSummableType returns whether the supplied input satisfies the relevant condition.
    bool isSummableType(Type type) const {
        return type == PrimitiveType::Bool ||
            type == PrimitiveType::Char ||
            type == PrimitiveType::Int ||
            type == PrimitiveType::Float;
    }

    Type resolvedCastTarget(Type from, Type requested) const {
        if (requested.primitive == PrimitiveType::Set && requested.subtypes.empty()) {
            if (isListType(from)) {
                return Type(PrimitiveType::Set, {from.subtypes[0]});
            }
            if (isRangeType(from)) {
                return Type(PrimitiveType::Set, {PrimitiveType::Int});
            }
            return PrimitiveType::Unknown;
        }

        if (requested.primitive == PrimitiveType::Map && requested.subtypes.empty()) {
            if (isListType(from) && isPairType(from.subtypes[0])) {
                return Type(PrimitiveType::Map, {from.subtypes[0].subtypes[0], from.subtypes[0].subtypes[1]});
            }
            return PrimitiveType::Unknown;
        }

        if (requested.primitive == PrimitiveType::List && requested.subtypes.empty()) {
            if (isSetType(from)) {
                return Type(PrimitiveType::List, {from.subtypes[0]});
            }
            if (isMapType(from)) {
                return Type(PrimitiveType::List, {Type(PrimitiveType::Pair, {from.subtypes[0], from.subtypes[1]})});
            }
            if (isRangeType(from)) {
                return Type(PrimitiveType::List, {PrimitiveType::Int});
            }
            if (isLinearDataStructureType(from)) {
                return Type(PrimitiveType::List, {from.subtypes[0]});
            }
            if (isHeapType(from)) {
                return Type(PrimitiveType::List, {from.subtypes[0]});
            }
            return PrimitiveType::Unknown;
        }

        if ((requested.primitive == PrimitiveType::Stack ||
             requested.primitive == PrimitiveType::Queue ||
             requested.primitive == PrimitiveType::Deque ||
             requested.primitive == PrimitiveType::Heap) &&
            requested.subtypes.empty()) {
            if (isListType(from)) {
                return Type(requested.primitive, {from.subtypes[0]});
            }
            return PrimitiveType::Unknown;
        }

        return requested;
    }

// sumResultType implements the sumResultType behavior for the expressionAnalyzer.cpp module.
    Type sumResultType(Type elementType) const {
        if (elementType == PrimitiveType::Float) {
            return PrimitiveType::Float;
        }
        return PrimitiveType::Int;
    }

// binaryResultType implements the binaryResultType behavior for the expressionAnalyzer.cpp module.
    Type binaryResultType(Type left, Type right, const std::string& op) const {
        if (op == "in") {
            return PrimitiveType::Bool;
        }

        if (op == "+" && isListType(left) && left == right) {
            return left;
        }

        if (!isNumericType(left) || !isNumericType(right)) {
            return PrimitiveType::Unknown;
        }

        if (op == "%" && (isFloatType(left) || isFloatType(right))) {
            return PrimitiveType::Unknown;
        }

        if (left == PrimitiveType::Float || right == PrimitiveType::Float) {
            return PrimitiveType::Float;
        }

        return PrimitiveType::Int;
    }

// analyzeLiteral analyzes the construct and validates its semantics.
    bool analyzeLiteral(LiteralExpr& expr) {
        switch (expr.kind) {
            case LiteralExpr::Kind::Bool:
                expr.inferredType = PrimitiveType::Bool;
                break;
            case LiteralExpr::Kind::Null:
                expr.inferredType = PrimitiveType::Unknown;
                break;
            case LiteralExpr::Kind::Int:
                expr.inferredType = PrimitiveType::Int;
                break;
            case LiteralExpr::Kind::Float:
                expr.inferredType = PrimitiveType::Float;
                break;
            case LiteralExpr::Kind::String:
                expr.inferredType = declaredTypeForName("string");
                break;
            case LiteralExpr::Kind::Char:
                expr.inferredType = PrimitiveType::Char;
                break;
        }
        expr.mutableValue = false;
        return true;
    }

// analyzeVariable analyzes the construct and validates its semantics.
    bool analyzeVariable(VariableExpr& expr) {
        const auto variable = declaredVariables.find(expr.name);
        if (variable == declaredVariables.end()) {
            const auto function = declaredFunctions.find(expr.name);
            if (function != declaredFunctions.end()) {
                expr.inferredType = functionTypeForSignature(function->second);
                expr.mutableValue = false;
                expr.resolvedSymbol = "function:" + expr.name;
                return true;
            }
            const Type builtinType = builtinFunctionType(expr.name);
            if (isFunctionType(builtinType)) {
                expr.inferredType = builtinType;
                expr.mutableValue = false;
                expr.resolvedSymbol = "builtin:" + expr.name;
                return true;
            }
            if (futureVariableLines != nullptr) {
                const auto future = futureVariableLines->find(expr.name);
                if (future != futureVariableLines->end() && future->second > lineNumber) {
                    reportNameSuggestion(
                        expr.sourceColumn,
                        expr.sourceSpan,
                        "variable '" + expr.name + "' is used before its declaration",
                        expr.name,
                        {},
                        "move the declaration of '" + expr.name + "' before this use"
                    );
                    return false;
                }
            }
            std::vector<std::string> candidates;
            candidates.reserve(declaredVariables.size() + declaredFunctions.size() + 8);
            for (const auto& declared : declaredVariables) {
                candidates.push_back(declared.first);
            }
            for (const auto& declared : declaredFunctions) {
                candidates.push_back(declared.first);
            }
            const std::vector<std::string> builtins = {
                "len", "copy", "range", "min", "max", "sum", "abs", "input"
            };
            candidates.insert(candidates.end(), builtins.begin(), builtins.end());
            reportNameSuggestion(
                expr.sourceColumn,
                expr.sourceSpan,
                "use of undeclared variable '" + expr.name + "'",
                expr.name,
                candidates,
                "declare '" + expr.name + "' in this scope before using it"
            );
            return false;
        }

        if (variable->second == PrimitiveType::Unknown) {
            expr.inferredType = PrimitiveType::Unknown;
            expr.mutableValue = true;
            return false;
        }

        expr.inferredType = variable->second;
        expr.mutableValue = true;
        expr.resolvedSymbol = "variable:" + expr.name;
        return true;
    }

    bool analyzeField(FieldExpr& expr) {
        if (!analyze(*expr.base)) {
            return false;
        }
        if (!isStructType(expr.base->inferredType)) {
            report(expr.sourceColumn, "field access requires a class or struct value");
            return false;
        }
        const std::map<std::string, Type>* fields = declaredStructFieldsForName(expr.base->inferredType.name);
        const auto field = fields == nullptr ? std::map<std::string, Type>::const_iterator{} : fields->find(expr.field);
        if (fields == nullptr || field == fields->end()) {
            Diagnostic diagnostic;
            diagnostic.message =
                std::string(isClassType(expr.base->inferredType) ? "class " : "struct ") +
                expr.base->inferredType.name +
                " has no field '" +
                expr.field +
                "'";
            diagnostic.labels.push_back({
                expr.sourceSpan.valid()
                    ? expr.sourceSpan
                    : sourceTokenSpan(inputFile, sourceLines, lineNumber, expr.sourceColumn),
                "unknown field '" + expr.field + "'",
                true
            });
            if (fields != nullptr && !fields->empty()) {
                std::vector<std::string> fieldNames;
                fieldNames.reserve(fields->size());
                std::string available = fields->size() == 1
                    ? "available field: "
                    : "available fields: ";
                size_t fieldIndex = 0;
                for (const auto& declaredField : *fields) {
                    fieldNames.push_back(declaredField.first);
                    if (fieldIndex++ > 0) {
                        available += ", ";
                    }
                    available += "'" + declaredField.first + "'";
                }
                diagnostic.helps.push_back(std::move(available));
                const std::string closest = closestDiagnosticCandidate(
                    expr.field,
                    fieldNames
                );
                if (!closest.empty()) {
                    diagnostic.suggestions.push_back({
                        diagnostic.labels.front().span,
                        closest,
                        "did you mean '" + closest + "'?",
                        SuggestionApplicability::MaybeIncorrect
                    });
                }
            }
            recordDiagnostic(std::move(diagnostic));
            return false;
        }
        expr.inferredType = field->second;
        expr.mutableValue = expr.base->mutableValue;
        expr.resolvedOwnerType = expr.base->inferredType.name;
        expr.resolvedSymbol = "field:" + expr.base->inferredType.name + "." + expr.field;
        return true;
    }

// analyzeUnary analyzes the construct and validates its semantics.
    bool analyzeUnary(UnaryExpr& expr) {
        if (!analyze(*expr.operand)) {
            return false;
        }

        if (isFunctionType(expr.operand->inferredType)) {
            report(expr.sourceColumn, "function values only support calls, assignment, ==, and !=");
            return false;
        }

        if (expr.op == "++" || expr.op == "--") {
            if (!expr.operand->mutableValue) {
                report(expr.sourceColumn, expr.postfix ? "expected variable before '" + expr.op + "'" : "expected variable after '" + expr.op + "'");
                return false;
            }
            if (!isIncrementableType(expr.operand->inferredType)) {
                report(expr.sourceColumn, "cannot use '" + expr.op + "' with " + cpppTypeName(expr.operand->inferredType));
                return false;
            }

            expr.inferredType = expr.operand->inferredType;
            return true;
        }

        if (expr.op == "!") {
            if (!isValueType(expr.operand->inferredType)) {
                report(expr.sourceColumn, "cannot use '!' with " + cpppTypeName(expr.operand->inferredType));
                return false;
            }
            expr.inferredType = PrimitiveType::Bool;
            return true;
        }

        if (!isNumericType(expr.operand->inferredType)) {
            report(expr.sourceColumn, "cannot use unary '" + expr.op + "' with " + cpppTypeName(expr.operand->inferredType));
            return false;
        }

        expr.inferredType = expr.operand->inferredType;
        return true;
    }

// analyzeBinary analyzes the construct and validates its semantics.
    bool analyzeBinary(BinaryExpr& expr) {
        if (!analyze(*expr.left) || !analyze(*expr.right)) {
            return false;
        }

        if ((isFunctionType(expr.left->inferredType) || isFunctionType(expr.right->inferredType)) &&
            expr.op != "==" && expr.op != "!=") {
            report(expr.sourceColumn, "function values only support calls, assignment, ==, and !=");
            return false;
        }

        if (expr.op == "in") {
            if (!isCollectionType(expr.right->inferredType) && !isRangeType(expr.right->inferredType)) {
                report(expr.sourceColumn, "right side of 'in' must be a List");
                return false;
            }

            if (isRangeType(expr.right->inferredType)) {
                if (!expr.left->explicitCast && expr.left->inferredType != PrimitiveType::Int) {
                    report(expr.sourceColumn, "cannot check membership of " + cpppTypeName(expr.left->inferredType) + " in range");
                    return false;
                }
                expr.inferredType = PrimitiveType::Bool;
                return true;
            }

            if (isListType(expr.right->inferredType)) {
                if (isListType(expr.left->inferredType)) {
                    expr.inferredType = PrimitiveType::Bool;
                    return true;
                }

                const Type elementType = expr.right->inferredType.subtypes[0];
                if (!expr.left->explicitCast && !isImplicitlyConvertible(expr.left->inferredType, elementType)) {
                    report(expr.sourceColumn, "cannot check membership of " + cpppTypeName(expr.left->inferredType) + " in " + cpppTypeName(expr.right->inferredType));
                    return false;
                }

                expr.inferredType = PrimitiveType::Bool;
                return true;
            }

            const Type memberType = expr.right->inferredType.subtypes[0];
            if (!expr.left->explicitCast && !isImplicitlyConvertible(expr.left->inferredType, memberType)) {
                report(expr.sourceColumn, "cannot check membership of " + cpppTypeName(expr.left->inferredType) + " in " + cpppTypeName(expr.right->inferredType));
                return false;
            }

            expr.inferredType = PrimitiveType::Bool;
            return true;
        }

        if (expr.op == "||" || expr.op == "&&") {
            if (!isValueType(expr.left->inferredType) || !isValueType(expr.right->inferredType)) {
                report(expr.sourceColumn, "cannot use '" + expr.op + "' with " + cpppTypeName(expr.left->inferredType) + " and " + cpppTypeName(expr.right->inferredType));
                return false;
            }
            expr.inferredType = PrimitiveType::Bool;
            return true;
        }

        if (expr.op == "|" || expr.op == "^" || expr.op == "&" || expr.op == "<<" || expr.op == ">>") {
            if (!isBitwiseType(expr.left->inferredType) || !isBitwiseType(expr.right->inferredType)) {
                report(expr.sourceColumn, "cannot use '" + expr.op + "' with " + cpppTypeName(expr.left->inferredType) + " and " + cpppTypeName(expr.right->inferredType));
                return false;
            }
            expr.inferredType = expr.left->inferredType;
            return true;
        }

        if (expr.op == "==" || expr.op == "!=" || expr.op == "<" || expr.op == "<=" || expr.op == ">" || expr.op == ">=") {
            const Type& leftType = expr.left->inferredType;
            const Type& rightType = expr.right->inferredType;
            const auto* leftLiteral = dynamic_cast<LiteralExpr*>(expr.left.get());
            const auto* rightLiteral = dynamic_cast<LiteralExpr*>(expr.right.get());
            const bool leftNull = leftLiteral != nullptr && leftLiteral->kind == LiteralExpr::Kind::Null;
            const bool rightNull = rightLiteral != nullptr && rightLiteral->kind == LiteralExpr::Kind::Null;
            if (leftNull || rightNull) {
                if (expr.op != "==" && expr.op != "!=") {
                    report(expr.sourceColumn, "NULL can only be compared using == or !=");
                    return false;
                }
                expr.inferredType = PrimitiveType::Bool;
                return true;
            }
            if (isFunctionType(leftType) || isFunctionType(rightType)) {
                if ((expr.op != "==" && expr.op != "!=") || leftType != rightType) {
                    report(expr.sourceColumn, "function values only support == and != with the same function type");
                    return false;
                }
                expr.inferredType = PrimitiveType::Bool;
                return true;
            }
            if (leftType.primitive == PrimitiveType::List || rightType.primitive == PrimitiveType::List) {
                if (leftType != rightType || !isLexicographicallyComparable(leftType)) {
                    report(expr.sourceColumn, "cannot compare " + cpppTypeName(leftType) + " and " + cpppTypeName(rightType));
                    return false;
                }
            } else if ((isStructType(leftType) || isStructType(rightType)) && expr.op != "==" && expr.op != "!=") {
                report(expr.sourceColumn, "struct values only support == and != comparisons");
                return false;
            } else if (!isComparable(leftType, rightType)) {
                report(expr.sourceColumn, "cannot compare " + cpppTypeName(leftType) + " and " + cpppTypeName(rightType));
                return false;
            }
            expr.inferredType = PrimitiveType::Bool;
            return true;
        }

        expr.inferredType = binaryResultType(expr.left->inferredType, expr.right->inferredType, expr.op);
        if (expr.inferredType == PrimitiveType::Unknown) {
            report(expr.sourceColumn, "cannot mix " + cpppTypeName(expr.left->inferredType) + " and " + cpppTypeName(expr.right->inferredType) + " with '" + expr.op + "'");
            return false;
        }

        return true;
    }

// analyzeCast analyzes the construct and validates its semantics.
    bool analyzeCast(CastExpr& expr) {
        if (!analyze(*expr.operand)) {
            return false;
        }

        if (isHeapType(expr.targetType) && isFunctionType(expr.operand->inferredType)) {
            const Type& comparator = expr.operand->inferredType;
            const Type& elementType = expr.targetType.subtypes[0];
            if (comparator.subtypes.size() != 3 || comparator.subtypes[0] != PrimitiveType::Bool ||
                comparator.subtypes[1] != elementType || comparator.subtypes[2] != elementType) {
                report(expr.sourceColumn, "Heap comparator must have type bool(" + cpppTypeName(elementType) + ", " + cpppTypeName(elementType) + ")");
                return false;
            }
            expr.inferredType = expr.targetType;
            expr.explicitCast = true;
            return true;
        }

        const Type targetType = resolvedCastTarget(expr.operand->inferredType, expr.targetType);
        if (targetType == PrimitiveType::Unknown ||
            !canExplicitlyCastType(expr.operand->inferredType, targetType)) {
            report(expr.sourceColumn, "cannot cast " + cpppTypeName(expr.operand->inferredType) + " to " + cpppTypeName(expr.targetType));
            return false;
        }

        expr.targetType = targetType;
        expr.inferredType = targetType;
        expr.explicitCast = true;
        return true;
    }

// analyzeCall analyzes the construct and validates its semantics.
    bool analyzeCall(CallExpr& expr) {
        if (expr.receiver && !analyze(*expr.receiver)) {
            return false;
        }
        for (const std::unique_ptr<Expr>& argument : expr.arguments) {
            if (!analyze(*argument)) {
                return false;
            }
        }

        if (expr.explicitConstructedType != PrimitiveType::Unknown) {
            expr.resolvedCallable = "constructor:" + cpppTypeName(expr.explicitConstructedType);
            if (!isListType(expr.explicitConstructedType) || expr.explicitConstructedType.subtypes.size() != 1 ||
                expr.arguments.empty() || expr.arguments.size() > 2) {
                report(expr.sourceColumn, "List<T> construction expects size and an optional fill value");
                return false;
            }
            if (expr.arguments[0]->inferredType != PrimitiveType::Int) {
                report(expr.arguments[0]->sourceColumn, "List<T> size must be int");
                return false;
            }
            if (expr.arguments.size() == 2 && !expr.arguments[1]->explicitCast &&
                !isImplicitlyConvertible(expr.arguments[1]->inferredType, expr.explicitConstructedType.subtypes[0])) {
                report(expr.arguments[1]->sourceColumn, "cannot use " +
                    cpppTypeName(expr.arguments[1]->inferredType) + " as List element type " +
                    cpppTypeName(expr.explicitConstructedType.subtypes[0]));
                return false;
            }
            expr.inferredType = expr.explicitConstructedType;
            return true;
        }

        if (expr.receiver) {
            expr.resolvedCallable = "method:" + cpppTypeName(expr.receiver->inferredType) + "." + expr.callee;
        } else {
            const auto variable = declaredVariables.find(expr.callee);
            if (variable != declaredVariables.end() && isFunctionType(variable->second)) {
                expr.resolvedCallable = "function-variable:" + expr.callee;
            } else if (declaredFunctions.count(expr.callee) != 0) {
                expr.resolvedCallable = "function:" + expr.callee;
            } else if (isStructType(declaredTypeForName(expr.callee))) {
                expr.resolvedCallable = "constructor:" + expr.callee;
            } else {
                expr.resolvedCallable = "builtin:" + expr.callee;
            }
        }

        if (!expr.receiver) {
            const auto variable = declaredVariables.find(expr.callee);
            if (variable != declaredVariables.end() && isFunctionType(variable->second)) {
                const Type& functionType = variable->second;
                expr.functionType = functionType;
                const size_t parameterCount = functionType.subtypes.size() - 1;
                if (expr.arguments.size() > parameterCount) {
                    report(expr.sourceColumn, expr.callee + " expects " + std::to_string(parameterCount) + " arguments, got " + std::to_string(expr.arguments.size()));
                    return false;
                }
                for (size_t i = 0; i < expr.arguments.size(); ++i) {
                    if (!expr.arguments[i]->explicitCast && !isImplicitlyConvertible(expr.arguments[i]->inferredType, functionType.subtypes[i + 1])) {
                        report(expr.arguments[i]->sourceColumn, "cannot use " + cpppTypeName(expr.arguments[i]->inferredType) +
                            " as " + cpppTypeName(functionType.subtypes[i + 1]) + " for " + expr.callee + "()");
                        return false;
                    }
                }
                if (expr.arguments.size() < parameterCount) {
                    std::vector<Type> remaining = {functionType.subtypes[0]};
                    remaining.insert(remaining.end(), functionType.subtypes.begin() + 1 + expr.arguments.size(), functionType.subtypes.end());
                    expr.inferredType = Type(PrimitiveType::Function, std::move(remaining));
                    if (functionType.functionParameterCopy.size() > expr.arguments.size()) {
                        expr.inferredType.functionParameterCopy.assign(
                            functionType.functionParameterCopy.begin() + expr.arguments.size(),
                            functionType.functionParameterCopy.end()
                        );
                    }
                    expr.partialApplication = true;
                } else {
                    expr.inferredType = functionType.subtypes[0];
                }
                return true;
            }
        }

        if (expr.callee == "len") {
            if (expr.arguments.size() != 1) {
                report(expr.sourceColumn, "len must be called as len(collection)");
                return false;
            }
            if (!isCollectionType(expr.arguments[0]->inferredType)) {
                report(expr.sourceColumn, "len() expects a List, Heap, Set, or Map value");
                return false;
            }
            expr.inferredType = PrimitiveType::Int;
            return true;
        }

        if (expr.callee == "copy") {
            if (expr.receiver || expr.arguments.size() != 1) {
                report(expr.sourceColumn, "copy must be called as copy(value)");
                return false;
            }
            if (expr.arguments[0]->inferredType == PrimitiveType::Unknown ||
                expr.arguments[0]->inferredType == PrimitiveType::Void) {
                report(expr.arguments[0]->sourceColumn, "copy() needs a value with an unambiguous type");
                return false;
            }
            expr.inferredType = expr.arguments[0]->inferredType;
            return true;
        }

        if (expr.receiver && isHeapType(expr.receiver->inferredType)) {
            const Type elementType = expr.receiver->inferredType.subtypes[0];
            if (expr.callee == "clear") {
                if (!expr.arguments.empty()) {
                    report(expr.sourceColumn, "clear() does not take arguments");
                    return false;
                }
                if (!expr.receiver->mutableValue) {
                    report(expr.sourceColumn, "clear() requires a mutable collection variable");
                    return false;
                }
                expr.inferredType = PrimitiveType::Void;
                return true;
            }
            if (expr.callee == "top" || expr.callee == "pop") {
                if (!expr.arguments.empty()) {
                    report(expr.sourceColumn, expr.callee + "() does not take arguments");
                    return false;
                }
                expr.inferredType = elementType;
                return true;
            }
            if (expr.callee == "push") {
                if (expr.arguments.size() != 1) {
                    report(expr.sourceColumn, "push() expects exactly one value");
                    return false;
                }
                if (!expr.arguments[0]->explicitCast && !isImplicitlyConvertible(expr.arguments[0]->inferredType, elementType)) {
                    report(expr.arguments[0]->sourceColumn, "cannot push " + cpppTypeName(expr.arguments[0]->inferredType) + " to " + cpppTypeName(expr.receiver->inferredType));
                    return false;
                }
                expr.inferredType = PrimitiveType::Void;
                return true;
            }
            reportNameSuggestion(expr.sourceColumn, expr.sourceSpan, cpppTypeName(expr.receiver->inferredType) + " has no method '" + expr.callee + "'", expr.callee, {"push", "pop", "top", "clear"});
            return false;
        }

        if (expr.receiver && isLinearDataStructureType(expr.receiver->inferredType)) {
            const Type receiverType = expr.receiver->inferredType;
            const Type elementType = receiverType.subtypes[0];
            const bool stackOrQueue = isStackType(receiverType) || isQueueType(receiverType);
            const bool deque = isDequeType(receiverType);

            if (expr.callee == "clear") {
                if (!expr.arguments.empty()) {
                    report(expr.sourceColumn, "clear() does not take arguments");
                    return false;
                }
                if (!expr.receiver->mutableValue) {
                    report(expr.sourceColumn, "clear() requires a mutable collection variable");
                    return false;
                }
                expr.inferredType = PrimitiveType::Void;
                return true;
            }

            const bool accessor =
                (expr.callee == "top" && stackOrQueue) ||
                ((expr.callee == "front" || expr.callee == "back") && deque);
            const bool remover =
                (expr.callee == "pop" && stackOrQueue) ||
                ((expr.callee == "popFront" || expr.callee == "popBack") && deque);
            if (accessor || remover) {
                if (!expr.arguments.empty()) {
                    report(expr.sourceColumn, expr.callee + "() does not take arguments");
                    return false;
                }
                expr.inferredType = elementType;
                return true;
            }

            if ((expr.callee == "addFront" || expr.callee == "addBack") && deque) {
                if (expr.arguments.size() != 1) {
                    report(expr.sourceColumn, expr.callee + "() expects exactly one value");
                    return false;
                }
                if (!expr.arguments[0]->explicitCast &&
                    !isImplicitlyConvertible(expr.arguments[0]->inferredType, elementType)) {
                    report(expr.arguments[0]->sourceColumn, "cannot add " +
                        cpppTypeName(expr.arguments[0]->inferredType) + " to " +
                        cpppTypeName(receiverType));
                    return false;
                }
                expr.inferredType = PrimitiveType::Void;
                return true;
            }

            reportNameSuggestion(
                expr.sourceColumn,
                expr.sourceSpan,
                cpppTypeName(receiverType) + " has no method '" + expr.callee + "'",
                expr.callee,
                methodCandidates(receiverType)
            );
            return false;
        }

        if (expr.receiver && isStructType(expr.receiver->inferredType)) {
            const std::map<std::string, Type>* fields = declaredStructFieldsForName(expr.receiver->inferredType.name);
            const auto functionField = fields == nullptr ? std::map<std::string, Type>::const_iterator{} : fields->find(expr.callee);
            if (fields != nullptr && functionField != fields->end() && isFunctionType(functionField->second)) {
                const Type& functionType = functionField->second;
                expr.functionType = functionType;
                const size_t parameterCount = functionType.subtypes.size() - 1;
                if (expr.arguments.size() != parameterCount) {
                    report(expr.sourceColumn, expr.callee + " expects " + std::to_string(parameterCount) + " arguments");
                    return false;
                }
                for (size_t i = 0; i < expr.arguments.size(); ++i) {
                    if (!expr.arguments[i]->explicitCast && !isImplicitlyConvertible(expr.arguments[i]->inferredType, functionType.subtypes[i + 1])) {
                        report(expr.arguments[i]->sourceColumn, "cannot use " + cpppTypeName(expr.arguments[i]->inferredType) +
                            " as " + cpppTypeName(functionType.subtypes[i + 1]) + " for " + expr.callee + "()");
                        return false;
                    }
                }
                expr.inferredType = functionType.subtypes[0];
                return true;
            }
            const FunctionSignature* method = declaredStructMethodForType(expr.receiver->inferredType, expr.callee);
            if (method == nullptr) {
                reportNameSuggestion(
                    expr.sourceColumn,
                    expr.sourceSpan,
                    std::string(isClassType(expr.receiver->inferredType) ? "class " : "struct ") +
                        expr.receiver->inferredType.name +
                        " has no method '" +
                        expr.callee +
                        "'",
                    expr.callee,
                    methodCandidates(expr.receiver->inferredType)
                );
                return false;
            }
            if (expr.arguments.size() != method->parameters.size()) {
                report(expr.sourceColumn, expr.callee + " expects " + std::to_string(method->parameters.size()) + " argument" + (method->parameters.size() == 1 ? "" : "s"));
                return false;
            }
            for (size_t i = 0; i < expr.arguments.size(); ++i) {
                if (!expr.arguments[i]->explicitCast && !isImplicitlyConvertible(expr.arguments[i]->inferredType, method->parameters[i].type)) {
                    report(expr.arguments[i]->sourceColumn, "cannot use " + cpppTypeName(expr.arguments[i]->inferredType) + " as " + cpppTypeName(method->parameters[i].type) + " for " + expr.callee + "()");
                    return false;
                }
            }
            expr.inferredType = method->returnsVoid ? PrimitiveType::Void : method->returnType;
            return true;
        }

        if (expr.callee == "range") {
            if (expr.receiver) {
                report(expr.sourceColumn, "range() cannot be called as a method");
                return false;
            }
            if (expr.arguments.empty() || expr.arguments.size() > 3) {
                report(expr.sourceColumn, "range must be called as range(stop), range(start, stop), or range(start, stop, step)");
                return false;
            }
            for (const std::unique_ptr<Expr>& argument : expr.arguments) {
                if (!argument->explicitCast && argument->inferredType != PrimitiveType::Int) {
                    report(argument->sourceColumn, "range() arguments must be int");
                    return false;
                }
            }
            expr.inferredType = PrimitiveType::Range;
            return true;
        }

        if (expr.callee == "clear") {
            if (!expr.receiver || !isCollectionType(expr.receiver->inferredType)) {
                report(expr.sourceColumn, "clear() can only be used on List, Set, or Map values");
                return false;
            }
            if (!expr.receiver->mutableValue) {
                report(expr.sourceColumn, "clear() requires a mutable collection variable");
                return false;
            }
            if (!expr.arguments.empty()) {
                report(expr.sourceColumn, "clear() does not take arguments");
                return false;
            }
            expr.inferredType = PrimitiveType::Void;
            return true;
        }

        if (expr.callee == "remove") {
            if (!expr.receiver || !isCollectionType(expr.receiver->inferredType)) {
                report(expr.sourceColumn, "remove() can only be used on List, Set, or Map values");
                return false;
            }
            if (!expr.receiver->mutableValue) {
                report(expr.sourceColumn, "remove() requires a mutable collection variable");
                return false;
            }

            if (isListType(expr.receiver->inferredType)) {
                if (expr.arguments.size() > 1) {
                    report(expr.sourceColumn, "remove() expects no arguments or index");
                    return false;
                }
                if (!expr.arguments.empty()) {
                    const Expr& index = *expr.arguments[0];
                    if (!index.explicitCast && !isImplicitlyConvertible(index.inferredType, PrimitiveType::Int)) {
                        report(expr.sourceColumn, "list index must be int");
                        return false;
                    }
                }
                expr.inferredType = expr.receiver->inferredType.subtypes[0];
                return true;
            }

            if (expr.arguments.size() != 1) {
                report(expr.sourceColumn, "remove() expects exactly one key or value");
                return false;
            }

            const Type expectedType = expr.receiver->inferredType.subtypes[0];
            if (!expr.arguments[0]->explicitCast &&
                !isImplicitlyConvertible(expr.arguments[0]->inferredType, expectedType)) {
                report(expr.sourceColumn, "cannot remove " + cpppTypeName(expr.arguments[0]->inferredType) + " from " + cpppTypeName(expr.receiver->inferredType));
                return false;
            }

            expr.inferredType = isSetType(expr.receiver->inferredType)
                ? expr.receiver->inferredType.subtypes[0]
                : expr.receiver->inferredType.subtypes[1];
            return true;
        }

        if (expr.callee == "at") {
            if (!expr.receiver || !isMapType(expr.receiver->inferredType)) {
                report(expr.sourceColumn, "at() can only be used on Map values");
                return false;
            }
            if (expr.arguments.size() != 1) {
                report(expr.sourceColumn, "at() expects exactly one key");
                return false;
            }
            if (!expr.arguments[0]->explicitCast &&
                !isImplicitlyConvertible(expr.arguments[0]->inferredType, expr.receiver->inferredType.subtypes[0])) {
                report(expr.sourceColumn, "cannot use key of type " + cpppTypeName(expr.arguments[0]->inferredType) + " with " + cpppTypeName(expr.receiver->inferredType));
                return false;
            }

            expr.inferredType = expr.receiver->inferredType.subtypes[1];
            return true;
        }

        if (expr.callee == "prev" || expr.callee == "next" || expr.callee == "hasPrev" || expr.callee == "hasNext") {
            if (!expr.receiver || (!isSetType(expr.receiver->inferredType) && !isMapType(expr.receiver->inferredType))) {
                report(expr.sourceColumn, expr.callee + "() can only be used on Set or Map values");
                return false;
            }
            if (expr.arguments.size() != 1) {
                report(expr.sourceColumn, expr.callee + "() expects exactly one key");
                return false;
            }

            const Type keyType = expr.receiver->inferredType.subtypes[0];
            if (!expr.arguments[0]->explicitCast &&
                !isImplicitlyConvertible(expr.arguments[0]->inferredType, keyType)) {
                report(expr.sourceColumn, "cannot use key of type " + cpppTypeName(expr.arguments[0]->inferredType) + " with " + cpppTypeName(expr.receiver->inferredType));
                return false;
            }

            expr.inferredType = (expr.callee == "hasPrev" || expr.callee == "hasNext")
                ? Type(PrimitiveType::Bool)
                : keyType;
            return true;
        }

        if (expr.callee == "find") {
            if (!expr.receiver ||
                expr.receiver->inferredType.primitive != PrimitiveType::List ||
                expr.receiver->inferredType.subtypes.size() != 1) {
                report(expr.sourceColumn, "find() can only be used on List values");
                return false;
            }
            if (expr.arguments.size() != 1) {
                report(expr.sourceColumn, "find() expects exactly one value or sublist");
                return false;
            }

            const Type haystackType = expr.receiver->inferredType;
            const Type elementType = haystackType.subtypes[0];
            const Type needleType = expr.arguments[0]->inferredType;
            if (isListType(needleType)) {
                if (needleType == elementType) {
                    expr.inferredType = Type(PrimitiveType::List, {Type(PrimitiveType::Int)});
                    return true;
                }
                if (needleType != haystackType) {
                    report(expr.sourceColumn, "cannot find " + cpppTypeName(needleType) + " in " + cpppTypeName(haystackType));
                    return false;
                }
            } else if (!expr.arguments[0]->explicitCast && !isImplicitlyConvertible(needleType, elementType)) {
                report(expr.sourceColumn, "cannot find " + cpppTypeName(needleType) + " in " + cpppTypeName(haystackType));
                return false;
            }

            expr.inferredType = Type(PrimitiveType::List, {Type(PrimitiveType::Int)});
            return true;
        }

        if (expr.callee == "split") {
            if (!expr.receiver) {
                report(expr.sourceColumn, "split must be called as list.split(delimiter)");
                return false;
            }
            if (expr.arguments.size() != 1) {
                report(expr.sourceColumn, "split() expects exactly one delimiter");
                return false;
            }

            const Type haystackType = expr.receiver->inferredType;
            if (haystackType.primitive != PrimitiveType::List || haystackType.subtypes.size() != 1) {
                report(expr.sourceColumn, "split() can only be used on List values");
                return false;
            }

            const Type elementType = haystackType.subtypes[0];
            const Type delimiterType = expr.arguments[0]->inferredType;
            if (isListType(delimiterType)) {
                if (delimiterType != haystackType) {
                    report(expr.sourceColumn, "split() delimiter must be " + cpppTypeName(elementType) + " or " + cpppTypeName(haystackType));
                    return false;
                }
            } else if (!expr.arguments[0]->explicitCast && !isImplicitlyConvertible(delimiterType, elementType)) {
                report(expr.sourceColumn, "split() delimiter must be " + cpppTypeName(elementType) + " or " + cpppTypeName(haystackType));
                return false;
            }

            expr.inferredType = Type(PrimitiveType::List, {haystackType});
            return true;
        }

        if (expr.callee == "min" || expr.callee == "max") {
            if(expr.arguments.size() == 0) {
                report(expr.sourceColumn, expr.callee + "() must take in some values");
                    return false;
            }
            if (expr.arguments.size() == 1) {
                if (!isListType(expr.arguments[0]->inferredType) &&
                    !isSetType(expr.arguments[0]->inferredType) &&
                    !isMapType(expr.arguments[0]->inferredType)) {
                    report(expr.sourceColumn, expr.callee + "() expects a List, Set, or Map value, or a list of multiple values of the same type");
                    return false;
                }
                expr.inferredType = isMapType(expr.arguments[0]->inferredType)
                    ? expr.arguments[0]->inferredType.subtypes[0]
                    : expr.arguments[0]->inferredType.subtypes[0];
                return true;
            } else {
                for(unsigned int i = 0;i<expr.arguments.size();i++){
                    if (expr.arguments[i]->inferredType != expr.arguments[0]->inferredType){
                        report(expr.sourceColumn, expr.callee + "() expects all items in sequence to be of same type");
                        return false;
                    }
                }
                expr.inferredType = expr.arguments[0]->inferredType;
                return true;
            }
            
        }
        if (expr.callee == "abs") {
            if (expr.arguments.size() != 1) {
                report(expr.sourceColumn, expr.callee + " must be called as " + expr.callee + "(num)");
                return false;
            }
            if (expr.arguments[0]->inferredType.primitive != PrimitiveType::Int &&
            expr.arguments[0]->inferredType.primitive != PrimitiveType::Float) {
                report(expr.sourceColumn, expr.callee + "() expects a Numeric value");
                return false;
            }
            expr.inferredType = expr.arguments[0]->inferredType;
            return true;
        }
        if (expr.callee == "sum") {
            if (expr.arguments.size() != 1) {
                report(expr.sourceColumn, "sum must be called as sum(list)");
                return false;
            }
            if (expr.arguments[0]->inferredType.primitive != PrimitiveType::List ||
                expr.arguments[0]->inferredType.subtypes.size() != 1) {
                report(expr.sourceColumn, "sum() expects a List value");
                return false;
            }
            const Type elementType = expr.arguments[0]->inferredType.subtypes[0];
            if (!isSummableType(elementType)) {
                report(expr.sourceColumn, "sum() expects a List of numeric values");
                return false;
            }
            expr.inferredType = sumResultType(elementType);
            return true;
        }

        if (expr.receiver) {
            const std::vector<std::string> candidates =
                methodCandidates(expr.receiver->inferredType);
            if (std::find(candidates.begin(), candidates.end(), expr.callee) == candidates.end()) {
                reportNameSuggestion(
                    expr.sourceColumn,
                    expr.sourceSpan,
                    cpppTypeName(expr.receiver->inferredType) +
                        " has no method '" +
                        expr.callee +
                        "'",
                    expr.callee,
                    candidates
                );
                return false;
            }
        }

        const Type constructedType = declaredTypeForName(expr.callee);
        if (!expr.receiver && isStructType(constructedType)) {
            const FunctionSignature* constructor = declaredStructConstructorForName(expr.callee);
            if (constructor != nullptr) {
                if (expr.arguments.size() != constructor->parameters.size()) {
                    report(expr.sourceColumn, expr.callee + " constructor expects " +
                        std::to_string(constructor->parameters.size()) + " arguments");
                    return false;
                }
                for (size_t index = 0; index < constructor->parameters.size(); ++index) {
                    const Type& parameterType = constructor->parameters[index].type;
                    const auto* literal = dynamic_cast<LiteralExpr*>(expr.arguments[index].get());
                    const bool nullForClass = literal != nullptr &&
                        literal->kind == LiteralExpr::Kind::Null && isClassType(parameterType);
                    if (!nullForClass && !expr.arguments[index]->explicitCast &&
                        !isImplicitlyConvertible(expr.arguments[index]->inferredType, parameterType)) {
                        report(expr.arguments[index]->sourceColumn, "cannot use " +
                            cpppTypeName(expr.arguments[index]->inferredType) + " as " +
                            cpppTypeName(parameterType) + " for " + expr.callee + "()");
                        return false;
                    }
                }
                expr.inferredType = constructedType;
                return true;
            }
            const std::map<std::string, Type>* definition = declaredStructFieldsForName(expr.callee);
            const std::vector<std::string>* fieldOrder = declaredStructFieldOrderForName(expr.callee);
            if (definition == nullptr || fieldOrder == nullptr || expr.arguments.size() != fieldOrder->size()) {
                const size_t fieldCount = fieldOrder == nullptr ? 0 : fieldOrder->size();
                report(expr.sourceColumn, expr.callee + " constructor expects " + std::to_string(fieldCount) + " field values");
                return false;
            }
            size_t index = 0;
            for (const std::string& fieldName : *fieldOrder) {
                const auto field = definition->find(fieldName);
                const auto* literal = dynamic_cast<LiteralExpr*>(expr.arguments[index].get());
                const bool nullForStruct = literal != nullptr && literal->kind == LiteralExpr::Kind::Null && isClassType(field->second);
                if (!nullForStruct && !expr.arguments[index]->explicitCast && !isImplicitlyConvertible(expr.arguments[index]->inferredType, field->second)) {
                    report(expr.arguments[index]->sourceColumn, "cannot use " + cpppTypeName(expr.arguments[index]->inferredType) + " for field '" + field->first + "' of " + expr.callee);
                    return false;
                }
                ++index;
            }
            expr.inferredType = constructedType;
            return true;
        }

        const auto function = declaredFunctions.find(expr.callee);
        if (function != declaredFunctions.end()) {
            const FunctionSignature& signature = function->second;
            expr.functionType = functionTypeForSignature(signature);
            if (expr.arguments.size() > signature.parameters.size()) {
                std::string expected;
                for (size_t i = 0; i < signature.parameters.size(); ++i) {
                    if (i > 0) {
                        expected += " and ";
                    }
                    expected += cpppTypeName(signature.parameters[i].type);
                }
                report(
                    expr.sourceColumn,
                    signature.name + " expected " + std::to_string(signature.parameters.size()) +
                        " parameters of " + expected + " type got " + std::to_string(expr.arguments.size())
                );
                return false;
            }

            std::vector<Type> argumentTypes;
            for (size_t i = 0; i < expr.arguments.size(); ++i) {
                const Type parameterType = signature.parameters[i].type;
                const Type argumentType = expr.arguments[i]->inferredType;
                argumentTypes.push_back(argumentType);
                if (!expr.arguments[i]->explicitCast && !isImplicitlyConvertible(argumentType, parameterType)) {
                    std::string expected;
                    std::string got;
                    for (size_t j = 0; j < signature.parameters.size(); ++j) {
                        if (j > 0) {
                            expected += " and ";
                            got += " and ";
                        }
                        expected += cpppTypeName(signature.parameters[j].type);
                        if (j < expr.arguments.size()) {
                            got += cpppTypeName(expr.arguments[j]->inferredType);
                        } else {
                            got += "missing";
                        }
                    }
                    report(expr.sourceColumn, signature.name + " expected parameters of " + expected + " type got " + got);
                    return false;
                }
                if (!signature.parameters[i].copyParameter &&
                    (isStringType(parameterType) || isCollectionType(parameterType))) {
                    if (!expr.arguments[i]->mutableValue) {
                        report(expr.sourceColumn, signature.name + " requires collection and string arguments to be mutable variables");
                        return false;
                    }
                }
            }

            if (expr.arguments.size() < signature.parameters.size()) {
                std::vector<Type> remaining = {signature.returnsVoid ? Type(PrimitiveType::Void) : signature.returnType};
                for (size_t i = expr.arguments.size(); i < signature.parameters.size(); ++i) remaining.push_back(signature.parameters[i].type);
                expr.inferredType = Type(PrimitiveType::Function, std::move(remaining));
                for (size_t i = expr.arguments.size(); i < signature.parameters.size(); ++i) {
                    expr.inferredType.functionParameterCopy.push_back(signature.parameters[i].copyParameter);
                }
                expr.partialApplication = true;
            } else {
                expr.inferredType = signature.returnsVoid ? PrimitiveType::Void : signature.returnType;
            }
            return true;
        }

        reportNameSuggestion(
            expr.sourceColumn,
            expr.sourceSpan,
            "unexpected token in expression",
            expr.callee,
            callableCandidates()
        );
        return false;
    }

// analyzeIndex analyzes the construct and validates its semantics.
    bool analyzeIndex(IndexExpr& expr) {
        if (!analyze(*expr.base) || !analyze(*expr.index)) {
            return false;
        }

        if (isListType(expr.base->inferredType)) {
            if (expr.index->inferredType != PrimitiveType::Int) {
                report(expr.sourceColumn, "list index must be int");
                return false;
            }

            expr.inferredType = expr.base->inferredType.subtypes[0];
            expr.mutableValue = expr.base->mutableValue;
            return true;
        }

        if (isPairType(expr.base->inferredType)) {
            if (auto* literal = dynamic_cast<LiteralExpr*>(expr.index.get())) {
                if (literal->kind == LiteralExpr::Kind::Int) {
                    if (literal->text == "0") {
                        expr.inferredType = expr.base->inferredType.subtypes[0];
                        expr.mutableValue = expr.base->mutableValue;
                        return true;
                    }
                    if (literal->text == "1") {
                        expr.inferredType = expr.base->inferredType.subtypes[1];
                        expr.mutableValue = expr.base->mutableValue;
                        return true;
                    }
                    report(expr.sourceColumn, "pair index must be 0 or 1");
                    return false;
                }
            }
            if (expr.index->inferredType != PrimitiveType::Int) {
                report(expr.sourceColumn, "pair index must be int");
                return false;
            }
            if (expr.base->inferredType.subtypes[0] != expr.base->inferredType.subtypes[1]) {
                report(expr.sourceColumn, "dynamic Pair indexing requires both elements to have the same type; use [0] or [1]");
                return false;
            }
            expr.inferredType = expr.base->inferredType.subtypes[0];
            expr.mutableValue = expr.base->mutableValue;
            expr.dynamicPairIndex = true;
            return true;
        }

        if (!isMapType(expr.base->inferredType)) {
            report(expr.sourceColumn, "indexing requires a List value");
            return false;
        }

        if (!expr.index->explicitCast &&
            !isImplicitlyConvertible(expr.index->inferredType, expr.base->inferredType.subtypes[0])) {
            report(expr.sourceColumn, "map key type must be " + cpppTypeName(expr.base->inferredType.subtypes[0]));
            return false;
        }

        expr.inferredType = expr.base->inferredType.subtypes[1];
        expr.mutableValue = expr.base->mutableValue;
        return true;
    }

// analyzeSlice analyzes the construct and validates its semantics.
    bool analyzeSlice(SliceExpr& expr) {
        if (!analyze(*expr.base) || (expr.start && !analyze(*expr.start)) ||
            (expr.end && !analyze(*expr.end))) {
            return false;
        }

        if (expr.base->inferredType.primitive != PrimitiveType::List || expr.base->inferredType.subtypes.size() != 1) {
            report(expr.sourceColumn, "slicing requires a List value");
            return false;
        }

        if (expr.start && expr.start->inferredType != PrimitiveType::Int) {
            report(expr.start->sourceColumn, "slice start must be int");
            return false;
        }

        if (expr.end && expr.end->inferredType != PrimitiveType::Int) {
            report(expr.end->sourceColumn, "slice end must be int");
            return false;
        }

        expr.inferredType = expr.base->inferredType;
        return true;
    }

// analyzeListLiteral analyzes the construct and validates its semantics.
    bool analyzeListLiteral(ListLiteralExpr& expr) {
        if (expr.elements.empty()) {
            report(expr.sourceColumn, "empty list literal needs a declared List type");
            return false;
        }

        for (const std::unique_ptr<Expr>& element : expr.elements) {
            if (!analyze(*element)) {
                return false;
            }
        }

        if (expr.elements[0]->inferredType == PrimitiveType::Unknown) {
            report(expr.sourceColumn, "list literal elements must have a known CP++ type");
            return false;
        }

        const Type elementType = expr.elements[0]->inferredType;
        for (size_t i = 1; i < expr.elements.size(); ++i) {
            if (!isImplicitlyConvertible(expr.elements[i]->inferredType, elementType)) {
                report(expr.sourceColumn, "cannot implicitly convert " + cpppTypeName(expr.elements[i]->inferredType) + " to " + cpppTypeName(elementType) + " in list literal");
                return false;
            }
        }

        expr.inferredType = Type(PrimitiveType::List, {elementType});
        return true;
    }

    bool analyzeSetLiteral(SetLiteralExpr& expr) {
        if (expr.elements.empty()) {
            report(expr.sourceColumn, "empty set literal needs a declared Set type");
            return false;
        }

        for (const std::unique_ptr<Expr>& element : expr.elements) {
            if (!analyze(*element)) {
                return false;
            }
        }

        if (expr.elements[0]->inferredType == PrimitiveType::Unknown) {
            report(expr.sourceColumn, "set literal elements must have a known CP++ type");
            return false;
        }

        const Type elementType = expr.elements[0]->inferredType;
        for (size_t i = 1; i < expr.elements.size(); ++i) {
            if (!isImplicitlyConvertible(expr.elements[i]->inferredType, elementType)) {
                report(expr.elements[i]->sourceColumn, "cannot implicitly convert " + cpppTypeName(expr.elements[i]->inferredType) + " to " + cpppTypeName(elementType) + " in set literal");
                return false;
            }
        }

        expr.inferredType = Type(PrimitiveType::Set, {elementType});
        return true;
    }

    bool analyzeMapLiteral(MapLiteralExpr& expr) {
        if (expr.entries.empty()) {
            report(expr.sourceColumn, "empty map literal needs a declared Map type");
            return false;
        }

        for (MapLiteralEntry& entry : expr.entries) {
            if (!analyze(*entry.key) || !analyze(*entry.value)) {
                return false;
            }
        }

        if (expr.entries[0].key->inferredType == PrimitiveType::Unknown ||
            expr.entries[0].value->inferredType == PrimitiveType::Unknown) {
            report(expr.sourceColumn, "map literal keys and values must have a known CP++ type");
            return false;
        }

        const Type keyType = expr.entries[0].key->inferredType;
        const Type valueType = expr.entries[0].value->inferredType;
        for (size_t i = 1; i < expr.entries.size(); ++i) {
            if (!isImplicitlyConvertible(expr.entries[i].key->inferredType, keyType)) {
                report(expr.entries[i].key->sourceColumn, "cannot implicitly convert " + cpppTypeName(expr.entries[i].key->inferredType) + " to " + cpppTypeName(keyType) + " in map literal key");
                return false;
            }
            if (!isImplicitlyConvertible(expr.entries[i].value->inferredType, valueType)) {
                report(expr.entries[i].value->sourceColumn, "cannot implicitly convert " + cpppTypeName(expr.entries[i].value->inferredType) + " to " + cpppTypeName(valueType) + " in map literal value");
                return false;
            }
        }

        expr.inferredType = Type(PrimitiveType::Map, {keyType, valueType});
        return true;
    }

    bool analyzePairLiteral(PairLiteralExpr& expr) {
        if (!analyze(*expr.first) || !analyze(*expr.second)) {
            return false;
        }

        expr.inferredType = Type(PrimitiveType::Pair, {expr.first->inferredType, expr.second->inferredType});
        return true;
    }
};

}

bool analyzeExpressionAst(
    Expr& expression,
    const std::string& inputFile,
    int lineNumber,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    const std::map<std::string, int>* futureVariableLines
) {
    ExpressionAnalyzer analyzer(
        inputFile,
        lineNumber,
        sourceLines,
        declaredVariables,
        declaredFunctions,
        futureVariableLines
    );
    return analyzer.analyze(expression);
}
