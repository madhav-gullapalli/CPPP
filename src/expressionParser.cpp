/*
 * expressionParser.cpp
 *
 * Analyzes and lowers expressions, including type checks, coercions, and runtime overflow guards.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "expressionParser.h"

#include "typesCppp.h"

#include <climits>
#include <memory>

namespace {
// cppTypeForExpressionType implements the cppTypeForExpressionType behavior for the expressionParser.cpp module.
std::string cppTypeForExpressionType(const Type& type) {
    switch (type.primitive) {
        case PrimitiveType::Void:
            return "void";
        case PrimitiveType::Bool:
            return "bool";
        case PrimitiveType::Char:
            return "CPPPChar";
        case PrimitiveType::Int:
            return "long long";
        case PrimitiveType::Float:
            return "long double";
        case PrimitiveType::Range:
            return "CPPPRange";
        case PrimitiveType::List:
            if (type.subtypes.size() == 1) {
                return "CPPPList<" + cppTypeForExpressionType(type.subtypes[0]) + ">";
            }
            return "";
        case PrimitiveType::Stack:
            return type.subtypes.size() == 1
                ? "CPPPStack<" + cppTypeForExpressionType(type.subtypes[0]) + ">"
                : "";
        case PrimitiveType::Queue:
            return type.subtypes.size() == 1
                ? "CPPPQueue<" + cppTypeForExpressionType(type.subtypes[0]) + ">"
                : "";
        case PrimitiveType::Deque:
            return type.subtypes.size() == 1
                ? "CPPPDeque<" + cppTypeForExpressionType(type.subtypes[0]) + ">"
                : "";
        case PrimitiveType::Set:
            if (type.subtypes.size() == 1) {
                return "CPPPSet<" + cppTypeForExpressionType(type.subtypes[0]) + ">";
            }
            return "";
        case PrimitiveType::Map:
            if (type.subtypes.size() == 2) {
                return "CPPPMap<" + cppTypeForExpressionType(type.subtypes[0]) + ", " + cppTypeForExpressionType(type.subtypes[1]) + ">";
            }
            return "";
        case PrimitiveType::Pair:
            if (type.subtypes.size() == 2) {
                return "CPPPPair<" + cppTypeForExpressionType(type.subtypes[0]) + ", " + cppTypeForExpressionType(type.subtypes[1]) + ">";
            }
            return "";
        case PrimitiveType::Struct:
            return type.name;
        case PrimitiveType::Class:
            return "cppp_smart_pointer<" + type.name + ">";
        case PrimitiveType::Unknown:
            return "";
    }

    return "";
}

// ExpressionAnalyzer analyzes expressions, checks types, and emits validation logic.
class ExpressionAnalyzer {
public:
    ExpressionAnalyzer(
        const std::string& inputFile,
        int lineNumber,
        const std::map<int, std::string>& sourceLines,
        const std::map<std::string, Type>& declaredVariables,
        const std::map<std::string, FunctionSignature>& declaredFunctions
    ) :
        inputFile(inputFile),
        lineNumber(lineNumber),
        sourceLines(sourceLines),
        declaredVariables(declaredVariables),
        declaredFunctions(declaredFunctions) {}

// analyze analyzes the construct and validates its semantics.
    bool analyze(Expr& expr) {
        if (auto* literal = dynamic_cast<LiteralExpr*>(&expr)) {
            return analyzeLiteral(*literal);
        }
        if (auto* variable = dynamic_cast<VariableExpr*>(&expr)) {
            return analyzeVariable(*variable);
        }
        if (auto* field = dynamic_cast<FieldExpr*>(&expr)) {
            return analyzeField(*field);
        }
        if (auto* unary = dynamic_cast<UnaryExpr*>(&expr)) {
            return analyzeUnary(*unary);
        }
        if (auto* binary = dynamic_cast<BinaryExpr*>(&expr)) {
            return analyzeBinary(*binary);
        }
        if (auto* cast = dynamic_cast<CastExpr*>(&expr)) {
            return analyzeCast(*cast);
        }
        if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
            return analyzeCall(*call);
        }
        if (auto* index = dynamic_cast<IndexExpr*>(&expr)) {
            return analyzeIndex(*index);
        }
        if (auto* slice = dynamic_cast<SliceExpr*>(&expr)) {
            return analyzeSlice(*slice);
        }
        if (auto* list = dynamic_cast<ListLiteralExpr*>(&expr)) {
            return analyzeListLiteral(*list);
        }
        if (auto* set = dynamic_cast<SetLiteralExpr*>(&expr)) {
            return analyzeSetLiteral(*set);
        }
        if (auto* map = dynamic_cast<MapLiteralExpr*>(&expr)) {
            return analyzeMapLiteral(*map);
        }
        if (auto* pair = dynamic_cast<PairLiteralExpr*>(&expr)) {
            return analyzePairLiteral(*pair);
        }

        return false;
    }

private:
    const std::string& inputFile;
    int lineNumber;
    const std::map<int, std::string>& sourceLines;
    const std::map<std::string, Type>& declaredVariables;
    const std::map<std::string, FunctionSignature>& declaredFunctions;

    void report(int column, const std::string& message) const {
        recordSourceError(inputFile, lineNumber, column, message, sourceLines);
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
            return PrimitiveType::Unknown;
        }

        if ((requested.primitive == PrimitiveType::Stack ||
             requested.primitive == PrimitiveType::Queue ||
             requested.primitive == PrimitiveType::Deque) &&
            requested.subtypes.empty()) {
            if (isListType(from)) {
                return Type(requested.primitive, {from.subtypes[0]});
            }
            return PrimitiveType::Unknown;
        }

        return requested;
    }

// sumResultType implements the sumResultType behavior for the expressionParser.cpp module.
    Type sumResultType(Type elementType) const {
        if (elementType == PrimitiveType::Float) {
            return PrimitiveType::Float;
        }
        return PrimitiveType::Int;
    }

// binaryResultType implements the binaryResultType behavior for the expressionParser.cpp module.
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
            report(expr.sourceColumn, "use of undeclared variable '" + expr.name + "'");
            return false;
        }

        if (variable->second == PrimitiveType::Unknown) {
            expr.inferredType = PrimitiveType::Unknown;
            expr.mutableValue = true;
            return false;
        }

        expr.inferredType = variable->second;
        expr.mutableValue = true;
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
            report(expr.sourceColumn, std::string(isClassType(expr.base->inferredType) ? "class " : "struct ") + expr.base->inferredType.name + " has no field '" + expr.field + "'");
            return false;
        }
        expr.inferredType = field->second;
        expr.mutableValue = expr.base->mutableValue;
        return true;
    }

// analyzeUnary analyzes the construct and validates its semantics.
    bool analyzeUnary(UnaryExpr& expr) {
        if (!analyze(*expr.operand)) {
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
            expr.inferredType = PrimitiveType::Int;
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
                const Type& structSide = leftNull ? rightType : leftType;
                if ((expr.op != "==" && expr.op != "!=") || !isClassType(structSide)) {
                    report(expr.sourceColumn, "NULL can only be compared with a class using == or !=");
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

        if (expr.callee == "len") {
            if (expr.arguments.size() != 1) {
                report(expr.sourceColumn, "len must be called as len(collection)");
                return false;
            }
            if (!isCollectionType(expr.arguments[0]->inferredType)) {
                report(expr.sourceColumn, "len() expects a List, Set, or Map value");
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

        if (expr.receiver && isLinearDataStructureType(expr.receiver->inferredType)) {
            const Type receiverType = expr.receiver->inferredType;
            const Type elementType = receiverType.subtypes[0];
            const bool stackOrQueue = isStackType(receiverType) || isQueueType(receiverType);
            const bool deque = isDequeType(receiverType);

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

            report(expr.sourceColumn, cpppTypeName(receiverType) + " has no method '" + expr.callee + "'");
            return false;
        }

        if (expr.receiver && isStructType(expr.receiver->inferredType)) {
            const FunctionSignature* method = declaredStructMethodForType(expr.receiver->inferredType, expr.callee);
            if (method == nullptr) {
                report(expr.sourceColumn, std::string(isClassType(expr.receiver->inferredType) ? "class " : "struct ") + expr.receiver->inferredType.name + " has no method '" + expr.callee + "'");
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

        const Type constructedType = declaredTypeForName(expr.callee);
        if (!expr.receiver && isStructType(constructedType)) {
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
            if (expr.arguments.size() != signature.parameters.size()) {
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
                if (!signature.parameters[i].deepCopy &&
                    (isStringType(parameterType) || isCollectionType(parameterType))) {
                    if (!expr.arguments[i]->mutableValue) {
                        report(expr.sourceColumn, signature.name + " requires collection and string arguments to be mutable variables");
                        return false;
                    }
                }
            }

            expr.inferredType = signature.returnsVoid ? PrimitiveType::Void : signature.returnType;
            return true;
        }

        report(expr.sourceColumn, "unexpected token in expression");
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
                }
            }

            report(expr.sourceColumn, "pair index must be 0 or 1");
            return false;
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
        if (!analyze(*expr.base) || !analyze(*expr.start) || !analyze(*expr.end)) {
            return false;
        }

        if (expr.base->inferredType.primitive != PrimitiveType::List || expr.base->inferredType.subtypes.size() != 1) {
            report(expr.sourceColumn, "slicing requires a List value");
            return false;
        }

        if (expr.start->inferredType != PrimitiveType::Int) {
            report(expr.start->sourceColumn, "slice start must be int");
            return false;
        }

        if (expr.end->inferredType != PrimitiveType::Int) {
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

// ExpressionCodegen holds state or behavior used by the expressionParser.cpp implementation.
class ExpressionCodegen {
public:
    ExpressionCodegen(int lineNumber, bool emitRuntimeChecks, const std::map<std::string, FunctionSignature>& declaredFunctions) :
        lineNumber(lineNumber),
        emitRuntimeChecks(emitRuntimeChecks),
        declaredFunctions(declaredFunctions) {}

// generate implements the generate behavior for the expressionParser.cpp module.
    std::string generate(const Expr& expr) const {
        if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expr)) {
            return generateLiteral(*literal);
        }
        if (const auto* variable = dynamic_cast<const VariableExpr*>(&expr)) {
            return variable->name;
        }
        if (const auto* field = dynamic_cast<const FieldExpr*>(&expr)) {
            return generateField(*field);
        }
        if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
            return generateUnary(*unary);
        }
        if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
            return generateBinary(*binary);
        }
        if (const auto* cast = dynamic_cast<const CastExpr*>(&expr)) {
            return generateCast(*cast);
        }
        if (const auto* call = dynamic_cast<const CallExpr*>(&expr)) {
            return generateCall(*call);
        }
        if (const auto* index = dynamic_cast<const IndexExpr*>(&expr)) {
            return generateIndex(*index);
        }
        if (const auto* slice = dynamic_cast<const SliceExpr*>(&expr)) {
            return generateSlice(*slice);
        }
        if (const auto* list = dynamic_cast<const ListLiteralExpr*>(&expr)) {
            return generateListLiteral(*list);
        }
        if (const auto* set = dynamic_cast<const SetLiteralExpr*>(&expr)) {
            return generateSetLiteral(*set);
        }
        if (const auto* map = dynamic_cast<const MapLiteralExpr*>(&expr)) {
            return generateMapLiteral(*map);
        }
        if (const auto* pair = dynamic_cast<const PairLiteralExpr*>(&expr)) {
            return generatePairLiteral(*pair);
        }

        return "";
    }

private:
    int lineNumber;
    bool emitRuntimeChecks;
    const std::map<std::string, FunctionSignature>& declaredFunctions;

// runtimeErrorThrowExpression provides runtime support for generated code.
    std::string runtimeErrorThrowExpression(int column, const std::string& message) const {
        return "throw runtime_error(\"" + std::to_string(lineNumber) + ":" + std::to_string(column) + ":" + message + "\")";
    }

    std::string generateCast(const CastExpr& expr) const {
        const std::string operand = generate(*expr.operand);
        if (isStringType(expr.operand->inferredType)) {
            if (expr.targetType == PrimitiveType::Bool) {
                requireRuntimeHelper("CPPPStringToBool");
                return "CPPPStringToBool(" + operand + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
            if (expr.targetType == PrimitiveType::Char) {
                requireRuntimeHelper("CPPPStringToChar");
                return "CPPPStringToChar(" + operand + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
            if (expr.targetType == PrimitiveType::Int) {
                requireRuntimeHelper("CPPPStringToInt");
                return "CPPPStringToInt(" + operand + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
            if (expr.targetType == PrimitiveType::Float) {
                requireRuntimeHelper("CPPPStringToFloat");
                return "CPPPStringToFloat(" + operand + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
        }
        return castExpressionTo(operand, expr.operand->inferredType, expr.targetType);
    }

    std::string checkedIntegerExpression(
        const std::string& left,
        const std::string& right,
        const std::string& op,
        int column
    ) const {
        if (op == "/") {
            return "([&](long long __cppp_left, long long __cppp_right) { "
                "if (__cppp_right == 0) { " + runtimeErrorThrowExpression(column, "division by zero") + "; } "
                "if (__cppp_left == LLONG_MIN && __cppp_right == -1) { " + runtimeErrorThrowExpression(column, "integer overflow") + "; } "
                "return __cppp_left / __cppp_right; "
                "})(" + left + ", " + right + ")";
        }

        if (op == "%") {
            return "([&](long long __cppp_left, long long __cppp_right) { "
                "if (__cppp_right == 0) { " + runtimeErrorThrowExpression(column, "modulo by zero") + "; } "
                "if (__cppp_left == LLONG_MIN && __cppp_right == -1) { " + runtimeErrorThrowExpression(column, "integer overflow") + "; } "
                "return __cppp_left % __cppp_right; "
                "})(" + left + ", " + right + ")";
        }

        if (op == "+") {
            return "([&](long long __cppp_left, long long __cppp_right) { "
                "if ((__cppp_right > 0 && __cppp_left > LLONG_MAX - __cppp_right) || (__cppp_right < 0 && __cppp_left < LLONG_MIN - __cppp_right)) { " + runtimeErrorThrowExpression(column, "integer overflow") + "; } "
                "return __cppp_left + __cppp_right; "
                "})(" + left + ", " + right + ")";
        }

        if (op == "-") {
            return "([&](long long __cppp_left, long long __cppp_right) { "
                "if ((__cppp_right < 0 && __cppp_left > LLONG_MAX + __cppp_right) || (__cppp_right > 0 && __cppp_left < LLONG_MIN + __cppp_right)) { " + runtimeErrorThrowExpression(column, "integer overflow") + "; } "
                "return __cppp_left - __cppp_right; "
                "})(" + left + ", " + right + ")";
        }

        if (op == "*") {
            return "([&](long long __cppp_left, long long __cppp_right) { "
                "__int128 __cppp_product = static_cast<__int128>(__cppp_left) * static_cast<__int128>(__cppp_right); "
                "if (__cppp_product > LLONG_MAX || __cppp_product < LLONG_MIN) { " + runtimeErrorThrowExpression(column, "integer overflow") + "; } "
                "return static_cast<long long>(__cppp_product); "
                "})(" + left + ", " + right + ")";
        }

        return "(" + left + " " + op + " " + right + ")";
    }

// concatenatedListExpression implements the concatenatedListExpression behavior for the expressionParser.cpp module.
    std::string concatenatedListExpression(const BinaryExpr& expr) const {
        const std::string left = generate(*expr.left);
        const std::string right = generate(*expr.right);
        const std::string elementType = cppTypeForExpressionType(expr.inferredType.subtypes[0]);
        return "([&]() { const auto& __cppp_left_source = " + left + "; CPPPList<" + elementType + "> __cppp_left(__cppp_left_source.begin(), __cppp_left_source.end()); auto __cppp_right = " + right +
            "; __cppp_left.insert(__cppp_left.end(), __cppp_right.begin(), __cppp_right.end()); return __cppp_left; }())";
    }

// generateLiteral implements the generateLiteral behavior for the expressionParser.cpp module.
    std::string generateLiteral(const LiteralExpr& expr) const {
        switch (expr.kind) {
            case LiteralExpr::Kind::Bool:
            case LiteralExpr::Kind::Null:
            case LiteralExpr::Kind::Int:
            case LiteralExpr::Kind::Float:
                return expr.kind == LiteralExpr::Kind::Null ? "nullptr" : expr.text;
            case LiteralExpr::Kind::String:
                requireRuntimeHelper("CPPPStringLiteral");
                return "CPPPStringLiteral(" + expr.text + ")";
            case LiteralExpr::Kind::Char:
                requireRuntimeHelper("CPPPCharType");
                return "CPPPChar(" + expr.text + ")";
        }
        return expr.text;
    }

// generateUnary implements the generateUnary behavior for the expressionParser.cpp module.
    std::string generateUnary(const UnaryExpr& expr) const {
        if (expr.op == "++" || expr.op == "--") {
            if (expr.operand->inferredType == PrimitiveType::Char) {
                requireRuntimeHelper(expr.op == "++" ? "CPPPCharIncrement" : "CPPPCharDecrement");
            }
            const std::string operand = generateMutableAccess(*expr.operand);
            return "([&]() { auto& __cppp_ref = " + operand + "; auto __cppp_old = __cppp_ref; " +
                expr.op + "__cppp_ref; return " + (expr.postfix ? "__cppp_old" : "__cppp_ref") + "; }())";
        }
        const std::string operand = generate(*expr.operand);
        if (expr.op == "!") {
            return "(!" + castExpressionTo(operand, expr.operand->inferredType, PrimitiveType::Bool) + ")";
        }
        return "(" + expr.op + operand + ")";
    }

// generateBinary implements the generateBinary behavior for the expressionParser.cpp module.
    std::string generateBinary(const BinaryExpr& expr) const {
        const std::string left = generate(*expr.left);
        const std::string right = generate(*expr.right);

        if ((isCollectionType(expr.left->inferredType) || isPairType(expr.left->inferredType)) &&
            (expr.op == "==" || expr.op == "!=" || expr.op == "<" || expr.op == ">" || expr.op == "<=" || expr.op == ">=")) {
            requireRuntimeHelper("CPPPContainerCompare");
        }

        if (expr.op == "in") {
            if (isRangeType(expr.right->inferredType)) {
                std::string needle = left;
                if (expr.left->inferredType != PrimitiveType::Int) {
                    needle = castExpressionTo(needle, expr.left->inferredType, PrimitiveType::Int);
                }
                return "((" + right + ").contains(" + needle + "))";
            }
            if (isListType(expr.right->inferredType) && isListType(expr.left->inferredType)) {
                const Type elementType = expr.right->inferredType.subtypes[0];
                if (expr.left->inferredType == elementType) {
                    return "([&]() { const auto& __cppp_list = " + right + "; return find(__cppp_list.begin(), __cppp_list.end(), " + left + ") != __cppp_list.end(); }())";
                }
                if (expr.left->inferredType != expr.right->inferredType) {
                    return "false";
                }
                requireRuntimeHelper("CPPPListContainsSublist");
                return "CPPPListContainsSublist(" + right + ", " + left + ")";
            }
            const Type elementType = expr.right->inferredType.subtypes[0];
            std::string needle = left;
            if (!isImplicitlyConvertible(expr.left->inferredType, elementType) || expr.left->inferredType != elementType) {
                needle = castExpressionTo(needle, expr.left->inferredType, elementType);
            }
            if (isListType(expr.right->inferredType)) {
                return "([&]() { const auto& __cppp_list = " + right + "; return find(__cppp_list.begin(), __cppp_list.end(), " + needle + ") != __cppp_list.end(); }())";
            }
            if (isSetType(expr.right->inferredType)) {
                return "([&]() { const auto& __cppp_set = " + right + "; return __cppp_set.find(" + needle + ") != __cppp_set.end(); }())";
            }

            return "([&]() { const auto& __cppp_map = " + right + "; return __cppp_map.find(" + needle + ") != __cppp_map.end(); }())";
        }

        if (expr.op == "||" || expr.op == "&&") {
            return "(" + castExpressionTo(left, expr.left->inferredType, PrimitiveType::Bool) + " " + expr.op + " " + castExpressionTo(right, expr.right->inferredType, PrimitiveType::Bool) + ")";
        }

        const auto* leftLiteral = dynamic_cast<const LiteralExpr*>(expr.left.get());
        const auto* rightLiteral = dynamic_cast<const LiteralExpr*>(expr.right.get());
        const bool leftNull = leftLiteral != nullptr && leftLiteral->kind == LiteralExpr::Kind::Null;
        const bool rightNull = rightLiteral != nullptr && rightLiteral->kind == LiteralExpr::Kind::Null;
        if ((expr.op == "==" || expr.op == "!=") && isClassType(expr.left->inferredType) && rightNull) {
            const std::string equal = "(" + left + " == nullptr)";
            return expr.op == "==" ? equal : "(!" + equal + ")";
        }

        if ((expr.op == "==" || expr.op == "!=") && isClassType(expr.right->inferredType) && leftNull) {
            const std::string equal = "(" + right + " == nullptr)";
            return expr.op == "==" ? equal : "(!" + equal + ")";
        }

        if ((expr.op == "==" || expr.op == "!=") && isClassType(expr.left->inferredType)) {
            const std::string equal = "((" + left + " && " + right + ") ? (*" + left + " == *" + right + ") : (!" + left + " && !" + right + "))";
            return expr.op == "==" ? equal : "(!" + equal + ")";
        }


        if (emitRuntimeChecks && expr.inferredType == PrimitiveType::Int) {
            return checkedIntegerExpression(left, right, expr.op, expr.sourceColumn);
        }

        if (expr.op == "+" && isListType(expr.inferredType)) {
            return concatenatedListExpression(expr);
        }

        return "(" + left + " " + expr.op + " " + right + ")";
    }

// generateCall implements the generateCall behavior for the expressionParser.cpp module.
    std::string generateCall(const CallExpr& expr) const {
        if (expr.receiver && isStructType(expr.receiver->inferredType)) {
            const std::string receiver = generate(*expr.receiver);
            requireStructMethod(expr.receiver->inferredType.name, expr.callee);
            const FunctionSignature* method = declaredStructMethodForType(expr.receiver->inferredType, expr.callee);
            const bool isClass = isClassType(expr.receiver->inferredType);
            std::string call = "(" + receiver + ")" + (isClass ? "->" : ".") + expr.callee + "(";
            for (size_t i = 0; i < expr.arguments.size(); ++i) {
                if (i > 0) call += ", ";
                std::string argument = generate(*expr.arguments[i]);
                if (method != nullptr && method->parameters[i].deepCopy) {
                    requireCopyHelpersForType(method->parameters[i].type);
                    argument = "CPPPCopy(" + argument + ")";
                }
                call += argument;
            }
            call += ")";
            if (!isClass || !emitRuntimeChecks) {
                return call;
            }
            return "([&]() -> decltype(auto) { auto& __cppp_object = " + receiver + "; if (!__cppp_object) { " +
                runtimeErrorThrowExpression(expr.sourceColumn, "cannot call " + expr.callee + "() on null " + expr.receiver->inferredType.name) +
                "; } return " + call + "; }())";
        }
        const Type constructedType = declaredTypeForName(expr.callee);
        if (isStructType(constructedType) && !expr.receiver) {
            std::string generated = isClassType(constructedType)
                ? "cppp_smart_pointer<" + constructedType.name + ">::make("
                : constructedType.name + "(";
            for (size_t i = 0; i < expr.arguments.size(); ++i) {
                if (i > 0) {
                    generated += ", ";
                }
                std::string argument = generate(*expr.arguments[i]);
                generated += argument;
            }
            return generated + ")";
        }

        if (expr.callee == "len") {
            return "static_cast<long long>((" + generate(*expr.arguments[0]) + ").size())";
        }

        if (expr.callee == "copy") {
            requireCopyHelpersForType(expr.arguments[0]->inferredType);
            return "CPPPCopy(" + generate(*expr.arguments[0]) + ")";
        }

        if (expr.receiver && isLinearDataStructureType(expr.receiver->inferredType)) {
            const std::string receiver = generate(*expr.receiver);
            const Type receiverType = expr.receiver->inferredType;
            if (expr.callee == "top" || expr.callee == "front" || expr.callee == "back" ||
                expr.callee == "pop" || expr.callee == "popFront" || expr.callee == "popBack") {
                std::string method = expr.callee;
                if (method == "pop") method = "pop_value";
                if (method == "popFront") method = "pop_front_value";
                if (method == "popBack") method = "pop_back_value";
                return "(" + receiver + ")." + method + "(" + std::to_string(lineNumber) + ", " +
                    std::to_string(expr.sourceColumn) + ")";
            }
            if (expr.callee == "addFront" || expr.callee == "addBack") {
                std::string value = generate(*expr.arguments[0]);
                const Type elementType = receiverType.subtypes[0];
                if (expr.arguments[0]->inferredType != elementType) {
                    value = castExpressionTo(value, expr.arguments[0]->inferredType, elementType);
                }
                return "(" + receiver + ")." +
                    (expr.callee == "addFront" ? "push_front" : "push_back") +
                    "(" + value + ")";
            }
        }

        if (expr.callee == "remove") {
            const std::string receiver = generate(*expr.receiver);
            if (isListType(expr.receiver->inferredType) && expr.arguments.empty()) {
                if (emitRuntimeChecks) {
                    requireRuntimeHelper("CPPPListPop");
                    return "CPPPListPop(" + receiver + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                }
                return "([&]() { auto __cppp_removed = (" + receiver + ").back(); (" + receiver + ").pop_back(); return __cppp_removed; }())";
            }

            if (isListType(expr.receiver->inferredType)) {
                std::string index = generate(*expr.arguments[0]);
                if (!isImplicitlyConvertible(expr.arguments[0]->inferredType, PrimitiveType::Int) || expr.arguments[0]->inferredType != PrimitiveType::Int) {
                    index = castExpressionTo(index, expr.arguments[0]->inferredType, PrimitiveType::Int);
                }
                if (emitRuntimeChecks) {
                    requireRuntimeHelper("CPPPListRemoveAt");
                    return "CPPPListRemoveAt(" + receiver + ", " + index + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                }
                return "([&]() { auto __cppp_removed = (" + receiver + ")[" + index + "]; (" + receiver + ").erase((" + receiver + ").begin() + " + index + "); return __cppp_removed; }())";
            }

            std::string key = generate(*expr.arguments[0]);
            const Type keyType = expr.receiver->inferredType.subtypes[0];
            if (!isImplicitlyConvertible(expr.arguments[0]->inferredType, keyType) || expr.arguments[0]->inferredType != keyType) {
                key = castExpressionTo(key, expr.arguments[0]->inferredType, keyType);
            }
            if (isSetType(expr.receiver->inferredType)) {
                requireRuntimeHelper("CPPPSetRemove");
                return "CPPPSetRemove(" + receiver + ", " + key + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
            requireRuntimeHelper("CPPPMapRemove");
            return "CPPPMapRemove(" + receiver + ", " + key + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
        }

        if (expr.callee == "at") {
            const std::string receiver = generate(*expr.receiver);
            std::string key = generate(*expr.arguments[0]);
            const Type keyType = expr.receiver->inferredType.subtypes[0];
            if (!isImplicitlyConvertible(expr.arguments[0]->inferredType, keyType) || expr.arguments[0]->inferredType != keyType) {
                key = castExpressionTo(key, expr.arguments[0]->inferredType, keyType);
            }
            if (emitRuntimeChecks) {
                requireRuntimeHelper("CPPPMapAt");
                return "CPPPMapAt(" + receiver + ", " + key + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
            return "(" + receiver + ").at(" + key + ")";
        }

        if (expr.callee == "prev" || expr.callee == "next" || expr.callee == "hasPrev" || expr.callee == "hasNext") {
            const std::string receiver = generate(*expr.receiver);
            std::string key = generate(*expr.arguments[0]);
            const Type keyType = expr.receiver->inferredType.subtypes[0];
            if (!isImplicitlyConvertible(expr.arguments[0]->inferredType, keyType) || expr.arguments[0]->inferredType != keyType) {
                key = castExpressionTo(key, expr.arguments[0]->inferredType, keyType);
            }

            if (expr.callee == "hasPrev" || expr.callee == "hasNext") {
                if (isSetType(expr.receiver->inferredType)) {
                    requireRuntimeHelper(expr.callee == "hasPrev" ? "CPPPSetHasPrev" : "CPPPSetHasNext");
                    return (expr.callee == "hasPrev" ? "CPPPSetHasPrev(" : "CPPPSetHasNext(") + receiver + ", " + key + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                }
                requireRuntimeHelper(expr.callee == "hasPrev" ? "CPPPMapHasPrev" : "CPPPMapHasNext");
                return (expr.callee == "hasPrev" ? "CPPPMapHasPrev(" : "CPPPMapHasNext(") + receiver + ", " + key + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }

            if (isSetType(expr.receiver->inferredType)) {
                requireRuntimeHelper(expr.callee == "prev" ? "CPPPSetPrev" : "CPPPSetNext");
                return (expr.callee == "prev" ? "CPPPSetPrev(" : "CPPPSetNext(") + receiver + ", " + key + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }

            requireRuntimeHelper(expr.callee == "prev" ? "CPPPMapPrev" : "CPPPMapNext");
            return (expr.callee == "prev" ? "CPPPMapPrev(" : "CPPPMapNext(") + receiver + ", " + key + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
        }

        if (expr.callee == "find") {
            const std::string receiver = generate(*expr.receiver);
            const Type haystackType = expr.receiver->inferredType;
            const Type elementType = haystackType.subtypes[0];
            const Type needleType = expr.arguments[0]->inferredType;
            if (isListType(needleType) && needleType == haystackType) {
                requireRuntimeHelper("CPPPListFindSublist");
                return "CPPPListFindSublist(" + receiver + ", " + generate(*expr.arguments[0]) + ")";
            }

            std::string needle = generate(*expr.arguments[0]);
            if (!isImplicitlyConvertible(needleType, elementType) || needleType != elementType) {
                needle = castExpressionTo(needle, needleType, elementType);
            }
            requireRuntimeHelper("CPPPListFindValue");
            return "CPPPListFindValue(" + receiver + ", " + needle + ")";
        }

        if (expr.callee == "split") {
            const std::string haystack = generate(*expr.receiver);
            const Type haystackType = expr.receiver->inferredType;
            const Type elementType = haystackType.subtypes[0];
            const Type delimiterType = expr.arguments[0]->inferredType;
            if (isListType(delimiterType) && delimiterType == haystackType) {
                requireRuntimeHelper("CPPPListSplitSublist");
                return "CPPPListSplitSublist(" + haystack + ", " + generate(*expr.arguments[0]) + ")";
            }

            std::string delimiter = generate(*expr.arguments[0]);
            if (!isImplicitlyConvertible(delimiterType, elementType) || delimiterType != elementType) {
                delimiter = castExpressionTo(delimiter, delimiterType, elementType);
            }
            requireRuntimeHelper("CPPPListSplitValue");
            return "CPPPListSplitValue(" + haystack + ", " + delimiter + ")";
        }

        if (expr.callee == "range") {
            if (expr.arguments.size() == 1) {
                requireRuntimeHelper("CPPPRangeMakeStop");
                std::string stop = generate(*expr.arguments[0]);
                if (expr.arguments[0]->inferredType != PrimitiveType::Int) {
                    stop = castExpressionTo(stop, expr.arguments[0]->inferredType, PrimitiveType::Int);
                }
                return "CPPPMakeRange(" + stop + ")";
            }
            if (expr.arguments.size() == 2) {
                requireRuntimeHelper("CPPPRangeMakeBounds");
                std::string start = generate(*expr.arguments[0]);
                std::string stop = generate(*expr.arguments[1]);
                if (expr.arguments[0]->inferredType != PrimitiveType::Int) {
                    start = castExpressionTo(start, expr.arguments[0]->inferredType, PrimitiveType::Int);
                }
                if (expr.arguments[1]->inferredType != PrimitiveType::Int) {
                    stop = castExpressionTo(stop, expr.arguments[1]->inferredType, PrimitiveType::Int);
                }
                return "CPPPMakeRange(" + start + ", " + stop + ")";
            }

            requireRuntimeHelper("CPPPRangeMakeStep");
            std::string start = generate(*expr.arguments[0]);
            std::string stop = generate(*expr.arguments[1]);
            std::string step = generate(*expr.arguments[2]);
            if (expr.arguments[0]->inferredType != PrimitiveType::Int) {
                start = castExpressionTo(start, expr.arguments[0]->inferredType, PrimitiveType::Int);
            }
            if (expr.arguments[1]->inferredType != PrimitiveType::Int) {
                stop = castExpressionTo(stop, expr.arguments[1]->inferredType, PrimitiveType::Int);
            }
            if (expr.arguments[2]->inferredType != PrimitiveType::Int) {
                step = castExpressionTo(step, expr.arguments[2]->inferredType, PrimitiveType::Int);
            }
            return "CPPPMakeRange(" + start + ", " + stop + ", " + step + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
        }

        if (expr.callee == "min") {
            if(expr.arguments.size() == 1){
                const std::string list = generate(*expr.arguments[0]);
                if (isListType(expr.arguments[0]->inferredType) && emitRuntimeChecks) {
                    requireRuntimeHelper("CPPPListMin");
                    return "CPPPListMin(" + list + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                }
                if (isSetType(expr.arguments[0]->inferredType)) {
                    if (emitRuntimeChecks) {
                        requireRuntimeHelper("CPPPSetMin");
                        return "CPPPSetMin(" + list + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                    }
                    return "(*(" + list + ").begin())";
                }
                if (isMapType(expr.arguments[0]->inferredType)) {
                    if (emitRuntimeChecks) {
                        requireRuntimeHelper("CPPPMapMin");
                        return "CPPPMapMin(" + list + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                    }
                    return "((" + list + ").begin()->first)";
                }
                return "([&]() { auto __cppp_list = " + list + "; return *min_element(__cppp_list.begin(), __cppp_list.end()); }())";
            } else {
                std::string retLine = "min(";
                for(unsigned int i = 0;i<expr.arguments.size() - 2;i++){
                    retLine += generate(*expr.arguments[i]) + ",min(";
                }
                retLine += generate(*expr.arguments[expr.arguments.size() - 2]) + "," + generate(*expr.arguments[expr.arguments.size() - 1]);
                for(unsigned int i = 1;i<expr.arguments.size();i++){
                    retLine += ")";
                }
                return retLine;
            }
        }

        if (expr.callee == "max") {
            if(expr.arguments.size() == 1){
                const std::string list = generate(*expr.arguments[0]);
                if (isListType(expr.arguments[0]->inferredType) && emitRuntimeChecks) {
                    requireRuntimeHelper("CPPPListMax");
                    return "CPPPListMax(" + list + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                }
                if (isSetType(expr.arguments[0]->inferredType)) {
                    if (emitRuntimeChecks) {
                        requireRuntimeHelper("CPPPSetMax");
                        return "CPPPSetMax(" + list + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                    }
                    return "(*(" + list + ").rbegin())";
                }
                if (isMapType(expr.arguments[0]->inferredType)) {
                    if (emitRuntimeChecks) {
                        requireRuntimeHelper("CPPPMapMax");
                        return "CPPPMapMax(" + list + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                    }
                    return "((" + list + ").rbegin()->first)";
                }
                return "([&]() { auto __cppp_list = " + list + "; return *max_element(__cppp_list.begin(), __cppp_list.end()); }())";
            } else {
                std::string retLine = "max(";
                for(unsigned int i = 0;i<expr.arguments.size() - 2;i++){
                    retLine += generate(*expr.arguments[i]) + ",max(";
                }
                retLine += generate(*expr.arguments[expr.arguments.size() - 2]) + "," + generate(*expr.arguments[expr.arguments.size() - 1]);
                for(unsigned int i = 1;i<expr.arguments.size();i++){
                    retLine += ")";
                }
                return retLine;
            }
        }
        if (expr.callee == "abs"){
            const std::string num = generate(*expr.arguments[0]);
            return "abs(" + num + ")";
        }
        if (expr.callee == "sum") {
            const std::string list = generate(*expr.arguments[0]);
            const Type elementType = expr.arguments[0]->inferredType.subtypes[0];
            const std::string initial = elementType == PrimitiveType::Float ? "0.0L" : "0LL";
            return "([&]() { auto __cppp_list = " + list + "; return accumulate(__cppp_list.begin(), __cppp_list.end(), " + initial + "); }())";
        }

        std::string generated = expr.callee + "(";
        const auto function = declaredFunctions.find(expr.callee);
        for (size_t i = 0; i < expr.arguments.size(); ++i) {
            if (i > 0) {
                generated += ", ";
            }
            std::string argument = generate(*expr.arguments[i]);
            if (function != declaredFunctions.end()) {
                const Type parameterType = function->second.parameters[i].type;
                if (expr.arguments[i]->inferredType != parameterType) {
                    argument = castExpressionTo(argument, expr.arguments[i]->inferredType, parameterType);
                }
                if (function->second.parameters[i].deepCopy) {
                    requireCopyHelpersForType(function->second.parameters[i].type);
                    argument = "CPPPCopy(" + argument + ")";
                }
            }
            generated += argument;
        }
        generated += ")";
        return generated;

        return "";
    }

// generateIndex implements the generateIndex behavior for the expressionParser.cpp module.
    std::string generateIndex(const IndexExpr& expr) const {
        const std::string base = generate(*expr.base);
        std::string index = generate(*expr.index);
        if (isListType(expr.base->inferredType)) {
            if (emitRuntimeChecks) {
                requireRuntimeHelper("CPPPListAt");
                return "CPPPListAt(" + base + ", " + index + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
            return "([&]() { const auto& __cppp_list = " + base + "; auto __cppp_index = static_cast<long long>(" + index + "); if (__cppp_index < 0) __cppp_index += static_cast<long long>(__cppp_list.size()); return (__cppp_list[__cppp_index]); }())";
        }
        if (isPairType(expr.base->inferredType)) {
            const auto* literal = dynamic_cast<const LiteralExpr*>(expr.index.get());
            return "((" + base + ")." + (literal != nullptr && literal->text == "0" ? "first()" : "second()") + ")";
        }
        const Type keyType = expr.base->inferredType.subtypes[0];
        if (!isImplicitlyConvertible(expr.index->inferredType, keyType) || expr.index->inferredType != keyType) {
            index = castExpressionTo(index, expr.index->inferredType, keyType);
        }
        return "((" + base + ")[" + index + "])";
    }

// generateMutableAccess implements the generateMutableAccess behavior for the expressionParser.cpp module.
    std::string generateMutableAccess(const Expr& expr) const {
        if (const auto* variable = dynamic_cast<const VariableExpr*>(&expr)) {
            return variable->name;
        }
        if (const auto* index = dynamic_cast<const IndexExpr*>(&expr)) {
            const std::string base = generateMutableAccess(*index->base);
            std::string generatedIndex = generate(*index->index);
            if (isListType(index->base->inferredType)) {
                if (index->index->inferredType != PrimitiveType::Int) {
                    generatedIndex = castExpressionTo(generatedIndex, index->index->inferredType, PrimitiveType::Int);
                }
                if (emitRuntimeChecks) {
                    requireRuntimeHelper("CPPPListRef");
                    return "CPPPListRef(" + base + ", " + generatedIndex + ", " + std::to_string(lineNumber) + ", " + std::to_string(index->sourceColumn) + ")";
                }
                return "([&]() -> auto& { auto& __cppp_list = " + base + "; auto __cppp_index = static_cast<long long>(" + generatedIndex + "); if (__cppp_index < 0) __cppp_index += static_cast<long long>(__cppp_list.size()); return __cppp_list[__cppp_index]; }())";
            }
            if (isPairType(index->base->inferredType)) {
                const auto* literal = dynamic_cast<const LiteralExpr*>(index->index.get());
                return "((" + base + ")." + (literal != nullptr && literal->text == "0" ? "first()" : "second()") + ")";
            }
            const Type keyType = index->base->inferredType.subtypes[0];
            if (!isImplicitlyConvertible(index->index->inferredType, keyType) || index->index->inferredType != keyType) {
                generatedIndex = castExpressionTo(generatedIndex, index->index->inferredType, keyType);
            }
            return "((" + base + ")[" + generatedIndex + "])";
        }
        if (const auto* field = dynamic_cast<const FieldExpr*>(&expr)) {
            const std::string base = generate(*field->base);
            return "((" + base + ")->" + field->field + ")";
        }
        return generate(expr);
    }

// generateSlice implements the generateSlice behavior for the expressionParser.cpp module.
    std::string generateSlice(const SliceExpr& expr) const {
        const std::string base = generate(*expr.base);
        const std::string start = generate(*expr.start);
        const std::string end = generate(*expr.end);
        if (emitRuntimeChecks) {
            requireRuntimeHelper("CPPPListSlice");
            return "CPPPListSlice(" + base + ", " + start + ", " + end + ")";
        }
        return "([&]() { const auto& __cppp_list = " + base + "; long long __cppp_start = static_cast<long long>(" + start + "); long long __cppp_end = static_cast<long long>(" + end + "); long long __cppp_size = static_cast<long long>(__cppp_list.size()); if (__cppp_start < 0) __cppp_start += __cppp_size; if (__cppp_end < 0) __cppp_end += __cppp_size; __cppp_start = max(0LL, min(__cppp_start, __cppp_size)); __cppp_end = max(0LL, min(__cppp_end, __cppp_size)); if (__cppp_start >= __cppp_end) return CPPPList<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">{}; return CPPPList<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">(__cppp_list.begin() + static_cast<CPPPList<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">::difference_type>(__cppp_start), __cppp_list.begin() + static_cast<CPPPList<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">::difference_type>(__cppp_end)); }())";
    }

// generateListLiteral implements the generateListLiteral behavior for the expressionParser.cpp module.
    std::string generateListLiteral(const ListLiteralExpr& expr) const {
        const Type elementType = expr.inferredType.subtypes[0];
        std::string generated = "CPPPList<" + cppTypeForExpressionType(elementType) + ">{";
        for (size_t i = 0; i < expr.elements.size(); ++i) {
            if (i > 0) {
                generated += ", ";
            }
            std::string element = generate(*expr.elements[i]);
            if (expr.elements[i]->inferredType != elementType) {
                element = castExpressionTo(element, expr.elements[i]->inferredType, elementType);
            }
            generated += element;
        }
        generated += "}";
        return generated;
    }

    std::string generateSetLiteral(const SetLiteralExpr& expr) const {
        const Type elementType = expr.inferredType.subtypes[0];
        std::string generated = "CPPPSet<" + cppTypeForExpressionType(elementType) + ">{";
        for (size_t i = 0; i < expr.elements.size(); ++i) {
            if (i > 0) {
                generated += ", ";
            }
            std::string element = generate(*expr.elements[i]);
            if (expr.elements[i]->inferredType != elementType) {
                element = castExpressionTo(element, expr.elements[i]->inferredType, elementType);
            }
            generated += element;
        }
        generated += "}";
        return generated;
    }

    std::string generateMapLiteral(const MapLiteralExpr& expr) const {
        const Type keyType = expr.inferredType.subtypes[0];
        const Type valueType = expr.inferredType.subtypes[1];
        std::string generated = "CPPPMap<" + cppTypeForExpressionType(keyType) + ", " + cppTypeForExpressionType(valueType) + ">{";
        for (size_t i = 0; i < expr.entries.size(); ++i) {
            if (i > 0) {
                generated += ", ";
            }
            std::string key = generate(*expr.entries[i].key);
            std::string value = generate(*expr.entries[i].value);
            if (expr.entries[i].key->inferredType != keyType) {
                key = castExpressionTo(key, expr.entries[i].key->inferredType, keyType);
            }
            if (expr.entries[i].value->inferredType != valueType) {
                value = castExpressionTo(value, expr.entries[i].value->inferredType, valueType);
            }
            generated += "{" + key + ", " + value + "}";
        }
        generated += "}";
        return generated;
    }

    std::string generatePairLiteral(const PairLiteralExpr& expr) const {
        return "CPPPPair<" + cppTypeForExpressionType(expr.first->inferredType) + ", " + cppTypeForExpressionType(expr.second->inferredType) + ">(" + generate(*expr.first) + ", " + generate(*expr.second) + ")";
    }

    std::string generateField(const FieldExpr& expr) const {
        const std::string base = generate(*expr.base);
        if (!isClassType(expr.base->inferredType)) {
            return "((" + base + ")." + expr.field + ")";
        }
        if (!emitRuntimeChecks) {
            return "((" + base + ")->" + expr.field + ")";
        }
        return "([&]() -> decltype(auto) { auto& __cppp_object = " + base + "; if (!__cppp_object) { " +
            runtimeErrorThrowExpression(expr.sourceColumn, "cannot access a field on null " + expr.base->inferredType.name) + "; } return (__cppp_object->" + expr.field + "); }())";
    }
};

std::string generateMutableAccessExpression(
    const Expr& expr,
    int lineNumber,
    bool emitRuntimeChecks,
    const std::map<std::string, FunctionSignature>& declaredFunctions
) {
    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expr)) {
        return variable->name;
    }
    if (const auto* index = dynamic_cast<const IndexExpr*>(&expr)) {
        const std::string base = generateMutableAccessExpression(*index->base, lineNumber, emitRuntimeChecks, declaredFunctions);
        ExpressionCodegen codegen(lineNumber, emitRuntimeChecks, declaredFunctions);
        std::string generatedIndex = codegen.generate(*index->index);
        if (isListType(index->base->inferredType)) {
            if (index->index->inferredType != PrimitiveType::Int) {
                generatedIndex = castExpressionTo(generatedIndex, index->index->inferredType, PrimitiveType::Int);
            }
            if (emitRuntimeChecks) {
                requireRuntimeHelper("CPPPListRef");
                return "CPPPListRef(" + base + ", " + generatedIndex + ", " + std::to_string(lineNumber) + ", " + std::to_string(index->sourceColumn) + ")";
            }
            return "([&]() -> decltype(auto) { auto& __cppp_list = " + base + "; auto __cppp_index = static_cast<long long>(" + generatedIndex + "); if (__cppp_index < 0) __cppp_index += static_cast<long long>(__cppp_list.size()); return __cppp_list[__cppp_index]; }())";
        }
        if (isPairType(index->base->inferredType)) {
            const auto* literal = dynamic_cast<const LiteralExpr*>(index->index.get());
            return "((" + base + ")." + (literal != nullptr && literal->text == "0" ? "first()" : "second()") + ")";
        }
        const Type keyType = index->base->inferredType.subtypes[0];
        if (!isImplicitlyConvertible(index->index->inferredType, keyType) || index->index->inferredType != keyType) {
            generatedIndex = castExpressionTo(generatedIndex, index->index->inferredType, keyType);
        }
        return "((" + base + ")[" + generatedIndex + "])";
    }
    if (const auto* field = dynamic_cast<const FieldExpr*>(&expr)) {
        const std::string base = ExpressionCodegen(lineNumber, emitRuntimeChecks, declaredFunctions).generate(*field->base);
        return "((" + base + ")" + (isClassType(field->base->inferredType) ? "->" : ".") + field->field + ")";
    }
    return ExpressionCodegen(lineNumber, emitRuntimeChecks, declaredFunctions).generate(expr);
}
}

ExpressionParser::ExpressionParser(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    bool emitRuntimeChecks
) :
    inputFile(inputFile),
    lineNumber(lineNumber),
    expressionText(expressionText),
    expressionColumn(expressionColumn),
    sourceLines(sourceLines),
    declaredVariables(declaredVariables),
    declaredFunctions(declaredFunctions),
    emitRuntimeChecks(emitRuntimeChecks),
    tokens(tokenize(expressionText)) {}

ExpressionEmitResult ExpressionParser::parse() {
    for (const Token& token : tokens) {
        if (isUnterminatedQuotedToken(token)) {
            report(token, token.kind == TokenKind::Char ? "unterminated char literal" : "unterminated string literal");
            return {false, "", PrimitiveType::Unknown, false, {}};
        }
    }

    bool ok = true;
    std::unique_ptr<Expr> expression = parseAst(ok);
    if (!ok || !expression) {
        return {false, "", PrimitiveType::Unknown, false, {}};
    }

// analyzer analyzes the construct and validates its semantics.
    ExpressionAnalyzer analyzer(inputFile, lineNumber, sourceLines, declaredVariables, declaredFunctions);
    if (!analyzer.analyze(*expression)) {
        return {false, "", PrimitiveType::Unknown, false, {}};
    }

    ExpressionCodegen codegen(lineNumber, emitRuntimeChecks, declaredFunctions);
    return {
        true,
        codegen.generate(*expression),
        expression->inferredType,
        expression->explicitCast,
        {{
            lineNumber,
            expression->sourceColumn,
            0,
            0
        }}
    };
}

LvalueEmitResult emitLvalueExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks
) {
    static const std::map<std::string, FunctionSignature> emptyFunctions;
    return emitLvalueExpression(
        inputFile,
        lineNumber,
        expressionText,
        expressionColumn,
        sourceLines,
        declaredVariables,
        emptyFunctions,
        emitRuntimeChecks
    );
}

LvalueEmitResult emitLvalueExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    bool emitRuntimeChecks
) {
    ExpressionParser parser(
        inputFile,
        lineNumber,
        expressionText,
        expressionColumn,
        sourceLines,
        declaredVariables,
        declaredFunctions,
        emitRuntimeChecks
    );

    for (const Token& token : tokenize(expressionText)) {
        if ((token.kind == TokenKind::String || token.kind == TokenKind::Char) &&
            (token.text.size() < 2 || token.text.front() != token.text.back())) {
            recordSourceError(
                inputFile,
                lineNumber,
                expressionColumn + token.span.startColumn - 1,
                token.kind == TokenKind::Char ? "unterminated char literal" : "unterminated string literal",
                sourceLines
            );
            return {false, "", PrimitiveType::Unknown, expressionColumn};
        }
    }

    bool ok = true;
    std::unique_ptr<Expr> expression = parser.parseAst(ok);
    if (!ok || !expression) {
        return {false, "", PrimitiveType::Unknown, expressionColumn};
    }

// analyzer analyzes the construct and validates its semantics.
    ExpressionAnalyzer analyzer(inputFile, lineNumber, sourceLines, declaredVariables, declaredFunctions);
    if (!analyzer.analyze(*expression)) {
        return {false, "", PrimitiveType::Unknown, expressionColumn};
    }

    if (!expression->mutableValue) {
        recordSourceError(inputFile, lineNumber, expression->sourceColumn, "assignment target must be a mutable variable or collection element", sourceLines);
        return {false, "", PrimitiveType::Unknown, expression->sourceColumn};
    }

    return {
        true,
        generateMutableAccessExpression(*expression, lineNumber, emitRuntimeChecks, declaredFunctions),
        expression->inferredType,
        expression->sourceColumn
    };
}

std::unique_ptr<Expr> ExpressionParser::parseAst(bool& ok) {
    std::unique_ptr<Expr> expression = parseExpression(ok);
    if (!ok) {
        return nullptr;
    }
    if (!atEnd()) {
        report(peek(), "unexpected token in expression");
        ok = false;
        return nullptr;
    }
    return expression;
}

bool ExpressionParser::atEnd() const {
    return peek().kind == TokenKind::EndOfFile;
}

const Token& ExpressionParser::peek() const {
    return tokens[current];
}

const Token& ExpressionParser::previous() const {
    return tokens[current - 1];
}

bool ExpressionParser::match(TokenKind kind, const std::string& text) {
    if (peek().kind != kind || (!text.empty() && peek().text != text)) {
        return false;
    }
    ++current;
    return true;
}

bool ExpressionParser::check(TokenKind kind, const std::string& text) const {
    return peek().kind == kind && (text.empty() || peek().text == text);
}

bool ExpressionParser::isOperator(const std::string& text) const {
    return check(TokenKind::Operator, text);
}

bool ExpressionParser::isUnterminatedQuotedToken(const Token& token) const {
    if (token.kind != TokenKind::String && token.kind != TokenKind::Char) {
        return false;
    }
    return token.text.size() < 2 || token.text.front() != token.text.back();
}

int ExpressionParser::absoluteColumn(const Token& token) const {
    return expressionColumn + token.span.startColumn - 1;
}

void ExpressionParser::report(const Token& token, const std::string& message) const {
    recordSourceError(inputFile, lineNumber, absoluteColumn(token), message, sourceLines);
}

bool ExpressionParser::reportInputUsageError(const Token& inputToken) const {
    if (!check(TokenKind::LeftParen)) {
        report(inputToken, "input must be called as input()");
        return true;
    }

    const Token& leftParen = peek();
    if (current + 1 >= tokens.size() || tokens[current + 1].kind == TokenKind::EndOfFile) {
        report(leftParen, "unclosed parenthesis in input");
        return true;
    }

    if (tokens[current + 1].kind != TokenKind::RightParen) {
        report(tokens[current + 1], "input() does not take arguments");
        return true;
    }

    report(inputToken, "input() can only be used as the entire value in an assignment or declaration");
    return true;
}

bool ExpressionParser::isTypeName(const std::string& name) const {
    return declaredTypeForName(name) != PrimitiveType::Unknown;
}

std::unique_ptr<Expr> ExpressionParser::parseExpression(bool& ok) { return parseLogicalOr(ok); }
std::unique_ptr<Expr> ExpressionParser::parseLogicalOr(bool& ok) {
    std::unique_ptr<Expr> expression = parseLogicalAnd(ok);
    while (ok && isOperator("||")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseLogicalAnd(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op));
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseLogicalAnd(bool& ok) {
    std::unique_ptr<Expr> expression = parseBitwiseOr(ok);
    while (ok && isOperator("&&")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseBitwiseOr(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op));
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseBitwiseOr(bool& ok) {
    std::unique_ptr<Expr> expression = parseBitwiseXor(ok);
    while (ok && isOperator("|")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseBitwiseXor(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op));
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseBitwiseXor(bool& ok) {
    std::unique_ptr<Expr> expression = parseBitwiseAnd(ok);
    while (ok && isOperator("^")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseBitwiseAnd(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op));
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseBitwiseAnd(bool& ok) {
    std::unique_ptr<Expr> expression = parseEquality(ok);
    while (ok && isOperator("&")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseEquality(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op));
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseEquality(bool& ok) {
    std::unique_ptr<Expr> expression = parseComparison(ok);
    while (ok && (isOperator("==") || isOperator("!="))) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseComparison(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op));
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseComparison(bool& ok) {
    std::unique_ptr<Expr> expression = parseShift(ok);
    while (ok && (isOperator("<") || isOperator("<=") || isOperator(">") || isOperator(">=") || check(TokenKind::Identifier, "in"))) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseShift(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op));
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseShift(bool& ok) {
    std::unique_ptr<Expr> expression = parseAdditive(ok);
    while (ok && (isOperator("<<") || isOperator(">>"))) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseAdditive(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op));
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseAdditive(bool& ok) {
    std::unique_ptr<Expr> expression = parseMultiplicative(ok);
    while (ok && (isOperator("+") || isOperator("-"))) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseMultiplicative(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op));
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseMultiplicative(bool& ok) {
    std::unique_ptr<Expr> expression = parseUnary(ok);
    while (ok && (isOperator("*") || isOperator("/") || isOperator("%"))) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseUnary(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op));
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseUnary(bool& ok) {
    if (isOperator("++") || isOperator("--") || isOperator("+") || isOperator("-") || isOperator("!")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseUnary(ok);
        if (!ok) return nullptr;
        return std::make_unique<UnaryExpr>(op.text, std::move(right), absoluteColumn(op));
    }
    return parsePostfix(ok);
}
std::unique_ptr<Expr> ExpressionParser::parsePostfix(bool& ok) {
    std::unique_ptr<Expr> expression = parsePrimary(ok);
    while (ok && (check(TokenKind::LeftBracket) ||
           (check(TokenKind::Operator, ".") && current + 1 < tokens.size() && tokens[current + 1].kind == TokenKind::Identifier) ||
           isOperator("++") || isOperator("--"))) {
        if (match(TokenKind::LeftBracket)) {
            const Token& leftBracket = previous();
            std::unique_ptr<Expr> start = parseLogicalOr(ok);
            if (!ok) return nullptr;
            if (match(TokenKind::Operator, ":")) {
                std::unique_ptr<Expr> end = parseLogicalOr(ok);
                if (!ok) return nullptr;
                if (!match(TokenKind::RightBracket)) {
                    report(leftBracket, "unclosed bracket in list slice");
                    ok = false;
                    return nullptr;
                }
                expression = std::make_unique<SliceExpr>(std::move(expression), std::move(start), std::move(end), absoluteColumn(leftBracket));
                continue;
            }
            if (!match(TokenKind::RightBracket)) {
                report(leftBracket, "unclosed bracket in list index");
                ok = false;
                return nullptr;
            }
            expression = std::make_unique<IndexExpr>(std::move(expression), std::move(start), absoluteColumn(leftBracket));
            continue;
        }
        if (check(TokenKind::Operator, ".")) {
            expression = parseMethodCall(std::move(expression), ok);
            continue;
        }

        const Token op = peek();
        ++current;
        expression = std::make_unique<UnaryExpr>(op.text, std::move(expression), absoluteColumn(op), true);
    }
    return expression;
}

std::unique_ptr<Expr> ExpressionParser::parseMethodCall(std::unique_ptr<Expr> expression, bool& ok) {
    const Token& dot = peek();
    ++current;
    if (!match(TokenKind::Identifier)) {
        report(dot, "expected method name after '.'");
        ok = false;
        return nullptr;
    }
    const Token& method = previous();
    if (!check(TokenKind::LeftParen)) {
        return std::make_unique<FieldExpr>(std::move(expression), method.text, absoluteColumn(method));
    }
    if (method.text != "remove" && method.text != "find" && method.text != "at" && method.text != "split" &&
        method.text != "prev" && method.text != "next" && method.text != "hasPrev" && method.text != "hasNext") {
        if (match(TokenKind::LeftParen)) {
            const Token& leftParen = previous();
            std::vector<std::unique_ptr<Expr>> arguments;
            if (!match(TokenKind::RightParen)) {
                arguments.push_back(parseExpression(ok));
                while (ok && match(TokenKind::Comma)) arguments.push_back(parseExpression(ok));
                if (ok && !match(TokenKind::RightParen)) {
                    report(leftParen, "unclosed parenthesis in method call");
                    ok = false;
                    return nullptr;
                }
            }
            return std::make_unique<CallExpr>(method.text, std::move(expression), std::move(arguments), absoluteColumn(method));
        }
        report(method, "unknown method '" + method.text + "'");
        ok = false;
        return nullptr;
    }
    if (!match(TokenKind::LeftParen)) {
        report(method, method.text + " must be called with parentheses");
        ok = false;
        return nullptr;
    }
    const Token& leftParen = previous();
    std::vector<std::unique_ptr<Expr>> arguments;
    if (!match(TokenKind::RightParen)) {
        arguments.push_back(parseExpression(ok));
        if (!ok) return nullptr;
        while (match(TokenKind::Comma)) {
            arguments.push_back(parseExpression(ok));
            if (!ok) return nullptr;
        }
        if (!match(TokenKind::RightParen)) {
            if (method.text == "remove") {
                report(leftParen, "remove() expects no arguments or index");
            } else if (method.text == "at") {
                report(leftParen, "at() expects exactly one key");
            } else if (method.text == "prev" || method.text == "next" || method.text == "hasPrev" || method.text == "hasNext") {
                report(leftParen, "unclosed parenthesis in " + method.text);
            } else if (method.text == "split") {
                report(leftParen, "split() expects exactly one delimiter");
            } else {
                report(leftParen, "find() expects exactly one value or sublist");
            }
            ok = false;
            return nullptr;
        }
    }
    if (method.text == "remove" && arguments.size() > 1) {
        report(leftParen, "remove() expects no arguments or index");
        ok = false;
        return nullptr;
    }
    if (method.text == "at" && arguments.size() != 1) {
        report(leftParen, "at() expects exactly one key");
        ok = false;
        return nullptr;
    }
    if ((method.text == "prev" || method.text == "next" || method.text == "hasPrev" || method.text == "hasNext") && arguments.size() != 1) {
        report(leftParen, method.text + "() expects exactly one key");
        ok = false;
        return nullptr;
    }
    if (method.text == "split" && arguments.size() != 1) {
        report(leftParen, "split() expects exactly one delimiter");
        ok = false;
        return nullptr;
    }
    if (method.text == "find" && arguments.size() != 1) {
        report(leftParen, "find() expects exactly one value or sublist");
        ok = false;
        return nullptr;
    }
    return std::make_unique<CallExpr>(method.text, std::move(expression), std::move(arguments), absoluteColumn(method));
}

std::unique_ptr<Expr> ExpressionParser::parseBraceLiteral(bool& ok) {
    const Token& leftBrace = peek();
    ++current;
    if (check(TokenKind::Unknown, "}")) {
        report(leftBrace, "empty set or map literal needs a declared type");
        ok = false;
        return nullptr;
    }

    const size_t contentStart = current;
    int braceDepth = 1;
    while (current < tokens.size()) {
        const Token& token = tokens[current];
        if (token.kind == TokenKind::EndOfFile) {
            break;
        }
        if (token.kind == TokenKind::Unknown && token.text == "{") {
            ++braceDepth;
        } else if (token.kind == TokenKind::Unknown && token.text == "}") {
            --braceDepth;
            if (braceDepth == 0) {
                break;
            }
        }
        ++current;
    }

    if (current >= tokens.size() || tokens[current].kind == TokenKind::EndOfFile) {
        report(leftBrace, "unclosed brace in set or map literal");
        ok = false;
        return nullptr;
    }

    const size_t contentEnd = current;
    ++current;

    auto sliceText = [&](size_t startIndex, size_t endIndex) -> std::string {
        if (startIndex >= endIndex) {
            return "";
        }
        const int startColumn = tokens[startIndex].span.startColumn;
        const int endColumn = tokens[endIndex - 1].span.endColumn;
        return expressionText.substr(
            static_cast<size_t>(startColumn - 1),
            static_cast<size_t>(endColumn - startColumn + 1)
        );
    };

    auto parseSlice = [&](const std::string& text, int column) -> std::unique_ptr<Expr> {
        return parseExpressionAst(
            inputFile,
            lineNumber,
            text,
            column,
            sourceLines,
            declaredVariables,
            declaredFunctions
        );
    };

    struct EntrySlice {
        size_t start = 0;
        size_t end = 0;
        size_t colonIndex = 0;
        bool hasColon = false;
    };

    std::vector<EntrySlice> entries;
    size_t segmentStart = contentStart;
    int parenDepth = 0;
    int bracketDepth = 0;
    int nestedBraceDepth = 0;
    size_t topLevelColon = tokens.size();

    for (size_t index = contentStart; index < contentEnd; ++index) {
        const Token& token = tokens[index];
        if (token.kind == TokenKind::LeftParen) {
            ++parenDepth;
            continue;
        }
        if (token.kind == TokenKind::RightParen) {
            if (parenDepth > 0) {
                --parenDepth;
            }
            continue;
        }
        if (token.kind == TokenKind::LeftBracket) {
            ++bracketDepth;
            continue;
        }
        if (token.kind == TokenKind::RightBracket) {
            if (bracketDepth > 0) {
                --bracketDepth;
            }
            continue;
        }
        if (token.kind == TokenKind::Unknown && token.text == "{") {
            ++nestedBraceDepth;
            continue;
        }
        if (token.kind == TokenKind::Unknown && token.text == "}") {
            if (nestedBraceDepth > 0) {
                --nestedBraceDepth;
            }
            continue;
        }

        if (parenDepth == 0 && bracketDepth == 0 && nestedBraceDepth == 0) {
            if (token.kind == TokenKind::Operator && token.text == ":" && topLevelColon == tokens.size()) {
                topLevelColon = index;
                continue;
            }
            if (token.kind == TokenKind::Comma) {
                if (segmentStart == index) {
                    report(token, "expected expression after ',' in set or map literal");
                    ok = false;
                    return nullptr;
                }
                entries.push_back({segmentStart, index, topLevelColon, topLevelColon != tokens.size()});
                segmentStart = index + 1;
                topLevelColon = tokens.size();
            }
        }
    }

    if (segmentStart >= contentEnd) {
        report(tokens[contentEnd - 1], "expected expression after ',' in set or map literal");
        ok = false;
        return nullptr;
    }
    entries.push_back({segmentStart, contentEnd, topLevelColon, topLevelColon != tokens.size()});

    bool mapLiteral = false;
    bool setLiteral = false;
    for (const EntrySlice& entry : entries) {
        mapLiteral = mapLiteral || entry.hasColon;
        setLiteral = setLiteral || !entry.hasColon;
    }

    if (mapLiteral && setLiteral) {
        report(leftBrace, "map literal entries must use key:value pairs");
        ok = false;
        return nullptr;
    }

    if (!mapLiteral) {
        std::vector<std::unique_ptr<Expr>> elements;
        for (const EntrySlice& entry : entries) {
            const std::string elementText = sliceText(entry.start, entry.end);
            std::unique_ptr<Expr> element = parseSlice(elementText, absoluteColumn(tokens[entry.start]));
            if (!element) {
                ok = false;
                return nullptr;
            }
            elements.push_back(std::move(element));
        }
        return std::make_unique<SetLiteralExpr>(std::move(elements), absoluteColumn(leftBrace));
    }

    std::vector<MapLiteralEntry> mapEntries;
    for (const EntrySlice& entry : entries) {
        if (!entry.hasColon || entry.colonIndex <= entry.start || entry.colonIndex + 1 >= entry.end) {
            report(leftBrace, "map literal entries must use key:value pairs");
            ok = false;
            return nullptr;
        }

        const std::string keyText = sliceText(entry.start, entry.colonIndex);
        const std::string valueText = sliceText(entry.colonIndex + 1, entry.end);
        std::unique_ptr<Expr> key = parseSlice(keyText, absoluteColumn(tokens[entry.start]));
        std::unique_ptr<Expr> value = parseSlice(valueText, absoluteColumn(tokens[entry.colonIndex + 1]));
        if (!key || !value) {
            ok = false;
            return nullptr;
        }
        mapEntries.push_back({std::move(key), std::move(value)});
    }
    return std::make_unique<MapLiteralExpr>(std::move(mapEntries), absoluteColumn(leftBrace));
}

std::unique_ptr<Expr> ExpressionParser::parsePrimary(bool& ok) {
    if (check(TokenKind::Identifier) &&
        peek().text != "range" &&
        isTypeName(peek().text) &&
        !isStructType(declaredTypeForName(peek().text)) &&
        current + 1 < tokens.size() &&
        tokens[current + 1].kind == TokenKind::LeftParen) {
        const Token typeToken = peek();
        const Token leftParen = tokens[current + 1];
        current += 2;
        std::unique_ptr<Expr> operand = parseExpression(ok);
        if (!ok) {
            return nullptr;
        }
        if (!match(TokenKind::RightParen)) {
            report(leftParen, "unclosed parenthesis in cast");
            ok = false;
            return nullptr;
        }
        return std::make_unique<CastExpr>(declaredTypeForName(typeToken.text), std::move(operand), absoluteColumn(typeToken));
    }

    if (match(TokenKind::LeftParen)) {
        const Token leftParen = previous();
        std::unique_ptr<Expr> expression = parseExpression(ok);
        if (!ok) {
            return nullptr;
        }

        if (match(TokenKind::Comma)) {
            std::unique_ptr<Expr> second = parseExpression(ok);
            if (!ok) {
                return nullptr;
            }
            if (!match(TokenKind::RightParen)) {
                report(leftParen, "unclosed parenthesis in pair literal");
                ok = false;
                return nullptr;
            }
            return std::make_unique<PairLiteralExpr>(std::move(expression), std::move(second), absoluteColumn(leftParen));
        }

        if (!match(TokenKind::RightParen)) {
            report(leftParen, "unclosed parenthesis in expression");
            ok = false;
            return nullptr;
        }
        return expression;
    }

    if (match(TokenKind::Identifier)) {
        const Token& identifier = previous();
        if (identifier.text == "true" || identifier.text == "false") {
            return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Bool, identifier.text, absoluteColumn(identifier));
        }
        if (identifier.text == "NULL") {
            return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Null, identifier.text, absoluteColumn(identifier));
        }
        if (identifier.text == "len") {
            if (!match(TokenKind::LeftParen)) {
                report(identifier, "len must be called as len(list)");
                ok = false;
                return nullptr;
            }
            const Token& leftParen = previous();
            std::vector<std::unique_ptr<Expr>> arguments;
            arguments.push_back(parseExpression(ok));
            if (!ok) return nullptr;
            if (!match(TokenKind::RightParen)) {
                report(leftParen, "unclosed parenthesis in len");
                ok = false;
                return nullptr;
            }
            return std::make_unique<CallExpr>("len", nullptr, std::move(arguments), absoluteColumn(identifier));
        }
        if (identifier.text == "split") {
            report(identifier, "split must be called as list.split(delimiter)");
            ok = false;
            return nullptr;
        }
        if (identifier.text == "copy" &&
            check(TokenKind::LeftParen) &&
            current + 2 < tokens.size() &&
            tokens[current + 1].kind == TokenKind::LeftBracket &&
            tokens[current + 2].kind == TokenKind::RightBracket) {
            report(tokens[current + 1], "copy() cannot infer the type of an empty list; declare the list type first");
            ok = false;
            return nullptr;
        }
        if (identifier.text == "min" || identifier.text == "max" || identifier.text == "sum" || identifier.text == "abs" || identifier.text == "range") {
            if (!match(TokenKind::LeftParen)) {
                if (identifier.text == "range") {
                    report(identifier, "range must be called as range(stop), range(start, stop), or range(start, stop, step)");
                } else {
                    report(identifier, identifier.text + " must be called as " + identifier.text + "(list)");
                }
                ok = false;
                return nullptr;
            }
            const Token& leftParen = previous();
            std::vector<std::unique_ptr<Expr>> arguments;
            if (!match(TokenKind::RightParen)) {
                arguments.push_back(parseExpression(ok));
                if (!ok) return nullptr;
                while (match(TokenKind::Comma)) {
                    arguments.push_back(parseExpression(ok));
                    if (!ok) return nullptr;
                }
                if (!match(TokenKind::RightParen)) {
                    report(leftParen, "unclosed parenthesis in " + identifier.text);
                    ok = false;
                    return nullptr;
                }
            }
            return std::make_unique<CallExpr>(identifier.text, nullptr, std::move(arguments), absoluteColumn(identifier));
        }
        if (identifier.text == "input") {
            reportInputUsageError(identifier);
            ok = false;
            return nullptr;
        }
        if (match(TokenKind::LeftParen)) {
            const Token& leftParen = previous();
            std::vector<std::unique_ptr<Expr>> arguments;
            if (!match(TokenKind::RightParen)) {
                arguments.push_back(parseExpression(ok));
                if (!ok) return nullptr;
                while (match(TokenKind::Comma)) {
                    arguments.push_back(parseExpression(ok));
                    if (!ok) return nullptr;
                }
                if (!match(TokenKind::RightParen)) {
                    report(leftParen, "unclosed parenthesis in function call");
                    ok = false;
                    return nullptr;
                }
            }
            return std::make_unique<CallExpr>(identifier.text, nullptr, std::move(arguments), absoluteColumn(identifier));
        }
        return std::make_unique<VariableExpr>(identifier.text, absoluteColumn(identifier));
    }

    if (match(TokenKind::Integer)) {
        const Token& literal = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Int, literal.text, absoluteColumn(literal));
    }
    if (match(TokenKind::Float)) {
        const Token& literal = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Float, literal.text, absoluteColumn(literal));
    }
    if (match(TokenKind::String)) {
        const Token& literal = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::String, literal.text, absoluteColumn(literal));
    }
    if (match(TokenKind::Char)) {
        const Token& literal = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Char, literal.text, absoluteColumn(literal));
    }
    if (match(TokenKind::LeftBracket)) {
        const Token& leftBracket = previous();
        if (match(TokenKind::RightBracket)) {
            report(leftBracket, "empty list literal needs a declared List type");
            ok = false;
            return nullptr;
        }
        std::vector<std::unique_ptr<Expr>> elements;
        elements.push_back(parseExpression(ok));
        if (!ok) return nullptr;
        while (match(TokenKind::Comma)) {
            const Token& comma = previous();
            if (check(TokenKind::RightBracket) || atEnd()) {
                report(comma, "expected expression after ',' in list literal");
                ok = false;
                return nullptr;
            }
            elements.push_back(parseExpression(ok));
            if (!ok) return nullptr;
        }
        if (!match(TokenKind::RightBracket)) {
            report(leftBracket, "unclosed bracket in list literal");
            ok = false;
            return nullptr;
        }
        return std::make_unique<ListLiteralExpr>(std::move(elements), absoluteColumn(leftBracket));
    }
    if (check(TokenKind::Unknown, "{")) {
        return parseBraceLiteral(ok);
    }
    if (match(TokenKind::LeftParen)) {
        const Token& leftParen = previous();
        std::unique_ptr<Expr> expression = parseExpression(ok);
        if (!ok) return nullptr;
        if (!match(TokenKind::RightParen)) {
            report(leftParen, "unclosed parenthesis in expression");
            ok = false;
            return nullptr;
        }
        return expression;
    }

    report(peek(), "expected expression");
    ok = false;
    return nullptr;
}
