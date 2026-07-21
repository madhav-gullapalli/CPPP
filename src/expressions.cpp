/*
 * expressions.cpp
 *
 * Provides shared expression utilities such as coercion, casting, and runtime helper emission.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "expressions.h"

#include "expressionParser.h"
#include "typesCppp.h"

namespace {
bool& expressionRuntimeChecksEnabled() {
    static bool enabled = false;
    return enabled;
}

const std::map<std::string, FunctionSignature>*& declaredFunctionsForExpressions() {
    static const std::map<std::string, FunctionSignature>* functions = nullptr;
    return functions;
}

const std::map<std::string, std::map<std::string, Type>>*& declaredStructsForExpressions() {
    static const std::map<std::string, std::map<std::string, Type>>* structs = nullptr;
    return structs;
}

const std::map<std::string, std::vector<std::string>>*& declaredStructFieldOrdersForExpressions() {
    static const std::map<std::string, std::vector<std::string>>* orders = nullptr;
    return orders;
}

const std::map<std::string, std::map<std::string, FunctionSignature>>*& declaredStructMethodsForExpressions() {
    static const std::map<std::string, std::map<std::string, FunctionSignature>>* methods = nullptr;
    return methods;
}

// trim removes surrounding whitespace from a string.
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// listDepth handles list-specific behavior for the compiler or runtime.
int listDepth(const Type& type) {
    int depth = 0;
    Type current = type;
    while (current.primitive == PrimitiveType::List && current.subtypes.size() == 1) {
        ++depth;
        current = current.subtypes[0];
    }
    return depth;
}

std::string emitListInputExpression(
    const Type& currentType,
    const std::vector<std::string>& dimensions,
    size_t dimensionIndex
) {
    if (currentType.primitive != PrimitiveType::List || currentType.subtypes.size() != 1) {
        return inputFunctionForType(currentType);
    }

    if (dimensionIndex >= dimensions.size()) {
        if (isStringType(currentType)) {
            return inputFunctionForType(currentType);
        }
        const int depth = listDepth(currentType);
        if (depth == 1) {
            std::string elementCppType;
            if (currentType.subtypes[0] == PrimitiveType::Bool) {
                elementCppType = "bool";
            } else if (currentType.subtypes[0] == PrimitiveType::Char) {
                elementCppType = "CPPPChar";
            } else if (currentType.subtypes[0] == PrimitiveType::Int) {
                elementCppType = "long long";
            } else if (currentType.subtypes[0] == PrimitiveType::Float) {
                elementCppType = "long double";
            }
            if (elementCppType.empty()) {
                return "";
            }
            requireRuntimeHelper("CPPPInputListLine");
            return "CPPPInputListLine<" + elementCppType + ">()";
        }
        return "";
    }

    requireRuntimeHelper("CPPPInputList");
    const std::string elementExpression = emitListInputExpression(currentType.subtypes[0], dimensions, dimensionIndex + 1);
    return "CPPPInputList(" + dimensions[dimensionIndex] + ", [&]() { return " + elementExpression + "; })";
}

std::string emitPairInputExpression(const Type& type) {
    if (!isPairType(type)) {
        return inputFunctionForType(type);
    }

    const std::string first = emitPairInputExpression(type.subtypes[0]);
    const std::string second = emitPairInputExpression(type.subtypes[1]);
    if (first.empty() || second.empty()) {
        return "";
    }

    return "make_pair(" + first + ", " + second + ")";
}

std::string castLambdaExpression(const Type& from, const Type& to) {
    const std::string fromCppType = cppTypeForType(from);
    return "[&](const " + fromCppType + "& __cppp_value) { return " + castExpressionTo("__cppp_value", from, to) + "; }";
}
}

// primitiveArity implements the primitiveArity behavior for the expressions.cpp module.
int primitiveArity(PrimitiveType primitive) {
    switch (primitive) {
        case PrimitiveType::Void:
        case PrimitiveType::Bool:
        case PrimitiveType::Char:
        case PrimitiveType::Int:
        case PrimitiveType::Float:
        case PrimitiveType::Range:
            return 0;
        case PrimitiveType::List:
        case PrimitiveType::Set:
            return 1;
        case PrimitiveType::Map:
        case PrimitiveType::Pair:
            return 2;
        case PrimitiveType::Struct:
            return 0;
        case PrimitiveType::Unknown:
            return 0;
    }

    return 0;
}

// cpppTypeName implements the cpppTypeName behavior for the expressions.cpp module.
std::string cpppTypeName(const Type& type) {
    if (isStringType(type)) {
        return "string";
    }

    switch (type.primitive) {
        case PrimitiveType::Void:
            return "void";
        case PrimitiveType::Bool:
            return "bool";
        case PrimitiveType::Char:
            return "char";
        case PrimitiveType::Int:
            return "int";
        case PrimitiveType::Float:
            return "float";
        case PrimitiveType::Range:
            return "range";
        case PrimitiveType::List:
            if (type.subtypes.size() == 1) {
                return "List<" + cpppTypeName(type.subtypes[0]) + ">";
            }
            return "List";
        case PrimitiveType::Set:
            if (type.subtypes.size() == 1) {
                return "Set<" + cpppTypeName(type.subtypes[0]) + ">";
            }
            return "Set";
        case PrimitiveType::Map:
            if (type.subtypes.size() == 2) {
                return "Map<" + cpppTypeName(type.subtypes[0]) + ", " + cpppTypeName(type.subtypes[1]) + ">";
            }
            return "Map";
        case PrimitiveType::Pair:
            if (type.subtypes.size() == 2) {
                return "Pair<" + cpppTypeName(type.subtypes[0]) + ", " + cpppTypeName(type.subtypes[1]) + ">";
            }
            return "Pair";
        case PrimitiveType::Struct:
            return type.name;
        case PrimitiveType::Unknown:
            return "unknown";
    }

    return "unknown";
}

// isStringType returns whether the supplied input satisfies the relevant condition.
bool isStringType(const Type& type) {
    return type.primitive == PrimitiveType::List &&
        type.subtypes.size() == 1 &&
        type.subtypes[0] == PrimitiveType::Char;
}

bool isListType(const Type& type) {
    return type.primitive == PrimitiveType::List && type.subtypes.size() == 1;
}

bool isRangeType(const Type& type) {
    return type == PrimitiveType::Range;
}

bool isSetType(const Type& type) {
    return type.primitive == PrimitiveType::Set && type.subtypes.size() == 1;
}

bool isMapType(const Type& type) {
    return type.primitive == PrimitiveType::Map && type.subtypes.size() == 2;
}

bool isPairType(const Type& type) {
    return type.primitive == PrimitiveType::Pair && type.subtypes.size() == 2;
}

bool isStructType(const Type& type) {
    return type.primitive == PrimitiveType::Struct && !type.name.empty();
}

bool isCollectionType(const Type& type) {
    return isListType(type) || isSetType(type) || isMapType(type);
}

namespace {
bool isScalarCastType(const Type& type) {
    return type == PrimitiveType::Bool ||
        type == PrimitiveType::Char ||
        type == PrimitiveType::Int ||
        type == PrimitiveType::Float;
}

bool canCollectionCastElementType(const Type& from, const Type& to) {
    if (from == to) {
        return true;
    }

    if (isScalarCastType(from) && isScalarCastType(to)) {
        return true;
    }

    if (isPairType(from) && isPairType(to)) {
        return canCollectionCastElementType(from.subtypes[0], to.subtypes[0]) &&
            canCollectionCastElementType(from.subtypes[1], to.subtypes[1]);
    }

    return false;
}
}

// isImplicitlyConvertible returns whether the supplied input satisfies the relevant condition.
bool isImplicitlyConvertible(const Type& from, const Type& to) {
    if (to == PrimitiveType::Bool && isCollectionType(from)) {
        return true;
    }

    if (isStructType(from) || isStructType(to)) {
        return from == to;
    }

    if (!from.subtypes.empty() || !to.subtypes.empty()) {
        return from == to;
    }

    if (from == to) {
        return true;
    }

    if (from == PrimitiveType::Bool) {
        return to == PrimitiveType::Char || to == PrimitiveType::Int || to == PrimitiveType::Float;
    }

    if (from == PrimitiveType::Char) {
        return to == PrimitiveType::Bool || to == PrimitiveType::Int || to == PrimitiveType::Float;
    }

    if (from == PrimitiveType::Int) {
        return to == PrimitiveType::Bool || to == PrimitiveType::Char || to == PrimitiveType::Float;
    }

    if (from == PrimitiveType::Float) {
        return to == PrimitiveType::Bool;
    }

    if (from == PrimitiveType::Void || to == PrimitiveType::Void) {
        return from == to;
    }

    if (isCollectionType(from) || isCollectionType(to)) {
        return from == to;
    }

    return false;
}

bool canExplicitlyCastType(const Type& from, const Type& to) {
    if (from == to) {
        return true;
    }

    if (from == PrimitiveType::Void || to == PrimitiveType::Void) {
        return false;
    }

    if (isScalarCastType(from) && isScalarCastType(to)) {
        return true;
    }

    if (isStringType(to)) {
        return isScalarCastType(from);
    }

    if (isStringType(from)) {
        return isScalarCastType(to);
    }

    if (isPairType(from) && isPairType(to)) {
        return canExplicitlyCastType(from.subtypes[0], to.subtypes[0]) &&
            canExplicitlyCastType(from.subtypes[1], to.subtypes[1]);
    }

    if (isSetType(to) && isListType(from)) {
        return canCollectionCastElementType(from.subtypes[0], to.subtypes[0]);
    }

    if (isSetType(to) && isRangeType(from)) {
        return to.subtypes[0] == PrimitiveType::Int;
    }

    if (isListType(to) && isSetType(from)) {
        return canCollectionCastElementType(from.subtypes[0], to.subtypes[0]);
    }

    if (isListType(to) && isRangeType(from)) {
        return to.subtypes[0] == PrimitiveType::Int;
    }

    if (isMapType(to) && isListType(from) && isPairType(from.subtypes[0])) {
        return canCollectionCastElementType(from.subtypes[0].subtypes[0], to.subtypes[0]) &&
            canCollectionCastElementType(from.subtypes[0].subtypes[1], to.subtypes[1]);
    }

    if (isListType(to) && isPairType(to.subtypes[0]) && isMapType(from)) {
        return canCollectionCastElementType(from.subtypes[0], to.subtypes[0].subtypes[0]) &&
            canCollectionCastElementType(from.subtypes[1], to.subtypes[0].subtypes[1]);
    }

    return false;
}

// castExpressionTo implements the castExpressionTo behavior for the expressions.cpp module.
std::string castExpressionTo(const std::string& expression, const Type& to) {
    return castExpressionTo(expression, PrimitiveType::Unknown, to);
}

// castExpressionTo implements the castExpressionTo behavior for the expressions.cpp module.
std::string castExpressionTo(const std::string& expression, const Type& from, const Type& to) {
    if (from == to) {
        return expression;
    }

    if (isStringType(to)) {
        if (from == PrimitiveType::Bool) {
            requireRuntimeHelper("CPPPToStringBool");
            return "CPPPToStringBool(" + expression + ")";
        }
        if (from == PrimitiveType::Char) {
            requireRuntimeHelper("CPPPToStringChar");
            return "CPPPToStringChar(" + expression + ")";
        }
        if (from == PrimitiveType::Int) {
            requireRuntimeHelper("CPPPToStringInt");
            return "CPPPToStringInt(" + expression + ")";
        }
        if (from == PrimitiveType::Float) {
            requireRuntimeHelper("CPPPToStringFloat");
            return "CPPPToStringFloat(" + expression + ")";
        }
        return expression;
    }

    switch (to.primitive) {
        case PrimitiveType::Void:
            return expression;
        case PrimitiveType::Bool:
            switch (from.primitive) {
                case PrimitiveType::Void:
                    return expression;
                case PrimitiveType::Bool:
                    requireRuntimeHelper("CPPPToBoolBool");
                    return "CPPPToBoolBool(" + expression + ")";
                case PrimitiveType::Char:
                    requireRuntimeHelper("CPPPToBoolChar");
                    return "CPPPToBoolChar(" + expression + ")";
                case PrimitiveType::Int:
                    requireRuntimeHelper("CPPPToBoolInt");
                    return "CPPPToBoolInt(" + expression + ")";
                case PrimitiveType::Float:
                    requireRuntimeHelper("CPPPToBoolFloat");
                    return "CPPPToBoolFloat(" + expression + ")";
                case PrimitiveType::Range:
                    return "(!(" + expression + ").empty())";
                case PrimitiveType::List:
                case PrimitiveType::Set:
                case PrimitiveType::Map:
                    return "(!(" + expression + ").empty())";
                case PrimitiveType::Pair:
                    return expression;
                case PrimitiveType::Struct:
                    return "(" + expression + " != nullptr)";
                case PrimitiveType::Unknown:
                    requireRuntimeHelper("CPPPToBoolFallback");
                    return "CPPPToBool(" + expression + ")";
            }
            requireRuntimeHelper("CPPPToBoolFallback");
            return "CPPPToBool(" + expression + ")";
        case PrimitiveType::Char:
            requireRuntimeHelper("CPPPCharType");
            return "CPPPChar(static_cast<char>(" + expression + "))";
        case PrimitiveType::Int:
            return "static_cast<long long>(" + expression + ")";
        case PrimitiveType::Float:
            return "static_cast<long double>(" + expression + ")";
        case PrimitiveType::List:
            if (isRangeType(from) && to.subtypes[0] == PrimitiveType::Int) {
                requireRuntimeHelper("CPPPRangeToList");
                return "CPPPRangeToList(" + expression + ")";
            }
            if (isSetType(from)) {
                requireRuntimeHelper("CPPPSetToList");
                return "CPPPSetToList<" + cppTypeForType(to.subtypes[0]) + ">(" + expression + ", " +
                    castLambdaExpression(from.subtypes[0], to.subtypes[0]) + ")";
            }
            if (isMapType(from) && isPairType(to.subtypes[0])) {
                requireRuntimeHelper("CPPPMapToList");
                return "CPPPMapToList<" + cppTypeForType(to.subtypes[0].subtypes[0]) + ", " + cppTypeForType(to.subtypes[0].subtypes[1]) + ">(" + expression + ", " +
                    castLambdaExpression(from.subtypes[0], to.subtypes[0].subtypes[0]) + ", " +
                    castLambdaExpression(from.subtypes[1], to.subtypes[0].subtypes[1]) + ")";
            }
            return expression;
        case PrimitiveType::Set:
            if (isRangeType(from) && to.subtypes[0] == PrimitiveType::Int) {
                requireRuntimeHelper("CPPPRangeToSet");
                return "CPPPRangeToSet(" + expression + ")";
            }
            if (isListType(from)) {
                requireRuntimeHelper("CPPPListToSet");
                return "CPPPListToSet<" + cppTypeForType(to.subtypes[0]) + ">(" + expression + ", " +
                    castLambdaExpression(from.subtypes[0], to.subtypes[0]) + ")";
            }
            return expression;
        case PrimitiveType::Map:
            if (isListType(from) && isPairType(from.subtypes[0])) {
                requireRuntimeHelper("CPPPListToMap");
                return "CPPPListToMap<" + cppTypeForType(to.subtypes[0]) + ", " + cppTypeForType(to.subtypes[1]) + ">(" + expression + ", " +
                    castLambdaExpression(from.subtypes[0].subtypes[0], to.subtypes[0]) + ", " +
                    castLambdaExpression(from.subtypes[0].subtypes[1], to.subtypes[1]) + ")";
            }
            return expression;
        case PrimitiveType::Range:
            return expression;
        case PrimitiveType::Struct:
            return expression;
        case PrimitiveType::Unknown:
            return expression;
        case PrimitiveType::Pair:
            if (isPairType(from)) {
                return "make_pair(" +
                    castExpressionTo("(" + expression + ").first", from.subtypes[0], to.subtypes[0]) + ", " +
                    castExpressionTo("(" + expression + ").second", from.subtypes[1], to.subtypes[1]) + ")";
            }
            return expression;
    }

    return expression;
}

// declaredTypeForName implements the declaredTypeForName behavior for the expressions.cpp module.
Type declaredTypeForName(const std::string& name) {
    if (name == "bool") {
        return PrimitiveType::Bool;
    }
    if (name == "char") {
        return PrimitiveType::Char;
    }
    if (name == "int") {
        return PrimitiveType::Int;
    }
    if (name == "float") {
        return PrimitiveType::Float;
    }
    if (name == "range") {
        return PrimitiveType::Range;
    }
    if (name == "void") {
        return PrimitiveType::Void;
    }
    if (name == "List") {
        return PrimitiveType::List;
    }
    if (name == "Set") {
        return PrimitiveType::Set;
    }
    if (name == "Map") {
        return PrimitiveType::Map;
    }
    if (name == "Pair") {
        return PrimitiveType::Pair;
    }
    if (name == "vector") {
        return PrimitiveType::List;
    }
    if (name == "set") {
        return PrimitiveType::Set;
    }
    if (name == "map") {
        return PrimitiveType::Map;
    }
    if (name == "pair") {
        return PrimitiveType::Pair;
    }
    if (name == "string") {
        return Type(PrimitiveType::List, {Type(PrimitiveType::Char)});
    }

    const auto* structs = declaredStructsForExpressions();
    if (structs && structs->count(name) != 0) {
        return Type(PrimitiveType::Struct, name);
    }

    return PrimitiveType::Unknown;
}

void setDeclaredStructsForExpressions(const std::map<std::string, std::map<std::string, Type>>* declaredStructs) {
    declaredStructsForExpressions() = declaredStructs;
}

void setDeclaredStructFieldOrdersForExpressions(const std::map<std::string, std::vector<std::string>>* fieldOrders) {
    declaredStructFieldOrdersForExpressions() = fieldOrders;
}

void setDeclaredStructMethodsForExpressions(const std::map<std::string, std::map<std::string, FunctionSignature>>* methods) {
    declaredStructMethodsForExpressions() = methods;
}

const std::map<std::string, Type>* declaredStructFieldsForName(const std::string& name) {
    const auto* structs = declaredStructsForExpressions();
    if (structs == nullptr) {
        return nullptr;
    }
    const auto found = structs->find(name);
    return found == structs->end() ? nullptr : &found->second;
}

const std::vector<std::string>* declaredStructFieldOrderForName(const std::string& name) {
    const auto* orders = declaredStructFieldOrdersForExpressions();
    if (orders == nullptr) {
        return nullptr;
    }
    const auto found = orders->find(name);
    return found == orders->end() ? nullptr : &found->second;
}

const FunctionSignature* declaredStructMethodForType(const Type& type, const std::string& name) {
    if (!isStructType(type) || declaredStructMethodsForExpressions() == nullptr) {
        return nullptr;
    }
    const auto methods = declaredStructMethodsForExpressions()->find(type.name);
    if (methods == declaredStructMethodsForExpressions()->end()) {
        return nullptr;
    }
    const auto method = methods->second.find(name);
    return method == methods->second.end() ? nullptr : &method->second;
}

// cppTypeForInput implements the cppTypeForInput behavior for the expressions.cpp module.
std::string cppTypeForInput(const Type& type) {
    if (type == PrimitiveType::Bool) {
        return "bool";
    }
    if (type == PrimitiveType::Char) {
        return "CPPPChar";
    }
    if (type == PrimitiveType::Int) {
        return "long long";
    }
    if (type == PrimitiveType::Float) {
        return "long double";
    }
    if (type == PrimitiveType::Void) {
        return "void";
    }
    if (isListType(type)) {
        return "vector<" + cppTypeForInput(type.subtypes[0]) + ">";
    }
    if (isPairType(type)) {
        const std::string first = cppTypeForInput(type.subtypes[0]);
        const std::string second = cppTypeForInput(type.subtypes[1]);
        if (first.empty() || second.empty()) {
            return "";
        }
        return "pair<" + first + ", " + second + ">";
    }
    return "";
}

// isInputCall returns whether the supplied input satisfies the relevant condition.
bool isInputCall(const std::vector<Token>& tokens) {
    return tokens.size() == 4 &&
        tokens[0].kind == TokenKind::Identifier &&
        tokens[0].text == "input" &&
        tokens[1].kind == TokenKind::LeftParen &&
        tokens[2].kind == TokenKind::RightParen &&
        tokens[3].kind == TokenKind::EndOfFile;
}

// parseInputCall parses inputcall for the compiler pipeline.
bool parseInputCall(const std::string& text, int startColumn, std::vector<InputArgument>& arguments) {
    const std::vector<Token> tokens = tokenize(text);
    if (tokens.size() < 4 ||
        tokens[0].kind != TokenKind::Identifier ||
        tokens[0].text != "input" ||
        tokens[1].kind != TokenKind::LeftParen ||
        tokens.back().kind != TokenKind::EndOfFile ||
        tokens[tokens.size() - 2].kind != TokenKind::RightParen) {
        return false;
    }

    const int argumentsStartColumn = startColumn + tokens[1].span.endColumn;
    const size_t contentStart = static_cast<size_t>(tokens[1].span.endColumn);
    const size_t contentLength = static_cast<size_t>(tokens[tokens.size() - 2].span.startColumn - tokens[1].span.endColumn - 1);
    const std::string content = text.substr(contentStart, contentLength);
    const std::vector<Token> contentTokens = tokenize(content);

    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;
    size_t argumentStartIndex = 0;
    int argumentColumn = argumentsStartColumn;

    for (const Token& token : contentTokens) {
        if (token.kind == TokenKind::EndOfFile) {
            break;
        }
        if (token.kind == TokenKind::LeftParen) {
            ++parenDepth;
        } else if (token.kind == TokenKind::RightParen && parenDepth > 0) {
            --parenDepth;
        } else if (token.kind == TokenKind::LeftBracket) {
            ++bracketDepth;
        } else if (token.kind == TokenKind::RightBracket && bracketDepth > 0) {
            --bracketDepth;
        } else if (token.kind == TokenKind::Unknown && token.text == "{") {
            ++braceDepth;
        } else if (token.kind == TokenKind::Unknown && token.text == "}" && braceDepth > 0) {
            --braceDepth;
        } else if (token.kind == TokenKind::Comma && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
            const std::string rawArgument = content.substr(argumentStartIndex, static_cast<size_t>(token.span.startColumn - 1) - argumentStartIndex);
            const std::string argumentText = trim(rawArgument);
            const size_t trimStart = rawArgument.find_first_not_of(" \t\r\n");
            arguments.push_back({
                argumentText,
                trimStart == std::string::npos ? argumentColumn : argumentColumn + static_cast<int>(trimStart)
            });
            argumentStartIndex = static_cast<size_t>(token.span.endColumn);
            argumentColumn = argumentsStartColumn + token.span.endColumn;
        }
    }

    const std::string rawArgument = content.substr(argumentStartIndex);
    const std::string argumentText = trim(rawArgument);
    if (!argumentText.empty() || !content.empty()) {
        const size_t trimStart = rawArgument.find_first_not_of(" \t\r\n");
        arguments.push_back({
            argumentText,
            trimStart == std::string::npos ? argumentColumn : argumentColumn + static_cast<int>(trimStart)
        });
    }

    return true;
}

// inputFunctionForType implements the inputFunctionForType behavior for the expressions.cpp module.
std::string inputFunctionForType(const Type& type) {
    if (isStringType(type)) {
        requireRuntimeHelper("CPPPInputStringWord");
        return "CPPPInputString()";
    }

    switch (type.primitive) {
        case PrimitiveType::Void:
            return "";
        case PrimitiveType::Bool:
            requireRuntimeHelper("CPPPInputBool");
            return "CPPPInputBool()";
        case PrimitiveType::Char:
            requireRuntimeHelper("CPPPInputChar");
            return "CPPPInputChar()";
        case PrimitiveType::Int:
            requireRuntimeHelper("CPPPInputInt");
            return "CPPPInputInt()";
        case PrimitiveType::Float:
            requireRuntimeHelper("CPPPInputFloat");
            return "CPPPInputFloat()";
        case PrimitiveType::Range:
            return "";
        case PrimitiveType::List:
        case PrimitiveType::Set:
        case PrimitiveType::Map:
        case PrimitiveType::Unknown:
            return "";
        case PrimitiveType::Pair:
            return emitPairInputExpression(type);
        case PrimitiveType::Struct:
            return "";
    }

    return "";
}

bool emitInputCallForType(
    const std::string& inputFile,
    int lineNumber,
    const std::string& inputText,
    int inputColumn,
    const Type& targetType,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    std::string& emittedExpression
) {
    std::vector<InputArgument> arguments;
    if (!parseInputCall(inputText, inputColumn, arguments)) {
        return false;
    }

    if (isStringType(targetType)) {
        if (arguments.empty()) {
            emittedExpression = inputFunctionForType(targetType);
            return true;
        }
        if (arguments.size() != 1) {
            recordSourceError(inputFile, lineNumber, arguments[0].column, "string input needs exactly 1 size argument", sourceLines);
            return false;
        }

        const InputArgument& argument = arguments[0];
        if (argument.text.empty()) {
            recordSourceError(inputFile, lineNumber, argument.column, "input() size argument cannot be empty", sourceLines);
            return false;
        }

        const ExpressionEmitResult expression = emitExpression(
            inputFile,
            lineNumber,
            argument.text,
            argument.column,
            sourceLines,
            declaredVariables
        );
        if (!expression.ok) {
            return false;
        }

        if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, PrimitiveType::Int)) {
            recordSourceError(inputFile, lineNumber, argument.column, "input() size must be int", sourceLines);
            return false;
        }

        std::string emittedSize = expression.generatedExpression;
        if (!isImplicitlyConvertible(expression.type, PrimitiveType::Int) || expression.type != PrimitiveType::Int) {
            emittedSize = castExpressionTo(emittedSize, expression.type, PrimitiveType::Int);
        }
        requireRuntimeHelper("CPPPInputStringCount");
        emittedExpression = "CPPPInputString(" + emittedSize + ")";
        return true;
    }

    if (targetType.primitive != PrimitiveType::List) {
        if (!arguments.empty()) {
            recordSourceError(inputFile, lineNumber, arguments[0].column, "input(count) is only supported for List targets", sourceLines);
            return false;
        }

        emittedExpression = inputFunctionForType(targetType);
        return true;
    }

    const int depth = listDepth(targetType);
    if (arguments.empty()) {
        if (depth == 1) {
            const std::string elementCppType = cppTypeForInput(targetType.subtypes[0]);
            if (elementCppType.empty()) {
                recordSourceError(inputFile, lineNumber, inputColumn, "unsupported List input element type", sourceLines);
                return false;
            }
            requireRuntimeHelper("CPPPInputListLine");
            emittedExpression = "CPPPInputListLine<" + elementCppType + ">()";
            return true;
        }

        recordSourceError(inputFile, lineNumber, inputColumn, "List input() without sizes only supports one-dimensional Lists; use input(n) or one size per List dimension", sourceLines);
        return false;
    }

    const bool exactDimensions = static_cast<int>(arguments.size()) == depth;
    const bool lineFinalDimension = depth > 1 && static_cast<int>(arguments.size()) == depth - 1;
    if (!exactDimensions && !lineFinalDimension) {
        std::string expected = "0 or 1";
        if (depth > 1) {
            expected = std::to_string(depth - 1) + " or " + std::to_string(depth);
        }
        recordSourceError(
            inputFile,
            lineNumber,
            arguments[0].column,
            "List input needs exactly " + expected + " size argument" + ((depth == 1 || depth == 2) ? "" : "s"),
            sourceLines
        );
        return false;
    }

    std::vector<std::string> dimensions;
    for (const InputArgument& argument : arguments) {
        if (argument.text.empty()) {
            recordSourceError(inputFile, lineNumber, argument.column, "input() size argument cannot be empty", sourceLines);
            return false;
        }

        const ExpressionEmitResult expression = emitExpression(
            inputFile,
            lineNumber,
            argument.text,
            argument.column,
            sourceLines,
            declaredVariables
        );
        if (!expression.ok) {
            return false;
        }

        if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, PrimitiveType::Int)) {
            recordSourceError(inputFile, lineNumber, argument.column, "input() size must be int", sourceLines);
            return false;
        }

        std::string emittedSize = expression.generatedExpression;
        if (!isImplicitlyConvertible(expression.type, PrimitiveType::Int) || expression.type != PrimitiveType::Int) {
            emittedSize = castExpressionTo(emittedSize, expression.type, PrimitiveType::Int);
        }
        dimensions.push_back(emittedSize);
    }

    emittedExpression = emitListInputExpression(targetType, dimensions, 0);
    if (emittedExpression.empty()) {
        recordSourceError(inputFile, lineNumber, arguments[0].column, "List input sizes must leave at most one final line-shaped List dimension", sourceLines);
        return false;
    }
    return true;
}

ExpressionEmitResult emitExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks
) {
    static const std::map<std::string, FunctionSignature> emptyFunctions;
    const std::map<std::string, FunctionSignature>* declaredFunctions = declaredFunctionsForExpressions();
    return emitExpression(
        inputFile,
        lineNumber,
        expressionText,
        expressionColumn,
        sourceLines,
        declaredVariables,
        declaredFunctions == nullptr ? emptyFunctions : *declaredFunctions,
        emitRuntimeChecks
    );
}

ExpressionEmitResult emitExpression(
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
        declaredFunctions.empty() && declaredFunctionsForExpressions() != nullptr ? *declaredFunctionsForExpressions() : declaredFunctions,
        emitRuntimeChecks || expressionRuntimeChecksEnabled()
    );
    return parser.parse();
}

void setExpressionRuntimeChecksEnabled(bool enabled) {
    expressionRuntimeChecksEnabled() = enabled;
}

void setDeclaredFunctionsForExpressions(const std::map<std::string, FunctionSignature>* declaredFunctions) {
    declaredFunctionsForExpressions() = declaredFunctions;
}

std::unique_ptr<Expr> parseExpressionAst(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables
) {
    static const std::map<std::string, FunctionSignature> emptyFunctions;
    const std::map<std::string, FunctionSignature>* declaredFunctions = declaredFunctionsForExpressions();
    return parseExpressionAst(
        inputFile,
        lineNumber,
        expressionText,
        expressionColumn,
        sourceLines,
        declaredVariables,
        declaredFunctions == nullptr ? emptyFunctions : *declaredFunctions
    );
}

std::unique_ptr<Expr> parseExpressionAst(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions
) {
    ExpressionParser parser(
        inputFile,
        lineNumber,
        expressionText,
        expressionColumn,
        sourceLines,
        declaredVariables,
        declaredFunctions.empty() && declaredFunctionsForExpressions() != nullptr ? *declaredFunctionsForExpressions() : declaredFunctions,
        expressionRuntimeChecksEnabled()
    );
    bool ok = true;
    std::unique_ptr<Expr> expression = parser.parseAst(ok);
    if (!ok) {
        return nullptr;
    }
    return expression;
}

// hasArithmeticOperator returns whether the supplied input satisfies the relevant condition.
bool hasArithmeticOperator(const std::vector<Token>& tokens) {
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::Operator &&
            (token.text == "+" || token.text == "-" || token.text == "*" || token.text == "/" || token.text == "%" ||
             token.text == "<<" || token.text == ">>" ||
             token.text == "^" || token.text == "&" || token.text == "|" ||
             token.text == "&&" || token.text == "||" || token.text == "!" ||
             token.text == "<" || token.text == "<=" || token.text == ">" || token.text == ">=" ||
             token.text == "==" || token.text == "!=" ||
             token.text == "++" || token.text == "--")) {
            return true;
        }
        if (token.kind == TokenKind::LeftBracket || token.kind == TokenKind::RightBracket) {
            return true;
        }
    }

    return false;
}
