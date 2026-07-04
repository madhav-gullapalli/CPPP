/*
 * functions.cpp
 *
 * Implements function parameter typing and runtime helper selection for function lowering.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "functions.h"

#include "typesCppp.h"

namespace {
// isReferenceParameterType returns whether the supplied input satisfies the relevant condition.
bool isReferenceParameterType(const Type& type) {
    return isStringType(type) || (type.primitive == PrimitiveType::List && type.subtypes.size() == 1);
}

// isDeepEligibleType returns whether the supplied input satisfies the relevant condition.
bool isDeepEligibleType(const Type& type) {
    return isReferenceParameterType(type);
}

// cppParameterType implements the cppParameterType behavior for the functions.cpp module.
std::string cppParameterType(const Type& type, bool deepCopy) {
    const std::string cppType = cppTypeForType(type);
    if (cppType.empty()) {
        return "";
    }
    if (!deepCopy && isReferenceParameterType(type)) {
        return cppType + "&";
    }
    return cppType;
}
}

// functionParameterTypesDescription implements the functionParameterTypesDescription behavior for the functions.cpp module.
std::string functionParameterTypesDescription(const FunctionSignature& signature) {
    if (signature.parameters.empty()) {
        return "no";
    }

    std::string description;
    for (size_t i = 0; i < signature.parameters.size(); ++i) {
        if (i > 0) {
            description += " and ";
        }
        description += cpppTypeName(signature.parameters[i].type);
    }
    return description;
}

// functionArgumentTypesDescription implements the functionArgumentTypesDescription behavior for the functions.cpp module.
std::string functionArgumentTypesDescription(const std::vector<Type>& argumentTypes) {
    if (argumentTypes.empty()) {
        return "no";
    }

    std::string description;
    for (size_t i = 0; i < argumentTypes.size(); ++i) {
        if (i > 0) {
            description += " and ";
        }
        description += cpppTypeName(argumentTypes[i]);
    }
    return description;
}

ParsedFunctionHeader parseFunctionHeader(
    const std::string& inputFile,
    int lineNumber,
    const std::string& statementBody,
    int statementColumn,
    const std::map<int, std::string>& sourceLines
) {
    ParsedFunctionHeader result;
    const std::vector<Token> tokens = tokenize(statementBody);
    if (tokens.size() < 4 || tokens[0].kind != TokenKind::Identifier) {
        return result;
    }

    const ParsedTypeResult returnType = parseDeclaredTypeTokens(inputFile, lineNumber, tokens, 0, sourceLines, true);
    if (!returnType.matched) {
        return result;
    }

    size_t tokenIndex = returnType.nextTokenIndex;
    if (tokenIndex >= tokens.size() || tokens[tokenIndex].kind != TokenKind::Identifier) {
        return result;
    }

    const size_t nameIndex = tokenIndex;
    if (nameIndex + 1 >= tokens.size() || tokens[nameIndex + 1].kind != TokenKind::LeftParen) {
        return result;
    }

    result.matched = true;
    if (!returnType.ok) {
        result.ok = false;
        return result;
    }

    result.signature.returnType = returnType.type;
    result.signature.returnsVoid = returnType.type == PrimitiveType::Void;
    result.signature.name = tokens[tokenIndex].text;
    result.nameColumn = statementColumn + tokens[tokenIndex].span.startColumn - 1;
    ++tokenIndex;
    ++tokenIndex;

    while (tokenIndex < tokens.size() && tokens[tokenIndex].kind != TokenKind::RightParen) {
        bool deepCopy = false;
        if (tokens[tokenIndex].kind == TokenKind::Identifier && tokens[tokenIndex].text == "deep") {
            deepCopy = true;
            ++tokenIndex;
            if (tokenIndex >= tokens.size()) {
                recordSourceError(inputFile, lineNumber, statementColumn + tokens[tokenIndex - 1].span.startColumn - 1, "deep must precede a collection", sourceLines);
                result.ok = false;
                return result;
            }
        }

        const ParsedTypeResult parameterType = parseDeclaredTypeTokens(inputFile, lineNumber, tokens, tokenIndex, sourceLines);
        if (!parameterType.matched) {
            recordSourceError(inputFile, lineNumber, statementColumn + tokens[tokenIndex].span.startColumn - 1, "expected parameter type", sourceLines);
            result.ok = false;
            return result;
        }
        if (!parameterType.ok) {
            result.ok = false;
            return result;
        }
        if (deepCopy && !isDeepEligibleType(parameterType.type)) {
            recordSourceError(inputFile, lineNumber, statementColumn + tokens[tokenIndex - 1].span.startColumn - 1, "deep must precede a collection", sourceLines);
            result.ok = false;
            return result;
        }
        tokenIndex = parameterType.nextTokenIndex;
        if (tokenIndex >= tokens.size() || tokens[tokenIndex].kind != TokenKind::Identifier) {
            recordSourceError(inputFile, lineNumber, statementColumn + tokens[tokenIndex - 1].span.endColumn, "expected parameter name", sourceLines);
            result.ok = false;
            return result;
        }

        result.signature.parameters.push_back({
            tokens[tokenIndex].text,
            parameterType.type,
            deepCopy,
            statementColumn + tokens[tokenIndex].span.startColumn - 1
        });
        ++tokenIndex;

        if (tokenIndex < tokens.size() && tokens[tokenIndex].kind == TokenKind::Comma) {
            ++tokenIndex;
            if (tokenIndex < tokens.size() && tokens[tokenIndex].kind == TokenKind::RightParen) {
                recordSourceError(inputFile, lineNumber, statementColumn + tokens[tokenIndex - 1].span.startColumn - 1, "expected parameter after ','", sourceLines);
                result.ok = false;
                return result;
            }
            continue;
        }
        break;
    }

    if (tokenIndex >= tokens.size() || tokens[tokenIndex].kind != TokenKind::RightParen) {
        recordSourceError(inputFile, lineNumber, result.nameColumn, "unclosed parenthesis in function declaration", sourceLines);
        result.ok = false;
        return result;
    }
    ++tokenIndex;

    if (tokenIndex >= tokens.size() || tokens[tokenIndex].text != "{") {
        return result;
    }
    ++tokenIndex;
    if (tokenIndex < tokens.size() && tokens[tokenIndex].kind != TokenKind::EndOfFile) {
        return result;
    }

    const std::string returnCppType = result.signature.returnsVoid ? "void" : cppTypeForType(result.signature.returnType);
    if (returnCppType.empty()) {
        recordSourceError(inputFile, lineNumber, statementColumn, "unsupported function return type " + cpppTypeName(result.signature.returnType), sourceLines);
        result.ok = false;
        return result;
    }

    result.generatedSignature = returnCppType + " " + result.signature.name + "(";
    for (size_t i = 0; i < result.signature.parameters.size(); ++i) {
        if (i > 0) {
            result.generatedSignature += ", ";
        }
        const std::string parameterCppType = cppParameterType(result.signature.parameters[i].type, result.signature.parameters[i].deepCopy);
        if (parameterCppType.empty()) {
            recordSourceError(inputFile, lineNumber, result.signature.parameters[i].column, "unsupported parameter type " + cpppTypeName(result.signature.parameters[i].type), sourceLines);
            result.ok = false;
            return result;
        }
        result.generatedSignature += parameterCppType + " " + result.signature.parameters[i].name;
    }
    result.generatedSignature += ") {";
    return result;
}
