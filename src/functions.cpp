/*
 * functions.cpp
 *
 * Implements function parameter typing and runtime helper selection for function lowering.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "functions.h"

#include "typesCppp.h"

#include <utility>

namespace {
// isReferenceParameterType returns whether the supplied input satisfies the relevant condition.
bool isReferenceParameterType(const Type& type) {
    return isStringType(type) || isCollectionType(type) || isClassType(type);
}

// isCopyParameterEligibleType returns whether the supplied input satisfies the relevant condition.
bool isCopyParameterEligibleType(const Type& type) {
    return isReferenceParameterType(type);
}

// cppParameterType implements the cppParameterType behavior for the functions.cpp module.
std::string cppParameterType(const Type& type, bool copyParameter) {
    const std::string cppType = cppTypeForType(type);
    if (cppType.empty()) {
        return "";
    }
    // CP++ handles aliasing in its value wrappers. Passing the wrapper itself by
    // value keeps ordinary parameters aliased while also giving function types
    // one modifier-free C++ signature. Direct calls to `copy` parameters still
    // wrap the argument in CPPPCopy at the call site.
    (void)copyParameter;
    return cppType;
}

void recordCopyParameterDiagnostic(
    const std::string& inputFile,
    int lineNumber,
    int startColumn,
    int endColumn,
    const std::string& message,
    const std::string& help,
    const std::map<int, std::string>& sourceLines
) {
    Diagnostic diagnostic;
    diagnostic.message = message;
    diagnostic.labels.push_back({
        sourceSpanForColumns(inputFile, sourceLines, lineNumber, startColumn, endColumn),
        "",
        true
    });
    diagnostic.helps.push_back(help);
    recordDiagnostic(std::move(diagnostic));
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
        bool copyParameter = false;
        if (tokens[tokenIndex].kind == TokenKind::Identifier && tokens[tokenIndex].text == "deep") {
            const int startColumn = statementColumn + tokens[tokenIndex].span.startColumn - 1;
            const int endColumn = statementColumn + tokens[tokenIndex].span.endColumn - 1;
            recordCopyParameterDiagnostic(
                inputFile,
                lineNumber,
                startColumn,
                endColumn,
                "deep parameter modifier has been replaced by copy",
                "replace `deep` with `copy`",
                sourceLines
            );
            result.ok = false;
            return result;
        }
        if (tokens[tokenIndex].kind == TokenKind::Identifier && tokens[tokenIndex].text == "copy") {
            copyParameter = true;
            ++tokenIndex;
            if (tokenIndex >= tokens.size() || tokens[tokenIndex].kind == TokenKind::EndOfFile) {
                const int column = statementColumn + tokens[tokenIndex - 1].span.startColumn - 1;
                recordCopyParameterDiagnostic(
                    inputFile,
                    lineNumber,
                    column,
                    column + 3,
                    "copy must precede a collection, string, or class parameter type",
                    "use `copy List<int> values` for an independent List parameter",
                    sourceLines
                );
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
        if (copyParameter && !isCopyParameterEligibleType(parameterType.type)) {
            const int startColumn = statementColumn + tokens[tokenIndex - 1].span.startColumn - 1;
            const int endColumn = statementColumn + tokens[tokenIndex - 1].span.endColumn - 1;
            recordCopyParameterDiagnostic(
                inputFile,
                lineNumber,
                startColumn,
                endColumn,
                "copy must precede a collection, string, or class parameter type",
                "remove `copy` to pass " + cpppTypeName(parameterType.type) + " normally",
                sourceLines
            );
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
            copyParameter,
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
        const std::string parameterCppType = cppParameterType(result.signature.parameters[i].type, result.signature.parameters[i].copyParameter);
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
