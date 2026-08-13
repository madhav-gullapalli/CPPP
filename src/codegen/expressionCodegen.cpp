/*
 * expressionCodegen.cpp
 *
 * Generates C++ from semantically analyzed expression AST nodes.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "expressionCodegen.h"
#include "expressionAnalyzer.h"
#include "expressionParser.h" // compatibility adapter for token-based lvalue emission
#include "typesCppp.h"

#include <algorithm>
#include <climits>
#include <memory>
// cppTypeForExpressionType implements the cppTypeForExpressionType behavior for the expressionCodegen.cpp module.
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
        case PrimitiveType::Heap:
            return type.subtypes.size() == 1
                ? "CPPPHeap<" + cppTypeForExpressionType(type.subtypes[0]) + ">"
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
        case PrimitiveType::Function:
            return cppTypeForType(type);
        case PrimitiveType::Struct:
            return type.name;
        case PrimitiveType::Class:
            return "cppp_smart_pointer<" + type.name + ">";
        case PrimitiveType::Unknown:
            return "";
    }

    return "";
}

// ExpressionCodegen holds state or behavior used by the expressionCodegen.cpp implementation.
class ExpressionCodegen {
public:
    ExpressionCodegen(int lineNumber, bool emitRuntimeChecks, const std::map<std::string, FunctionSignature>& declaredFunctions) :
        lineNumber(lineNumber),
        emitRuntimeChecks(emitRuntimeChecks),
        declaredFunctions(declaredFunctions) {}

// generate implements the generate behavior for the expressionCodegen.cpp module.
    std::string generate(const Expr& expr) const {
        if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expr)) {
            return generateLiteral(*literal);
        }
        if (const auto* variable = dynamic_cast<const VariableExpr*>(&expr)) {
            return generateVariable(*variable);
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

    std::string generateVariable(const VariableExpr& expr) const {
        if (expr.name == "self") return "(*this)";
        if (expr.name == "sum" && isFunctionType(expr.inferredType)) {
            requireRuntimeHelper("CPPPFunctionType");
            requireContainerMember(expr.inferredType.subtypes[1], "begin_mut");
            requireContainerMember(expr.inferredType.subtypes[1], "end_mut");
            return cppTypeForType(expr.inferredType) +
                "([](CPPPList<long long> values) { return accumulate(values.begin(), values.end(), 0LL); })";
        }
        if ((expr.name == "min" || expr.name == "max") && isFunctionType(expr.inferredType)) {
            requireRuntimeHelper("CPPPFunctionType");
            return cppTypeForType(expr.inferredType) +
                "([](long long a, long long b, long long c, long long d) { return " + expr.name +
                "(" + expr.name + "(a, b), " + expr.name + "(c, d)); })";
        }
        if (expr.name == "abs" && isFunctionType(expr.inferredType)) {
            requireRuntimeHelper("CPPPFunctionType");
            return cppTypeForType(expr.inferredType) +
                "([](long long value) { return abs(value); })";
        }
        return expr.name;
    }

// runtimeErrorThrowExpression provides runtime support for generated code.
    std::string runtimeErrorThrowExpression(int column, const std::string& message) const {
        return "throw runtime_error(\"" + std::to_string(lineNumber) + ":" + std::to_string(column) + ":" + message + "\")";
    }

    std::string generateCast(const CastExpr& expr) const {
        const std::string operand = generate(*expr.operand);
        if (isHeapType(expr.targetType) && isFunctionType(expr.operand->inferredType)) {
            requireContainerMember(expr.targetType, "ctor_default");
            return cppTypeForType(expr.targetType) + "(" + operand + ")";
        }
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

// concatenatedListExpression implements the concatenatedListExpression behavior for the expressionCodegen.cpp module.
    std::string concatenatedListExpression(const BinaryExpr& expr) const {
        const std::string left = generate(*expr.left);
        const std::string right = generate(*expr.right);
        const std::string elementType = cppTypeForExpressionType(expr.inferredType.subtypes[0]);
        requireContainerMember(expr.inferredType, "begin");
        requireContainerMember(expr.inferredType, "end");
        requireContainerMember(expr.inferredType, "insert_range");
        requireContainerMember(expr.inferredType, "ctor_iterator");
        return "([&]() { const auto& __cppp_left_source = " + left + "; CPPPList<" + elementType + "> __cppp_left(__cppp_left_source.begin(), __cppp_left_source.end()); auto __cppp_right = " + right +
            "; __cppp_left.insert(__cppp_left.end(), __cppp_right.begin(), __cppp_right.end()); return __cppp_left; }())";
    }

// generateLiteral implements the generateLiteral behavior for the expressionCodegen.cpp module.
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

// generateUnary implements the generateUnary behavior for the expressionCodegen.cpp module.
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

// generateBinary implements the generateBinary behavior for the expressionCodegen.cpp module.
    std::string generateBinary(const BinaryExpr& expr) const {
        const std::string left = generate(*expr.left);
        const std::string right = generate(*expr.right);

        if ((isCollectionType(expr.left->inferredType) || isPairType(expr.left->inferredType)) &&
            (expr.op == "==" || expr.op == "!=" || expr.op == "<" || expr.op == ">" || expr.op == "<=" || expr.op == ">=")) {
            const std::map<std::string, std::string> comparisonMembers = {
                {"==", "compare_eq"}, {"!=", "compare_ne"}, {"<", "compare_lt"},
                {">", "compare_gt"}, {"<=", "compare_le"}, {">=", "compare_ge"}
            };
            requireContainerMember(expr.left->inferredType, comparisonMembers.at(expr.op));
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
                requireContainerMember(expr.right->inferredType, "begin_const");
                requireContainerMember(expr.right->inferredType, "end_const");
                const Type elementType = expr.right->inferredType.subtypes[0];
                if (expr.left->inferredType == elementType) {
                    if (isCollectionType(elementType) || isPairType(elementType)) {
                        requireContainerMember(elementType, "compare_eq");
                    }
                    return "([&]() { const auto& __cppp_list = " + right + "; return find(__cppp_list.begin(), __cppp_list.end(), " + left + ") != __cppp_list.end(); }())";
                }
                if (expr.left->inferredType != expr.right->inferredType) {
                    return "false";
                }
                requireRuntimeHelper("CPPPListContainsSublist");
                return "CPPPListContainsSublist(" + right + ", " + left + ")";
            }
            const Type elementType = expr.right->inferredType.subtypes[0];
            if (isListType(expr.right->inferredType) &&
                (isCollectionType(elementType) || isPairType(elementType))) {
                requireContainerMember(elementType, "compare_eq");
            }
            std::string needle = left;
            if (!isImplicitlyConvertible(expr.left->inferredType, elementType) || expr.left->inferredType != elementType) {
                needle = castExpressionTo(needle, expr.left->inferredType, elementType);
            }
            if (isListType(expr.right->inferredType)) {
                requireContainerMember(expr.right->inferredType, "begin_const");
                requireContainerMember(expr.right->inferredType, "end_const");
                return "([&]() { const auto& __cppp_list = " + right + "; return find(__cppp_list.begin(), __cppp_list.end(), " + needle + ") != __cppp_list.end(); }())";
            }
            if (isSetType(expr.right->inferredType)) {
                requireContainerMember(expr.right->inferredType, "find_const");
                requireContainerMember(expr.right->inferredType, "end_const");
                return "([&]() { const auto& __cppp_set = " + right + "; return __cppp_set.find(" + needle + ") != __cppp_set.end(); }())";
            }

            requireContainerMember(expr.right->inferredType, "find_const");
            requireContainerMember(expr.right->inferredType, "end_const");
            return "([&]() { const auto& __cppp_map = " + right + "; return __cppp_map.find(" + needle + ") != __cppp_map.end(); }())";
        }

        if (expr.op == "||" || expr.op == "&&") {
            return "(" + castExpressionTo(left, expr.left->inferredType, PrimitiveType::Bool) + " " + expr.op + " " + castExpressionTo(right, expr.right->inferredType, PrimitiveType::Bool) + ")";
        }

        const auto* leftLiteral = dynamic_cast<const LiteralExpr*>(expr.left.get());
        const auto* rightLiteral = dynamic_cast<const LiteralExpr*>(expr.right.get());
        const bool leftNull = leftLiteral != nullptr && leftLiteral->kind == LiteralExpr::Kind::Null;
        const bool rightNull = rightLiteral != nullptr && rightLiteral->kind == LiteralExpr::Kind::Null;
        if ((expr.op == "==" || expr.op == "!=") && (leftNull || rightNull)) {
            if (leftNull && rightNull) return expr.op == "==" ? "true" : "false";
            const bool classHandle =
                (leftNull && isClassType(expr.right->inferredType)) ||
                (rightNull && isClassType(expr.left->inferredType));
            if (classHandle) {
                const std::string& handle = leftNull ? right : left;
                const std::string equal = "(" + handle + " == nullptr)";
                return expr.op == "==" ? equal : "(!" + equal + ")";
            }
            const std::string& value = leftNull ? right : left;
            return "([&]() { (void)(" + value + "); return " +
                (expr.op == "==" ? "false" : "true") + "; }())";
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

// generateCall implements the generateCall behavior for the expressionCodegen.cpp module.
    std::string generateCall(const CallExpr& expr) const {
        if (expr.receiver && isStructType(expr.receiver->inferredType)) {
            const std::map<std::string, Type>* fields = declaredStructFieldsForName(expr.receiver->inferredType.name);
            const auto field = fields == nullptr ? std::map<std::string, Type>::const_iterator{} : fields->find(expr.callee);
            if (fields != nullptr && field != fields->end() && isFunctionType(field->second)) {
                const std::string receiver = generate(*expr.receiver);
                std::string call = "(" + receiver + ")" + (isClassType(expr.receiver->inferredType) ? "->" : ".") + expr.callee + "(";
                for (size_t i = 0; i < expr.arguments.size(); ++i) {
                    if (i > 0) call += ", ";
                    std::string argument = generate(*expr.arguments[i]);
                    if (i < field->second.functionParameterCopy.size() &&
                        field->second.functionParameterCopy[i]) {
                        requireCopyHelpersForType(field->second.subtypes[i + 1]);
                        argument = "CPPPCopy(" + argument + ")";
                    }
                    call += argument;
                }
                return call + ")";
            }
        }
        if (!expr.receiver && expr.partialApplication) {
            requireRuntimeHelper("CPPPFunctionType");
            const auto declaredFunction = declaredFunctions.find(expr.callee);
            std::string callable = expr.callee;
            if (declaredFunction != declaredFunctions.end()) {
                const FunctionSignature& signature = declaredFunction->second;
                callable = "static_cast<" +
                    (signature.returnsVoid ? "void" : cppTypeForType(signature.returnType)) +
                    " (*) (";
                for (size_t i = 0; i < signature.parameters.size(); ++i) {
                    if (i > 0) callable += ", ";
                    callable += cppTypeForType(signature.parameters[i].type);
                }
                callable += ")>(&" + signature.name + ")";
            }
            std::string generated = "([&]() { auto __cppp_callable = " + callable + ";";
            std::vector<std::string> stateTypes = {"decltype(__cppp_callable)"};
            std::vector<std::string> stateValues = {"__cppp_callable"};
            for (size_t i = 0; i < expr.arguments.size(); ++i) {
                // CPPPPartialClosureState compares every captured value, even
                // when the program does not explicitly compare function values.
                if (isCollectionType(expr.arguments[i]->inferredType) ||
                    isPairType(expr.arguments[i]->inferredType)) {
                    requireContainerMember(expr.arguments[i]->inferredType, "compare_eq");
                }
                std::string bound = generate(*expr.arguments[i]);
                const bool copyBound =
                    (isFunctionType(expr.functionType) && i < expr.functionType.functionParameterCopy.size() && expr.functionType.functionParameterCopy[i]) ||
                    (declaredFunction != declaredFunctions.end() && declaredFunction->second.parameters[i].copyParameter);
                if (copyBound) {
                    const Type& parameterType = declaredFunction != declaredFunctions.end()
                        ? declaredFunction->second.parameters[i].type
                        : expr.functionType.subtypes[i + 1];
                    requireCopyHelpersForType(parameterType);
                    bound = "CPPPCopy(" + bound + ")";
                }
                const std::string boundName = "__cppp_bound" + std::to_string(i);
                generated += " auto " + boundName + " = " + bound + ";";
                stateTypes.push_back("decltype(" + boundName + ")");
                stateValues.push_back(boundName);
            }
            generated += " return " + cppTypeForType(expr.inferredType) + "([__cppp_callable";
            for (size_t i = 0; i < expr.arguments.size(); ++i) generated += ", __cppp_bound" + std::to_string(i);
            generated += "](";
            for (size_t i = 1; i < expr.inferredType.subtypes.size(); ++i) {
                if (i > 1) generated += ", ";
                const Type& parameterType = expr.inferredType.subtypes[i];
                generated += cppTypeForType(parameterType);
                generated += " __cppp_arg" + std::to_string(i - 1);
            }
            generated += ") { return __cppp_callable(";
            for (size_t i = 0; i < expr.arguments.size(); ++i) {
                if (i > 0) generated += ", ";
                generated += "__cppp_bound" + std::to_string(i);
            }
            for (size_t i = 1; i < expr.inferredType.subtypes.size(); ++i) {
                if (!expr.arguments.empty() || i > 1) generated += ", ";
                generated += "__cppp_arg" + std::to_string(i - 1);
            }
            generated += "); }, new CPPPPartialClosureState<";
            for (size_t i = 0; i < stateTypes.size(); ++i) {
                if (i > 0) generated += ", ";
                generated += stateTypes[i];
            }
            generated += ">(";
            for (size_t i = 0; i < stateValues.size(); ++i) {
                if (i > 0) generated += ", ";
                generated += stateValues[i];
            }
            return generated + ")); }())";
        }
        if (expr.receiver && isStructType(expr.receiver->inferredType)) {
            // Methods may mutate their receiver. Indexed class values must
            // therefore use mutable list access instead of the const read path.
            const std::string receiver = generateMutableAccess(*expr.receiver);
            requireStructMethod(expr.receiver->inferredType.name, expr.callee);
            const FunctionSignature* method = declaredStructMethodForType(expr.receiver->inferredType, expr.callee);
            const bool isClass = isClassType(expr.receiver->inferredType);
            std::string call = "(" + receiver + ")" + (isClass ? "->" : ".") + expr.callee + "(";
            for (size_t i = 0; i < expr.arguments.size(); ++i) {
                if (i > 0) call += ", ";
                std::string argument = generate(*expr.arguments[i]);
                if (method != nullptr && method->parameters[i].copyParameter) {
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
            requireContainerMember(expr.arguments[0]->inferredType, "size");
            return "static_cast<long long>((" + generate(*expr.arguments[0]) + ").size())";
        }

        if (expr.callee == "copy") {
            requireCopyHelpersForType(expr.arguments[0]->inferredType);
            return "CPPPCopy(" + generate(*expr.arguments[0]) + ")";
        }

        if (expr.receiver && isHeapType(expr.receiver->inferredType)) {
            const std::string receiver = generate(*expr.receiver);
            const Type heapType = expr.receiver->inferredType;
            if (expr.callee == "top" || expr.callee == "pop") {
                const std::string method = expr.callee == "pop" ? "pop_value" : "top";
                requireContainerMember(heapType, method);
                return "(" + receiver + ")." + method + "(" + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
            std::string value = generate(*expr.arguments[0]);
            if (expr.arguments[0]->inferredType != heapType.subtypes[0]) {
                value = castExpressionTo(value, expr.arguments[0]->inferredType, heapType.subtypes[0]);
            }
            requireContainerMember(heapType, "push");
            return "(" + receiver + ").push(" + value + ")";
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
                requireContainerMember(receiverType, method);
                return "(" + receiver + ")." + method + "(" + std::to_string(lineNumber) + ", " +
                    std::to_string(expr.sourceColumn) + ")";
            }
            if (expr.callee == "addFront" || expr.callee == "addBack") {
                std::string value = generate(*expr.arguments[0]);
                const Type elementType = receiverType.subtypes[0];
                if (expr.arguments[0]->inferredType != elementType) {
                    value = castExpressionTo(value, expr.arguments[0]->inferredType, elementType);
                }
                requireContainerMember(receiverType, expr.callee == "addFront" ? "push_front" : "push_back");
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
                requireContainerMember(expr.receiver->inferredType, "back");
                requireContainerMember(expr.receiver->inferredType, "pop_back");
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
                requireContainerMember(expr.receiver->inferredType, "index_mut");
                requireContainerMember(expr.receiver->inferredType, "erase_one");
                requireContainerMember(expr.receiver->inferredType, "begin_mut");
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
            requireContainerMember(expr.receiver->inferredType, "at_const");
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
                    requireContainerMember(expr.arguments[0]->inferredType, "begin_mut");
                    return "(*(" + list + ").begin())";
                }
                if (isMapType(expr.arguments[0]->inferredType)) {
                    if (emitRuntimeChecks) {
                        requireRuntimeHelper("CPPPMapMin");
                        return "CPPPMapMin(" + list + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                    }
                    requireContainerMember(expr.arguments[0]->inferredType, "begin_mut");
                    return "((" + list + ").begin()->first)";
                }
                requireContainerMember(expr.arguments[0]->inferredType, "begin_mut");
                requireContainerMember(expr.arguments[0]->inferredType, "end_mut");
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
                    requireContainerMember(expr.arguments[0]->inferredType, "rbegin_mut");
                    return "(*(" + list + ").rbegin())";
                }
                if (isMapType(expr.arguments[0]->inferredType)) {
                    if (emitRuntimeChecks) {
                        requireRuntimeHelper("CPPPMapMax");
                        return "CPPPMapMax(" + list + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
                    }
                    requireContainerMember(expr.arguments[0]->inferredType, "rbegin_mut");
                    return "((" + list + ").rbegin()->first)";
                }
                requireContainerMember(expr.arguments[0]->inferredType, "begin_mut");
                requireContainerMember(expr.arguments[0]->inferredType, "end_mut");
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
            requireContainerMember(expr.arguments[0]->inferredType, "begin_mut");
            requireContainerMember(expr.arguments[0]->inferredType, "end_mut");
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
                if (function->second.parameters[i].copyParameter) {
                    requireCopyHelpersForType(function->second.parameters[i].type);
                    argument = "CPPPCopy(" + argument + ")";
                }
            } else if (isFunctionType(expr.functionType) &&
                       i < expr.functionType.functionParameterCopy.size() &&
                       expr.functionType.functionParameterCopy[i]) {
                requireCopyHelpersForType(expr.functionType.subtypes[i + 1]);
                argument = "CPPPCopy(" + argument + ")";
            }
            generated += argument;
        }
        generated += ")";
        return generated;

        return "";
    }

// generateIndex implements the generateIndex behavior for the expressionCodegen.cpp module.
    std::string generateIndex(const IndexExpr& expr) const {
        const std::string base = generate(*expr.base);
        std::string index = generate(*expr.index);
        if (isListType(expr.base->inferredType)) {
            requireContainerMember(expr.base->inferredType, "index_const");
            requireContainerMember(expr.base->inferredType, "size");
            if (emitRuntimeChecks) {
                requireRuntimeHelper("CPPPListAt");
                return "CPPPListAt(" + base + ", " + index + ", " + std::to_string(lineNumber) + ", " + std::to_string(expr.sourceColumn) + ")";
            }
            return "([&]() { const auto& __cppp_list = " + base + "; auto __cppp_index = static_cast<long long>(" + index + "); if (__cppp_index < 0) __cppp_index += static_cast<long long>(__cppp_list.size()); return (__cppp_list[__cppp_index]); }())";
        }
        if (isPairType(expr.base->inferredType)) {
            const auto* literal = dynamic_cast<const LiteralExpr*>(expr.index.get());
            if (literal != nullptr) {
                requireContainerMember(expr.base->inferredType, literal->text == "0" ? "first_const" : "second_const");
                return "((" + base + ")." + (literal->text == "0" ? "first()" : "second()") + ")";
            }
            requireContainerMember(expr.base->inferredType, "first_const");
            requireContainerMember(expr.base->inferredType, "second_const");
            return "([&]() -> decltype(auto) { const auto& __cppp_pair = " + base +
                "; long long __cppp_index = static_cast<long long>(" + index +
                "); if (__cppp_index != 0 && __cppp_index != 1) { " +
                runtimeErrorThrowExpression(expr.sourceColumn, "Pair index must be 0 or 1") +
                "; } return __cppp_index == 0 ? __cppp_pair.first() : __cppp_pair.second(); }())";
        }
        const Type keyType = expr.base->inferredType.subtypes[0];
        requireContainerMember(expr.base->inferredType, "index_mut");
        if (!isImplicitlyConvertible(expr.index->inferredType, keyType) || expr.index->inferredType != keyType) {
            index = castExpressionTo(index, expr.index->inferredType, keyType);
        }
        return "((" + base + ")[" + index + "])";
    }

// generateMutableAccess implements the generateMutableAccess behavior for the expressionCodegen.cpp module.
    std::string generateMutableAccess(const Expr& expr) const {
        if (const auto* variable = dynamic_cast<const VariableExpr*>(&expr)) {
            return variable->name;
        }
        if (const auto* index = dynamic_cast<const IndexExpr*>(&expr)) {
            const std::string base = generateMutableAccess(*index->base);
            std::string generatedIndex = generate(*index->index);
            if (isListType(index->base->inferredType)) {
                requireContainerMember(index->base->inferredType, "index_mut");
                requireContainerMember(index->base->inferredType, "size");
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
                if (literal != nullptr) {
                    requireContainerMember(index->base->inferredType, literal->text == "0" ? "first_mut" : "second_mut");
                    return "((" + base + ")." + (literal->text == "0" ? "first()" : "second()") + ")";
                }
                requireContainerMember(index->base->inferredType, "first_mut");
                requireContainerMember(index->base->inferredType, "second_mut");
                return "([&]() -> decltype(auto) { auto& __cppp_pair = " + base +
                    "; long long __cppp_index = static_cast<long long>(" + generatedIndex +
                    "); if (__cppp_index != 0 && __cppp_index != 1) { " +
                    runtimeErrorThrowExpression(index->sourceColumn, "Pair index must be 0 or 1") +
                    "; } return __cppp_index == 0 ? __cppp_pair.first() : __cppp_pair.second(); }())";
            }
            const Type keyType = index->base->inferredType.subtypes[0];
            requireContainerMember(index->base->inferredType, "index_mut");
            if (!isImplicitlyConvertible(index->index->inferredType, keyType) || index->index->inferredType != keyType) {
                generatedIndex = castExpressionTo(generatedIndex, index->index->inferredType, keyType);
            }
            return "((" + base + ")[" + generatedIndex + "])";
        }
        if (const auto* field = dynamic_cast<const FieldExpr*>(&expr)) {
            const std::string base = generateMutableAccess(*field->base);
            return "((" + base + ")" +
                (isClassType(field->base->inferredType) ? "->" : ".") +
                field->field + ")";
        }
        return generate(expr);
    }

// generateSlice implements the generateSlice behavior for the expressionCodegen.cpp module.
    std::string generateSlice(const SliceExpr& expr) const {
        const std::string base = generate(*expr.base);
        const std::string start = expr.start ? generate(*expr.start) : "0";
        const std::string end = expr.end ? generate(*expr.end) : "LLONG_MAX";
        if (emitRuntimeChecks) {
            requireRuntimeHelper("CPPPListSlice");
            return "CPPPListSlice(" + base + ", " + start + ", " + end + ")";
        }
        requireContainerMember(expr.base->inferredType, "size");
        requireContainerMember(expr.base->inferredType, "begin_const");
        requireContainerMember(expr.base->inferredType, "end_const");
        requireContainerMember(expr.inferredType, "ctor_default");
        requireContainerMember(expr.inferredType, "ctor_iterator");
        return "([&]() { const auto& __cppp_list = " + base + "; long long __cppp_start = static_cast<long long>(" + start + "); long long __cppp_end = " +
            (expr.end ? "static_cast<long long>(" + end + ")" : "static_cast<long long>(__cppp_list.size())") +
            "; long long __cppp_size = static_cast<long long>(__cppp_list.size()); if (__cppp_start < 0) __cppp_start += __cppp_size; if (__cppp_end < 0) __cppp_end += __cppp_size; __cppp_start = max(0LL, min(__cppp_start, __cppp_size)); __cppp_end = max(0LL, min(__cppp_end, __cppp_size)); if (__cppp_start >= __cppp_end) return CPPPList<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">{}; return CPPPList<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">(__cppp_list.begin() + static_cast<CPPPList<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">::difference_type>(__cppp_start), __cppp_list.begin() + static_cast<CPPPList<" + cppTypeForExpressionType(expr.inferredType.subtypes[0]) + ">::difference_type>(__cppp_end)); }())";
    }

// generateListLiteral implements the generateListLiteral behavior for the expressionCodegen.cpp module.
    std::string generateListLiteral(const ListLiteralExpr& expr) const {
        requireContainerMember(expr.inferredType, expr.elements.empty() ? "ctor_default" : "ctor_init");
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
        requireContainerMember(expr.inferredType, expr.elements.empty() ? "ctor_default" : "ctor_init");
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
        requireContainerMember(expr.inferredType, expr.entries.empty() ? "ctor_default" : "ctor_init");
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
        requireContainerMember(expr.inferredType, "ctor_values");
        const Type& firstType = expr.inferredType.subtypes[0];
        const Type& secondType = expr.inferredType.subtypes[1];
        std::string first = generate(*expr.first);
        std::string second = generate(*expr.second);
        if (expr.first->inferredType != firstType) {
            first = castExpressionTo(first, expr.first->inferredType, firstType);
        }
        if (expr.second->inferredType != secondType) {
            second = castExpressionTo(second, expr.second->inferredType, secondType);
        }
        return "CPPPPair<" + cppTypeForExpressionType(firstType) + ", " +
            cppTypeForExpressionType(secondType) + ">(" + first + ", " + second + ")";
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
            requireContainerMember(index->base->inferredType, "index_mut");
            requireContainerMember(index->base->inferredType, "size");
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
            if (literal != nullptr) {
                requireContainerMember(index->base->inferredType, literal->text == "0" ? "first_mut" : "second_mut");
                return "((" + base + ")." + (literal->text == "0" ? "first()" : "second()") + ")";
            }
            requireContainerMember(index->base->inferredType, "first_mut");
            requireContainerMember(index->base->inferredType, "second_mut");
            return "([&]() -> decltype(auto) { auto& __cppp_pair = " + base +
                "; long long __cppp_index = static_cast<long long>(" + generatedIndex +
                "); if (__cppp_index != 0 && __cppp_index != 1) { throw runtime_error(\"" +
                std::to_string(lineNumber) + ":" + std::to_string(index->sourceColumn) +
                ":Pair index must be 0 or 1\"); } return __cppp_index == 0 ? __cppp_pair.first() : __cppp_pair.second(); }())";
        }
        const Type keyType = index->base->inferredType.subtypes[0];
        requireContainerMember(index->base->inferredType, "index_mut");
        if (!isImplicitlyConvertible(index->index->inferredType, keyType) || index->index->inferredType != keyType) {
            generatedIndex = castExpressionTo(generatedIndex, index->index->inferredType, keyType);
        }
        return "((" + base + ")[" + generatedIndex + "])";
    }
    if (const auto* field = dynamic_cast<const FieldExpr*>(&expr)) {
        const std::string base = generateMutableAccessExpression(
            *field->base, lineNumber, emitRuntimeChecks, declaredFunctions);
        return "((" + base + ")" + (isClassType(field->base->inferredType) ? "->" : ".") + field->field + ")";
    }
    return ExpressionCodegen(lineNumber, emitRuntimeChecks, declaredFunctions).generate(expr);
}

std::string generateAnalyzedExpression(
    const Expr& expression,
    int lineNumber,
    bool emitRuntimeChecks,
    const std::map<std::string, FunctionSignature>& declaredFunctions
) {
    return ExpressionCodegen(lineNumber, emitRuntimeChecks, declaredFunctions).generate(expression);
}

// Compatibility adapter: assignment/list emitters still provide token slices.
// Keep this localized until those callers carry analyzed lvalue Expr nodes.
LvalueEmitResult emitLvalueExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& expressionTokens,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks
) {
    static const std::map<std::string, FunctionSignature> emptyFunctions;
    return emitLvalueExpression(
        inputFile, lineNumber, expressionTokens, expressionColumn, sourceLines,
        declaredVariables, emptyFunctions, emitRuntimeChecks);
}

LvalueEmitResult emitLvalueExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& expressionTokens,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    bool emitRuntimeChecks
) {
    ExpressionParser parser(
        inputFile, lineNumber, expressionTokens, expressionColumn, sourceLines);
    for (const Token& token : expressionTokens) {
        if ((token.kind == TokenKind::String || token.kind == TokenKind::Char) &&
            (token.text.size() < 2 || token.text.front() != token.text.back())) {
            recordSourceError(
                inputFile, lineNumber,
                expressionColumn + token.span.startColumn - 1,
                token.kind == TokenKind::Char ? "unterminated char literal" : "unterminated string literal",
                sourceLines);
            return {false, "", PrimitiveType::Unknown, expressionColumn};
        }
    }
    bool ok = true;
    std::unique_ptr<Expr> expression = parser.parseAst(ok);
    if (!ok || !expression || !analyzeExpressionAst(
            *expression, inputFile, lineNumber, sourceLines,
            declaredVariables, declaredFunctions)) {
        return {false, "", PrimitiveType::Unknown, expressionColumn};
    }
    if (!expression->mutableValue) {
        recordSourceError(inputFile, lineNumber, expression->sourceColumn,
            "assignment target must be a mutable variable or collection element", sourceLines);
        return {false, "", PrimitiveType::Unknown, expression->sourceColumn};
    }
    return {
        true,
        generateMutableAccessExpression(*expression, lineNumber, emitRuntimeChecks, declaredFunctions),
        expression->inferredType,
        expression->sourceColumn
    };
}
