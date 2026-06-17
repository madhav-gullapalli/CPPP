#include "expressionParser.h"

#include <memory>

namespace {
std::string cppTypeForExpressionType(const Type& type) {
    switch (type.primitive) {
        case PrimitiveType::Bool:
            return "bool";
        case PrimitiveType::Char:
            return "CPPPChar";
        case PrimitiveType::Int:
            return "long long";
        case PrimitiveType::Float:
            return "long double";
        case PrimitiveType::List:
            if (type.subtypes.size() == 1) {
                return "vector<" + cppTypeForExpressionType(type.subtypes[0]) + ">";
            }
            return "";
        case PrimitiveType::Unknown:
            return "";
    }

    return "";
}

bool isListType(const Type& type) {
    return type.primitive == PrimitiveType::List && type.subtypes.size() == 1;
}

class ExpressionAnalyzer {
public:
    ExpressionAnalyzer(
        const std::string& inputFile,
        int lineNumber,
        const std::map<int, std::string>& sourceLines,
        const std::map<std::string, Type>& declaredVariables
    ) :
        inputFile(inputFile),
        lineNumber(lineNumber),
        sourceLines(sourceLines),
        declaredVariables(declaredVariables) {}

    bool analyze(Expr& expr) {
        if (auto* literal = dynamic_cast<LiteralExpr*>(&expr)) {
            return analyzeLiteral(*literal);
        }
        if (auto* variable = dynamic_cast<VariableExpr*>(&expr)) {
            return analyzeVariable(*variable);
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

        return false;
    }

private:
    const std::string& inputFile;
    int lineNumber;
    const std::map<int, std::string>& sourceLines;
    const std::map<std::string, Type>& declaredVariables;

    void report(int column, const std::string& message) const {
        recordSourceError(inputFile, lineNumber, column, message, sourceLines);
    }

    bool isValueType(Type type) const {
        return type != PrimitiveType::Unknown;
    }

    bool isNumericType(Type type) const {
        return type == PrimitiveType::Bool ||
            type == PrimitiveType::Char ||
            type == PrimitiveType::Int ||
            type == PrimitiveType::Float;
    }

    bool isBitwiseType(Type type) const {
        return type == PrimitiveType::Bool || type == PrimitiveType::Char || type == PrimitiveType::Int;
    }

    bool isIncrementableType(Type type) const {
        return type == PrimitiveType::Char ||
            type == PrimitiveType::Int ||
            type == PrimitiveType::Float;
    }

    bool isLexicographicallyComparable(Type type) const {
        if (isNumericType(type)) {
            return true;
        }

        if (!isListType(type)) {
            return false;
        }

        return isLexicographicallyComparable(type.subtypes[0]);
    }

    bool isComparable(Type left, Type right) const {
        if (!isValueType(left) || !isValueType(right)) {
            return false;
        }

        if (isListType(left) || isListType(right)) {
            return left == right && isLexicographicallyComparable(left);
        }

        return isNumericType(left) && isNumericType(right);
    }

    bool isFloatType(Type type) const {
        return type == PrimitiveType::Float;
    }

    bool isSummableType(Type type) const {
        return type == PrimitiveType::Bool ||
            type == PrimitiveType::Char ||
            type == PrimitiveType::Int ||
            type == PrimitiveType::Float;
    }

    Type sumResultType(Type elementType) const {
        if (elementType == PrimitiveType::Float) {
            return PrimitiveType::Float;
        }
        return PrimitiveType::Int;
    }

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

    bool analyzeLiteral(LiteralExpr& expr) {
        switch (expr.kind) {
            case LiteralExpr::Kind::Bool:
                expr.inferredType = PrimitiveType::Bool;
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

    bool analyzeVariable(VariableExpr& expr) {
        const auto variable = declaredVariables.find(expr.name);
        if (variable == declaredVariables.end()) {
            report(expr.sourceColumn, "use of undeclared variable '" + expr.name + "'");
            return false;
        }

        expr.inferredType = variable->second;
        expr.mutableValue = true;
        return true;
    }

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

    bool analyzeBinary(BinaryExpr& expr) {
        if (!analyze(*expr.left) || !analyze(*expr.right)) {
            return false;
        }

        if (expr.op == "in") {
            if (expr.right->inferredType.primitive != PrimitiveType::List || expr.right->inferredType.subtypes.size() != 1) {
                report(expr.sourceColumn, "right side of 'in' must be a List");
                return false;
            }

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
            if (leftType.primitive == PrimitiveType::List || rightType.primitive == PrimitiveType::List) {
                if (leftType != rightType || !isLexicographicallyComparable(leftType)) {
                    report(expr.sourceColumn, "cannot compare " + cpppTypeName(leftType) + " and " + cpppTypeName(rightType));
                    return false;
                }
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

    bool analyzeCast(CastExpr& expr) {
        if (!analyze(*expr.operand)) {
            return false;
        }

        expr.inferredType = expr.targetType;
        expr.explicitCast = true;
        return true;
    }

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
                report(expr.sourceColumn, "len must be called as len(list)");
                return false;
            }
            if (expr.arguments[0]->inferredType.primitive != PrimitiveType::List || expr.arguments[0]->inferredType.subtypes.size() != 1) {
                report(expr.sourceColumn, "len() expects a List value");
                return false;
            }
            expr.inferredType = PrimitiveType::Int;
            return true;
        }

        if (expr.callee == "remove") {
            if (!expr.receiver ||
                expr.receiver->inferredType.primitive != PrimitiveType::List ||
                expr.receiver->inferredType.subtypes.size() != 1) {
                report(expr.sourceColumn, "remove() can only be used on List values");
                return false;
            }
            if (!expr.receiver->mutableValue) {
                report(expr.sourceColumn, "remove() requires a mutable List variable");
                return false;
            }
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

        if (expr.callee == "min" || expr.callee == "max") {
            if (expr.arguments.size() != 1) {
                report(expr.sourceColumn, expr.callee + " must be called as " + expr.callee + "(list)");
                return false;
            }
            if (expr.arguments[0]->inferredType.primitive != PrimitiveType::List ||
                expr.arguments[0]->inferredType.subtypes.size() != 1) {
                report(expr.sourceColumn, expr.callee + "() expects a List value");
                return false;
            }
            expr.inferredType = expr.arguments[0]->inferredType.subtypes[0];
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

        report(expr.sourceColumn, "unexpected token in expression");
        return false;
    }

    bool analyzeIndex(IndexExpr& expr) {
        if (!analyze(*expr.base) || !analyze(*expr.index)) {
            return false;
        }

        if (expr.base->inferredType.primitive != PrimitiveType::List || expr.base->inferredType.subtypes.size() != 1) {
            report(expr.sourceColumn, "indexing requires a List value");
            return false;
        }

        if (expr.index->inferredType != PrimitiveType::Int) {
            report(expr.sourceColumn, "list index must be int");
            return false;
        }

        expr.inferredType = expr.base->inferredType.subtypes[0];
        return true;
    }

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
};

class ExpressionCodegen {
public:
    ExpressionCodegen(int lineNumber, bool emitRuntimeChecks) :
        lineNumber(lineNumber),
        emitRuntimeChecks(emitRuntimeChecks) {}

    std::string generate(const Expr& expr) const {
        if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expr)) {
            return generateLiteral(*literal);
        }
        if (const auto* variable = dynamic_cast<const VariableExpr*>(&expr)) {
            return variable->name;
        }
        if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
            return generateUnary(*unary);
        }
        if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
            return generateBinary(*binary);
        }
        if (const auto* cast = dynamic_cast<const CastExpr*>(&expr)) {
            return castExpressionTo(generate(*cast->operand), cast->operand->inferredType, cast->targetType);
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

        return "";
    }

private:
    int lineNumber;
    bool emitRuntimeChecks;

    std::string runtimeErrorThrowExpression(int column, const std::string& message) const {
        return "throw runtime_error(\"" + std::to_string(lineNumber) + ":" + std::to_string(column) + ":" + message + "\")";
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

    std::string concatenatedListExpression(const BinaryExpr& expr) const {
        const std::string left = generate(*expr.left);
        const std::string right = generate(*expr.right);
        return "([&]() { auto __cppp_left = " + left + "; auto __cppp_right = " + right +
            "; __cppp_left.insert(__cppp_left.end(), __cppp_right.begin(), __cppp_right.end()); return __cppp_left; }())";
    }

    std::string generateLiteral(const LiteralExpr& expr) const {
        switch (expr.kind) {
            case LiteralExpr::Kind::Bool:
            case LiteralExpr::Kind::Int:
            case LiteralExpr::Kind::Float:
                return expr.text;
            case LiteralExpr::Kind::String:
                return "CPPPStringLiteral(" + expr.text + ")";
            case LiteralExpr::Kind::Char:
                return "CPPPChar(" + expr.text + ")";
        }
        return expr.text;
    }

    std::string generateUnary(const UnaryExpr& expr) const {
        const std::string operand = generate(*expr.operand);
        if (expr.op == "++" || expr.op == "--") {
            return expr.postfix ? "(" + operand + expr.op + ")" : "(" + expr.op + operand + ")";
        }
        if (expr.op == "!") {
            return "(!" + castExpressionTo(operand, expr.operand->inferredType, PrimitiveType::Bool) + ")";
        }
        return "(" + expr.op + operand + ")";
    }

    std::string generateBinary(const BinaryExpr& expr) const {
        const std::string left = generate(*expr.left);
        const std::string right = generate(*expr.right);

        if (expr.op == "in") {
            if (isListType(expr.left->inferredType)) {
                if (!isListType(expr.right->inferredType)) {
                    return "false";
                }
                const Type elementType = expr.right->inferredType.subtypes[0];
                if (expr.left->inferredType == elementType) {
                    return "([&]() { const auto& __cppp_list = " + right + "; return find(__cppp_list.begin(), __cppp_list.end(), " + left + ") != __cppp_list.end(); }())";
                }
                if (expr.left->inferredType != expr.right->inferredType) {
                    return "false";
                }
                return "CPPPListContainsSublist(" + right + ", " + left + ")";
            }
            const Type elementType = expr.right->inferredType.subtypes[0];
            std::string needle = left;
            if (!isImplicitlyConvertible(expr.left->inferredType, elementType) || expr.left->inferredType != elementType) {
                needle = castExpressionTo(needle, expr.left->inferredType, elementType);
            }
            return "([&]() { const auto& __cppp_list = " + right + "; return find(__cppp_list.begin(), __cppp_list.end(), " + needle + ") != __cppp_list.end(); }())";
        }

        if (expr.op == "||" || expr.op == "&&") {
            return "(" + castExpressionTo(left, expr.left->inferredType, PrimitiveType::Bool) + " " + expr.op + " " + castExpressionTo(right, expr.right->inferredType, PrimitiveType::Bool) + ")";
        }

        if (emitRuntimeChecks && expr.inferredType == PrimitiveType::Int) {
            return checkedIntegerExpression(left, right, expr.op, expr.sourceColumn);
        }

        if (expr.op == "+" && isListType(expr.inferredType)) {
            return concatenatedListExpression(expr);
        }

        return "(" + left + " " + expr.op + " " + right + ")";
    }

    std::string generateCall(const CallExpr& expr) const {
        if (expr.callee == "len") {
            return "static_cast<long long>((" + generate(*expr.arguments[0]) + ").size())";
        }

        if (expr.callee == "remove") {
            const std::string receiver = generate(*expr.receiver);
            if (expr.arguments.empty()) {
                if (emitRuntimeChecks) {
                    return "CPPPListPop(" + receiver + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                }
                return "([&]() { auto __cppp_removed = (" + receiver + ").back(); (" + receiver + ").pop_back(); return __cppp_removed; }())";
            }

            std::string index = generate(*expr.arguments[0]);
            if (!isImplicitlyConvertible(expr.arguments[0]->inferredType, PrimitiveType::Int) || expr.arguments[0]->inferredType != PrimitiveType::Int) {
                index = castExpressionTo(index, expr.arguments[0]->inferredType, PrimitiveType::Int);
            }
            if (emitRuntimeChecks) {
                return "CPPPListRemoveAt(" + receiver + ", " + index + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
            return "([&]() { auto __cppp_removed = (" + receiver + ")[" + index + "]; (" + receiver + ").erase((" + receiver + ").begin() + " + index + "); return __cppp_removed; }())";
        }

        if (expr.callee == "find") {
            const std::string receiver = generate(*expr.receiver);
            const Type haystackType = expr.receiver->inferredType;
            const Type elementType = haystackType.subtypes[0];
            const Type needleType = expr.arguments[0]->inferredType;
            if (isListType(needleType) && needleType == haystackType) {
                return "CPPPListFindSublist(" + receiver + ", " + generate(*expr.arguments[0]) + ")";
            }

            std::string needle = generate(*expr.arguments[0]);
            if (!isImplicitlyConvertible(needleType, elementType) || needleType != elementType) {
                needle = castExpressionTo(needle, needleType, elementType);
            }
            return "CPPPListFindValue(" + receiver + ", " + needle + ")";
        }

        if (expr.callee == "min") {
            const std::string list = generate(*expr.arguments[0]);
            if (emitRuntimeChecks) {
                return "CPPPListMin(" + list + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
            return "([&]() { auto __cppp_list = " + list + "; return *min_element(__cppp_list.begin(), __cppp_list.end()); }())";
        }

        if (expr.callee == "max") {
            const std::string list = generate(*expr.arguments[0]);
            if (emitRuntimeChecks) {
                return "CPPPListMax(" + list + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
            return "([&]() { auto __cppp_list = " + list + "; return *max_element(__cppp_list.begin(), __cppp_list.end()); }())";
        }

        if (expr.callee == "sum") {
            const std::string list = generate(*expr.arguments[0]);
            const Type elementType = expr.arguments[0]->inferredType.subtypes[0];
            const std::string initial = elementType == PrimitiveType::Float ? "0.0L" : "0LL";
            return "([&]() { auto __cppp_list = " + list + "; return accumulate(__cppp_list.begin(), __cppp_list.end(), " + initial + "); }())";
        }

        return "";
    }

    std::string generateIndex(const IndexExpr& expr) const {
        const std::string base = generate(*expr.base);
        const std::string index = generate(*expr.index);
        if (emitRuntimeChecks) {
            return "CPPPListAt(" + base + ", " + index + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
        }
        return "([&]() { const auto& __cppp_list = " + base + "; auto __cppp_index = static_cast<long long>(" + index + "); if (__cppp_index < 0) __cppp_index += static_cast<long long>(__cppp_list.size()); return (__cppp_list[__cppp_index]); }())";
    }

    std::string generateSlice(const SliceExpr& expr) const {
        const std::string base = generate(*expr.base);
        const std::string start = generate(*expr.start);
        const std::string end = generate(*expr.end);
        if (emitRuntimeChecks) {
            return "CPPPListSlice(" + base + ", " + start + ", " + end + ")";
        }
        return "([&]() { const auto& __cppp_list = " + base + "; long long __cppp_start = static_cast<long long>(" + start + "); long long __cppp_end = static_cast<long long>(" + end + "); long long __cppp_size = static_cast<long long>(__cppp_list.size()); if (__cppp_start < 0) __cppp_start += __cppp_size; if (__cppp_end < 0) __cppp_end += __cppp_size; __cppp_start = max(0LL, min(__cppp_start, __cppp_size)); __cppp_end = max(0LL, min(__cppp_end, __cppp_size)); if (__cppp_start >= __cppp_end) return vector<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">{}; return vector<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">(__cppp_list.begin() + static_cast<vector<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">::difference_type>(__cppp_start), __cppp_list.begin() + static_cast<vector<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">::difference_type>(__cppp_end)); }())";
    }

    std::string generateListLiteral(const ListLiteralExpr& expr) const {
        const Type elementType = expr.inferredType.subtypes[0];
        std::string generated = "vector<" + cppTypeForExpressionType(elementType) + ">{";
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
};
}

ExpressionParser::ExpressionParser(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks
) :
    inputFile(inputFile),
    lineNumber(lineNumber),
    expressionText(expressionText),
    expressionColumn(expressionColumn),
    sourceLines(sourceLines),
    declaredVariables(declaredVariables),
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

    ExpressionAnalyzer analyzer(inputFile, lineNumber, sourceLines, declaredVariables);
    if (!analyzer.analyze(*expression)) {
        return {false, "", PrimitiveType::Unknown, false, {}};
    }

    ExpressionCodegen codegen(lineNumber, emitRuntimeChecks);
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
            std::unique_ptr<Expr> start = parseExpression(ok);
            if (!ok) return nullptr;
            if (match(TokenKind::Operator, ":")) {
                std::unique_ptr<Expr> end = parseExpression(ok);
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
            expression = parseListMethodCall(std::move(expression), ok);
            continue;
        }

        const Token op = peek();
        ++current;
        expression = std::make_unique<UnaryExpr>(op.text, std::move(expression), absoluteColumn(op), true);
    }
    return expression;
}

std::unique_ptr<Expr> ExpressionParser::parseListMethodCall(std::unique_ptr<Expr> expression, bool& ok) {
    const Token& dot = peek();
    ++current;
    if (!match(TokenKind::Identifier)) {
        report(dot, "expected method name after '.'");
        ok = false;
        return nullptr;
    }
    const Token& method = previous();
    if (method.text != "remove" && method.text != "find") {
        report(method, "unexpected token in expression");
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
    if (method.text == "find" && arguments.size() != 1) {
        report(leftParen, "find() expects exactly one value or sublist");
        ok = false;
        return nullptr;
    }
    return std::make_unique<CallExpr>(method.text, std::move(expression), std::move(arguments), absoluteColumn(method));
}

std::unique_ptr<Expr> ExpressionParser::parsePrimary(bool& ok) {
    if (check(TokenKind::LeftParen) &&
        current + 2 < tokens.size() &&
        tokens[current + 1].kind == TokenKind::Identifier &&
        isTypeName(tokens[current + 1].text) &&
        tokens[current + 2].kind == TokenKind::RightParen) {
        ++current;
        const Token typeToken = peek();
        ++current;
        ++current;
        std::unique_ptr<Expr> operand = parseUnary(ok);
        if (!ok) return nullptr;
        return std::make_unique<CastExpr>(declaredTypeForName(typeToken.text), std::move(operand), absoluteColumn(typeToken));
    }

    if (match(TokenKind::Identifier)) {
        const Token& identifier = previous();
        if (identifier.text == "true" || identifier.text == "false") {
            return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Bool, identifier.text, absoluteColumn(identifier));
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
        if (identifier.text == "min" || identifier.text == "max" || identifier.text == "sum") {
            if (!match(TokenKind::LeftParen)) {
                report(identifier, identifier.text + " must be called as " + identifier.text + "(list)");
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
