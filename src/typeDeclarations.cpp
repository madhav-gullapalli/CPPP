/*
 * typeDeclarations.cpp
 *
 * Parses and validates type declarations and user-defined type syntax.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "typesCppp.h"

#include "tokenizer.h"

#include <cctype>
#include <map>

namespace {
// TypeInfo implements the TypeInfo behavior for the typeDeclarations.cpp module.
struct TypeInfo {
    std::string cppType;
    std::string defaultValue;
};

// DeclaredName implements the DeclaredName behavior for the typeDeclarations.cpp module.
struct DeclaredName {
    std::string name;
    int column;
};

// ParsedTypeName parses dtypename for the compiler pipeline.
struct ParsedTypeName {
    bool ok = true;
    Type type;
    std::string name;
    size_t nextTokenIndex = 0;
    int pendingRightClosers = 0;
};

// trim removes surrounding whitespace from a string.
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// isIdentifier returns whether the supplied input satisfies the relevant condition.
bool isIdentifier(const std::string& text) {
    if (text.empty() || !(std::isalpha(static_cast<unsigned char>(text[0])) || text[0] == '_')) {
        return false;
    }

    for (const char ch : text) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
            return false;
        }
    }

    return true;
}

// isCharLiteral returns whether the supplied input satisfies the relevant condition.
bool isCharLiteral(const std::string& text) {
    return text.size() == 3 && text.front() == '\'' && text.back() == '\'';
}

// isMalformedCharLiteral returns whether the supplied input satisfies the relevant condition.
bool isMalformedCharLiteral(const std::string& text) {
    return !text.empty() && text.front() == '\'';
}

// isIntegerLiteral returns whether the supplied input satisfies the relevant condition.
bool isIntegerLiteral(const std::string& text) {
    if (text.empty()) {
        return false;
    }

    size_t start = 0;
    if (text[0] == '+' || text[0] == '-') {
        if (text.size() == 1) {
            return false;
        }
        start = 1;
    }

    for (size_t i = start; i < text.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
    }

    return true;
}

// fitsLongLong implements the fitsLongLong behavior for the typeDeclarations.cpp module.
bool fitsLongLong(const std::string& text) {
    std::string value = text;
    bool negative = false;
    if (!value.empty() && (value[0] == '+' || value[0] == '-')) {
        negative = value[0] == '-';
        value = value.substr(1);
    }

    const std::string limit = negative ? "9223372036854775808" : "9223372036854775807";
    while (value.size() > 1 && value[0] == '0') {
        value.erase(value.begin());
    }

    return value.size() < limit.size() || (value.size() == limit.size() && value <= limit);
}

// shouldParseAsExpression returns whether the supplied input satisfies the relevant condition.
bool shouldParseAsExpression(const std::vector<Token>& tokens) {
    if (hasArithmeticOperator(tokens)) {
        return true;
    }

    for (const Token& token : tokens) {
        if (token.kind == TokenKind::Identifier ||
            token.kind == TokenKind::LeftParen ||
            token.kind == TokenKind::RightParen ||
            (token.kind == TokenKind::Operator && token.text == ":") ||
            (token.kind == TokenKind::Unknown && (token.text == "{" || token.text == "}"))) {
            return true;
        }
    }

    return false;
}

// nonEndTokenCount implements the nonEndTokenCount behavior for the typeDeclarations.cpp module.
size_t nonEndTokenCount(const std::vector<Token>& tokens) {
    size_t count = 0;
    for (const Token& token : tokens) {
        if (token.kind != TokenKind::EndOfFile) {
            ++count;
        }
    }
    return count;
}

const std::map<std::string, TypeInfo>& primitiveTypes() {
    static const std::map<std::string, TypeInfo> types = {
        {"bool", {"bool", "false"}},
        {"int", {"long long", "0"}},
        {"char", {"CPPPChar", "CPPPChar()"}},
        {"float", {"long double", "0.0L"}},
        {"range", {"CPPPRange", "CPPPRange()"}},
        {"string", {"CPPPList<CPPPChar>", "{}"}},
    };

    return types;
}

// isBoolLiteral returns whether the supplied input satisfies the relevant condition.
bool isBoolLiteral(const std::string& text) {
    return text == "true" || text == "false";
}

// isListType returns whether the supplied input satisfies the relevant condition.
// needsCharRuntimeHelper implements the needsCharRuntimeHelper behavior for the typeDeclarations.cpp module.
bool needsCharRuntimeHelper(const Type& type) {
    if (type == PrimitiveType::Char) {
        return true;
    }

    for (const Type& subtype : type.subtypes) {
        if (needsCharRuntimeHelper(subtype)) {
            return true;
        }
    }

    return false;
}

bool needsRangeRuntimeHelper(const Type& type) {
    if (type == PrimitiveType::Range) {
        return true;
    }

    for (const Type& subtype : type.subtypes) {
        if (needsRangeRuntimeHelper(subtype)) {
            return true;
        }
    }

    return false;
}

// typeInfoFor implements the typeInfoFor behavior for the typeDeclarations.cpp module.
TypeInfo typeInfoFor(const Type& type) {
    if (isListType(type)) {
        const TypeInfo subtypeInfo = typeInfoFor(type.subtypes[0]);
        if (subtypeInfo.cppType.empty()) {
            return {"", ""};
        }
        return {"CPPPList<" + subtypeInfo.cppType + ">", "{}"};
    }

    if (isSetType(type)) {
        const TypeInfo subtypeInfo = typeInfoFor(type.subtypes[0]);
        if (subtypeInfo.cppType.empty()) {
            return {"", ""};
        }
        return {"CPPPSet<" + subtypeInfo.cppType + ">", "{}"};
    }

    if (isMapType(type)) {
        const TypeInfo keyInfo = typeInfoFor(type.subtypes[0]);
        const TypeInfo valueInfo = typeInfoFor(type.subtypes[1]);
        if (keyInfo.cppType.empty() || valueInfo.cppType.empty()) {
            return {"", ""};
        }
        return {"CPPPMap<" + keyInfo.cppType + ", " + valueInfo.cppType + ">", "{}"};
    }

    if (isPairType(type)) {
        const TypeInfo firstInfo = typeInfoFor(type.subtypes[0]);
        const TypeInfo secondInfo = typeInfoFor(type.subtypes[1]);
        if (firstInfo.cppType.empty() || secondInfo.cppType.empty()) {
            return {"", ""};
        }
        return {"CPPPPair<" + firstInfo.cppType + ", " + secondInfo.cppType + ">", "{" + firstInfo.defaultValue + ", " + secondInfo.defaultValue + "}"};
    }

    if (isStructType(type)) {
        return {"cppp_smart_pointer<" + type.name + ">", "nullptr"};
    }

    const auto primitive = primitiveTypes().find(cpppTypeName(type));
    if (primitive != primitiveTypes().end()) {
        return primitive->second;
    }

    return {"", ""};
}

std::string expressionSliceForTokens(
    const std::string& text,
    const std::vector<Token>& tokens,
    size_t startIndex,
    size_t endIndex
) {
    if (startIndex >= endIndex) {
        return "";
    }

    const int startColumn = tokens[startIndex].span.startColumn;
    const int endColumn = tokens[endIndex - 1].span.endColumn;
    return trim(text.substr(
        static_cast<size_t>(startColumn - 1),
        static_cast<size_t>(endColumn - startColumn + 1)
    ));
}

bool emitTypedListLiteralAt(
    const std::string& inputFile,
    int lineNumber,
    const std::string& assignedValue,
    int assignedValueColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::vector<Token>& tokens,
    size_t& tokenIndex,
    const Type& targetType,
    std::string& emittedValue
) {
    if (!isListType(targetType)) {
        return false;
    }

    if (tokenIndex >= tokens.size() || tokens[tokenIndex].kind != TokenKind::LeftBracket) {
        return false;
    }

    const Token& leftBracket = tokens[tokenIndex];
    ++tokenIndex;
    const Type& elementType = targetType.subtypes[0];
    const TypeInfo elementInfo = typeInfoFor(elementType);
    if (elementInfo.cppType.empty()) {
        recordSourceError(
            inputFile,
            lineNumber,
            assignedValueColumn + leftBracket.span.startColumn - 1,
            "unsupported type " + cpppTypeName(elementType),
            sourceLines
        );
        return false;
    }

    std::vector<std::string> elements;
    if (tokenIndex < tokens.size() && tokens[tokenIndex].kind == TokenKind::RightBracket) {
        ++tokenIndex;
        emittedValue = "CPPPList<" + elementInfo.cppType + ">{}";
        return true;
    }

    while (tokenIndex < tokens.size() && tokens[tokenIndex].kind != TokenKind::EndOfFile) {
        std::string emittedElement;
        if (isListType(elementType) && tokens[tokenIndex].kind == TokenKind::LeftBracket) {
            if (!emitTypedListLiteralAt(
                    inputFile,
                    lineNumber,
                    assignedValue,
                    assignedValueColumn,
                    sourceLines,
                    declaredVariables,
                    tokens,
                    tokenIndex,
                    elementType,
                    emittedElement)) {
                return false;
            }
        } else {
            const size_t elementStart = tokenIndex;
            int parenDepth = 0;
            int bracketDepth = 0;
            while (tokenIndex < tokens.size()) {
                const Token& token = tokens[tokenIndex];
                if (token.kind == TokenKind::EndOfFile) {
                    break;
                }
                if (token.kind == TokenKind::LeftParen) {
                    ++parenDepth;
                } else if (token.kind == TokenKind::RightParen) {
                    if (parenDepth > 0) {
                        --parenDepth;
                    }
                } else if (token.kind == TokenKind::LeftBracket) {
                    ++bracketDepth;
                } else if (token.kind == TokenKind::RightBracket) {
                    if (bracketDepth == 0 && parenDepth == 0) {
                        break;
                    }
                    if (bracketDepth > 0) {
                        --bracketDepth;
                    }
                } else if (token.kind == TokenKind::Comma && parenDepth == 0 && bracketDepth == 0) {
                    break;
                }
                ++tokenIndex;
            }

            const std::string elementText = expressionSliceForTokens(assignedValue, tokens, elementStart, tokenIndex);
            if (elementText.empty()) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn + leftBracket.span.startColumn - 1,
                    "expected expression in list literal",
                    sourceLines
                );
                return false;
            }

            const ExpressionEmitResult elementExpression = emitExpression(
                inputFile,
                lineNumber,
                elementText,
                assignedValueColumn + tokens[elementStart].span.startColumn - 1,
                sourceLines,
                declaredVariables
            );
            if (!elementExpression.ok) {
                return false;
            }

            if (elementExpression.type == PrimitiveType::Unknown) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn + tokens[elementStart].span.startColumn - 1,
                    "list literal elements must have a known CP++ type",
                    sourceLines
                );
                return false;
            }

            if (!elementExpression.explicitCast && !isImplicitlyConvertible(elementExpression.type, elementType)) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn + tokens[elementStart].span.startColumn - 1,
                    "cannot implicitly convert " + cpppTypeName(elementExpression.type) + " to " + cpppTypeName(elementType) + " in list literal",
                    sourceLines
                );
                return false;
            }

            emittedElement = elementExpression.generatedExpression;
            if (!isImplicitlyConvertible(elementExpression.type, elementType) || elementExpression.type != elementType) {
                emittedElement = castExpressionTo(emittedElement, elementExpression.type, elementType);
            }
        }

        elements.push_back(emittedElement);

        if (tokenIndex >= tokens.size()) {
            break;
        }

        if (tokens[tokenIndex].kind == TokenKind::Comma) {
            const Token& comma = tokens[tokenIndex];
            ++tokenIndex;
            if (tokenIndex >= tokens.size() ||
                tokens[tokenIndex].kind == TokenKind::RightBracket ||
                tokens[tokenIndex].kind == TokenKind::EndOfFile) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn + comma.span.startColumn - 1,
                    "expected expression after ',' in list literal",
                    sourceLines
                );
                return false;
            }
            continue;
        }

        if (tokens[tokenIndex].kind == TokenKind::RightBracket) {
            ++tokenIndex;
            emittedValue = "CPPPList<" + elementInfo.cppType + ">{";
            for (size_t i = 0; i < elements.size(); ++i) {
                if (i > 0) {
                    emittedValue += ", ";
                }
                emittedValue += elements[i];
            }
            emittedValue += "}";
            return true;
        }

        break;
    }

    recordSourceError(
        inputFile,
        lineNumber,
        assignedValueColumn + leftBracket.span.startColumn - 1,
        "unclosed bracket in list literal",
        sourceLines
    );
    return false;
}

bool parseTypeAt(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& tokens,
    size_t startIndex,
    const std::map<int, std::string>& sourceLines,
    ParsedTypeName& parsedType
) {
    if (startIndex >= tokens.size() || tokens[startIndex].kind != TokenKind::Identifier) {
        return false;
    }

    const std::string typeName = tokens[startIndex].text;
    if (typeName == "bigint" || typeName == "Bigint" || typeName == "bigfloat" || typeName == "BigFloat") {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[startIndex].span.startColumn,
            typeName + " has been removed from CP++; use int or float instead",
            sourceLines
        );
        parsedType.ok = false;
        parsedType.nextTokenIndex = startIndex + 1;
        return true;
    }

    const Type rootType = declaredTypeForName(typeName);
    if (rootType == PrimitiveType::Unknown) {
        return false;
    }

    parsedType.ok = true;
    parsedType.type = rootType;
    parsedType.name = typeName;
    parsedType.nextTokenIndex = startIndex + 1;

    if (typeName == "string") {
        if (tokens.size() > startIndex + 1 &&
            tokens[startIndex + 1].kind == TokenKind::Operator &&
            tokens[startIndex + 1].text == "<") {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[startIndex + 1].span.startColumn,
                "string expects 0 subtypes",
                sourceLines
            );
            parsedType.ok = false;
        }

        return true;
    }

    const int arity = primitiveArity(rootType.primitive);
    if (arity == 0) {
        if (tokens.size() > startIndex + 1 &&
            tokens[startIndex + 1].kind == TokenKind::Operator &&
            tokens[startIndex + 1].text == "<") {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[startIndex + 1].span.startColumn,
                typeName + " expects 0 subtypes",
                sourceLines
            );
            parsedType.ok = false;
        }

        return true;
    }

    const std::string expectedSubtypeExample = arity == 1
        ? typeName + "<int>"
        : typeName + "<int, int>";

    if (parsedType.nextTokenIndex >= tokens.size() ||
        tokens[parsedType.nextTokenIndex].kind != TokenKind::Operator ||
        tokens[parsedType.nextTokenIndex].text != "<") {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[startIndex].span.startColumn,
            typeName + " expects " + std::to_string(arity) + " subtype" + (arity == 1 ? "" : "s") + " like " + expectedSubtypeExample,
            sourceLines
        );
        parsedType.ok = false;
        return true;
    }

    ++parsedType.nextTokenIndex;
    if (parsedType.nextTokenIndex >= tokens.size() ||
        (tokens[parsedType.nextTokenIndex].kind == TokenKind::Operator &&
         tokens[parsedType.nextTokenIndex].text == ">")) {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[startIndex].span.startColumn,
            typeName + " expects " + std::to_string(arity) + " subtype" + (arity == 1 ? "" : "s") + " like " + expectedSubtypeExample,
            sourceLines
        );
        parsedType.ok = false;
        return true;
    }

    for (int subtypeIndex = 0; subtypeIndex < arity; ++subtypeIndex) {
        if (parsedType.nextTokenIndex >= tokens.size() ||
            (tokens[parsedType.nextTokenIndex].kind == TokenKind::Operator &&
             (tokens[parsedType.nextTokenIndex].text == ">" || tokens[parsedType.nextTokenIndex].text == ">>"))) {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[startIndex].span.startColumn,
                typeName + " expects " + std::to_string(arity) + " subtype" + (arity == 1 ? "" : "s") + " like " + expectedSubtypeExample,
                sourceLines
            );
            parsedType.ok = false;
            return true;
        }

        ParsedTypeName subtype;
        if (!parseTypeAt(inputFile, lineNumber, tokens, parsedType.nextTokenIndex, sourceLines, subtype)) {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[parsedType.nextTokenIndex].span.startColumn,
                "expected type inside " + typeName + "<...>",
                sourceLines
            );
            parsedType.ok = false;
            return true;
        }
        if (!subtype.ok) {
            parsedType.ok = false;
            parsedType.nextTokenIndex = subtype.nextTokenIndex;
            parsedType.pendingRightClosers = subtype.pendingRightClosers;
            return true;
        }

        parsedType.type.subtypes.push_back(subtype.type);
        parsedType.nextTokenIndex = subtype.nextTokenIndex;

        if (subtype.pendingRightClosers > 0) {
            if (subtypeIndex != arity - 1) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    tokens[startIndex].span.startColumn,
                    typeName + " expects " + std::to_string(arity) + " subtype" + (arity == 1 ? "" : "s") + " like " + expectedSubtypeExample,
                    sourceLines
                );
                parsedType.ok = false;
                return true;
            }
            parsedType.pendingRightClosers = subtype.pendingRightClosers - 1;
            return true;
        }

        if (subtypeIndex != arity - 1) {
            if (parsedType.nextTokenIndex >= tokens.size()) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    tokens[startIndex].span.startColumn,
                    "unclosed generic type for " + typeName,
                    sourceLines
                );
                parsedType.ok = false;
                return true;
            }

            if (tokens[parsedType.nextTokenIndex].kind != TokenKind::Comma) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    tokens[startIndex].span.startColumn,
                    typeName + " expects " + std::to_string(arity) + " subtype" + (arity == 1 ? "" : "s") + " like " + expectedSubtypeExample,
                    sourceLines
                );
                parsedType.ok = false;
                return true;
            }

            ++parsedType.nextTokenIndex;
        }
    }

    if (parsedType.nextTokenIndex >= tokens.size()) {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[startIndex].span.startColumn,
            "unclosed generic type for " + typeName,
            sourceLines
        );
        parsedType.ok = false;
        return true;
    }

    if (tokens[parsedType.nextTokenIndex].kind == TokenKind::Comma) {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[parsedType.nextTokenIndex].span.startColumn,
            typeName + " expects " + std::to_string(arity) + " subtype" + (arity == 1 ? "" : "s"),
            sourceLines
        );
        parsedType.ok = false;
        return true;
    }

    if (tokens[parsedType.nextTokenIndex].kind != TokenKind::Operator) {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[startIndex].span.startColumn,
            "unclosed generic type for " + typeName,
            sourceLines
        );
        parsedType.ok = false;
        return true;
    }

    if (tokens[parsedType.nextTokenIndex].text == ">") {
        ++parsedType.nextTokenIndex;
        return true;
    }

    if (tokens[parsedType.nextTokenIndex].text == ">>") {
        ++parsedType.nextTokenIndex;
        parsedType.pendingRightClosers = 1;
        return true;
    }

    recordSourceError(
        inputFile,
        lineNumber,
        tokens[parsedType.nextTokenIndex].span.startColumn,
        "expected '>' to close " + typeName + "<...>",
        sourceLines
    );
    parsedType.ok = false;
    return true;
}

[[maybe_unused]] bool parseTypeName(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& tokens,
    const std::map<int, std::string>& sourceLines,
    ParsedTypeName& parsedType
) {
    if (!parseTypeAt(inputFile, lineNumber, tokens, 0, sourceLines, parsedType)) {
        return false;
    }

    if (parsedType.ok && parsedType.pendingRightClosers > 0) {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[0].span.startColumn,
            "unexpected '>' after type " + cpppTypeName(parsedType.type),
            sourceLines
        );
        parsedType.ok = false;
    }

    return true;
}

// isVoidTypeToken returns whether the supplied input satisfies the relevant condition.
bool isVoidTypeToken(const std::vector<Token>& tokens, size_t startIndex) {
    return startIndex < tokens.size() &&
        tokens[startIndex].kind == TokenKind::Identifier &&
        tokens[startIndex].text == "void";
}

bool finishExpressionAssignment(
    const std::string& inputFile,
    int lineNumber,
    const std::string& assignedValue,
    int assignedValueColumn,
    Type targetType,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    std::string& emittedValue
) {
    const ExpressionEmitResult expression = emitExpression(
        inputFile,
        lineNumber,
        assignedValue,
        assignedValueColumn,
        sourceLines,
        declaredVariables
    );
    if (!expression.ok) {
        return false;
    }

    if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, targetType)) {
        recordSourceError(
            inputFile,
            lineNumber,
            assignedValueColumn,
            "cannot implicitly convert " + cpppTypeName(expression.type) + " to " + cpppTypeName(targetType),
            sourceLines
        );
        return false;
    }

    if (expression.explicitCast &&
        expression.type != targetType &&
        !canExplicitlyCastType(expression.type, targetType)) {
        recordSourceError(
            inputFile,
            lineNumber,
            assignedValueColumn,
            "cannot cast " + cpppTypeName(expression.type) + " to " + cpppTypeName(targetType),
            sourceLines
        );
        return false;
    }

    emittedValue = expression.generatedExpression;
    if (!isImplicitlyConvertible(expression.type, targetType) || expression.type != targetType) {
        emittedValue = castExpressionTo(emittedValue, expression.type, targetType);
    }

    return true;
}

std::vector<std::pair<std::string, int>> splitTopLevelCommaValues(
    const std::string& text,
    int startColumn
) {
    std::vector<std::pair<std::string, int>> values;
    const std::vector<Token> tokens = tokenize(text);
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;
    size_t startIndex = 0;
    int valueColumn = startColumn;

    for (const Token& token : tokens) {
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
            std::string value = trim(text.substr(startIndex, static_cast<size_t>(token.span.startColumn - 1) - startIndex));
            values.push_back({value, valueColumn});
            startIndex = static_cast<size_t>(token.span.endColumn);
            valueColumn = startColumn + token.span.endColumn;
        }
    }

    values.push_back({trim(text.substr(startIndex)), valueColumn});
    return values;
}

bool isVarDeclaration(const std::vector<Token>& tokens) {
    return tokens.size() >= 2 &&
        tokens[0].kind == TokenKind::Identifier &&
        tokens[0].text == "var";
}

bool isEmptyContainerLiteral(const std::string& text) {
    return text == "[]" || text == "{}";
}

void rememberInvalidVariable(
    std::map<std::string, Type>& declaredVariables,
    const std::string& variableName
) {
    if (!variableName.empty() && declaredVariables.count(variableName) == 0) {
        declaredVariables[variableName] = PrimitiveType::Unknown;
    }
}

void rememberInvalidVariables(
    std::map<std::string, Type>& declaredVariables,
    const std::vector<DeclaredName>& variables
) {
    for (const DeclaredName& variable : variables) {
        rememberInvalidVariable(declaredVariables, variable.name);
    }
}
}

TypeEmitResult emitTypeDeclaration(
    const std::string& inputFile,
    int lineNumber,
    const std::string& sourceLine,
    const std::string& statementBody,
    int statementStartColumn,
    const std::map<int, std::string>& sourceLines,
    std::map<std::string, Type>& declaredVariables
) {
    (void)sourceLine;
    const auto sourceColumn = [statementStartColumn](int tokenColumn) { return statementStartColumn + tokenColumn - 1; };

    const std::vector<Token> tokens = tokenize(statementBody);
    if (tokens.size() < 2 || tokens[0].kind != TokenKind::Identifier) {
        return {false, true, "", {}};
    }

    if (isVarDeclaration(tokens)) {
        if (tokens.size() < 3 || tokens[1].kind != TokenKind::Identifier) {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[0].span.startColumn,
                "var declarations require a variable name",
                sourceLines
            );
            return {true, false, "", {}};
        }

        const std::string variableName = tokens[1].text;
        const int variableColumn = tokens[1].span.startColumn;
        if (!isIdentifier(variableName)) {
            recordSourceError(
                inputFile,
                lineNumber,
                sourceColumn(variableColumn),
                "var declarations require a variable name",
                sourceLines
            );
            return {true, false, "", {}};
        }

        if (declaredVariables.count(variableName) != 0) {
            recordSourceError(
                inputFile,
                lineNumber,
                sourceColumn(variableColumn),
                "variable '" + variableName + "' is already declared",
                sourceLines
            );
            return {true, false, "", {}};
        }

        if (tokens[2].kind == TokenKind::Comma) {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[2].span.startColumn,
                "var declarations support exactly one variable",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        if (tokens[2].kind == TokenKind::EndOfFile) {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[1].span.endColumn + 1,
                "var declarations require an initializer so the type can be inferred",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        if (tokens[2].kind != TokenKind::Equals) {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[2].span.startColumn,
                "var declarations must use '=' with an initializer",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        if (tokens[3].kind == TokenKind::EndOfFile) {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[2].span.endColumn + 1,
                "var declarations require an initializer so the type can be inferred",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        const int assignedValueColumn = tokens[3].span.startColumn;
        const int assignedValueStartColumn = tokens[3].span.startColumn;
        int assignedValueEndColumn = tokens[3].span.endColumn;
        size_t tokenIndex = 3;
        while (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
            assignedValueEndColumn = tokens[tokenIndex].span.endColumn;
            ++tokenIndex;
        }

        const std::string assignedValue = trim(statementBody.substr(
            static_cast<size_t>(assignedValueStartColumn - 1),
            static_cast<size_t>(assignedValueEndColumn - assignedValueStartColumn + 1)
        ));

        std::vector<InputArgument> inputArguments;
        if (parseInputCall(assignedValue, assignedValueColumn, inputArguments)) {
            recordSourceError(
                inputFile,
                lineNumber,
                assignedValueColumn,
                "var cannot be initialized with input(); declare the type explicitly",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        if (isEmptyContainerLiteral(assignedValue)) {
            recordSourceError(
                inputFile,
                lineNumber,
                assignedValueColumn,
                "var cannot infer a type from an empty container; declare the type explicitly",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        const ExpressionEmitResult expression = emitExpression(
            inputFile,
            lineNumber,
            assignedValue,
            assignedValueColumn,
            sourceLines,
            declaredVariables
        );
        if (!expression.ok) {
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        if (expression.type == PrimitiveType::Unknown) {
            recordSourceError(
                inputFile,
                lineNumber,
                assignedValueColumn,
                "var could not infer a concrete type from this expression",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        if (expression.type == PrimitiveType::Void) {
            recordSourceError(
                inputFile,
                lineNumber,
                assignedValueColumn,
                "var cannot infer a type from a void expression",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        const Type inferredType = expression.type;
        const TypeInfo inferredInfo = typeInfoFor(inferredType);
        if (inferredInfo.cppType.empty()) {
            recordSourceError(
                inputFile,
                lineNumber,
                assignedValueColumn,
                "unsupported inferred type " + cpppTypeName(inferredType),
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        declaredVariables[variableName] = inferredType;
        if (needsCharRuntimeHelper(inferredType)) {
            requireRuntimeHelper("CPPPCharType");
        }
        if (needsRangeRuntimeHelper(inferredType)) {
            requireRuntimeHelper("CPPPRangeType");
        }

        const std::string generatedStatement = "    " + inferredInfo.cppType + " " + variableName + " = " + expression.generatedExpression + ";";
        return {
            true,
            true,
            generatedStatement,
            {{
                lineNumber,
                sourceColumn(variableColumn),
                5 + static_cast<int>(inferredInfo.cppType.size()) + 1,
                5 + static_cast<int>(inferredInfo.cppType.size()) + static_cast<int>(variableName.size())
            }}
        };
    }

    const ParsedTypeResult parsedType = parseDeclaredTypeTokens(inputFile, lineNumber, tokens, 0, sourceLines);
    if (!parsedType.matched) {
        return {false, true, "", {}};
    }
    if (!parsedType.ok) {
        return {true, false, "", {}};
    }

    const std::string typeName = parsedType.name;
    const Type targetType = parsedType.type;
    if (targetType == PrimitiveType::Void) {
        recordSourceError(inputFile, lineNumber, tokens[0].span.startColumn, "variables cannot have void type", sourceLines);
        return {true, false, "", {}};
    }
    const TypeInfo typeInfo = typeInfoFor(targetType);
    if (typeInfo.cppType.empty()) {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[0].span.startColumn,
            "unsupported type " + cpppTypeName(targetType),
            sourceLines
        );
        return {true, false, "", {}};
    }

    std::vector<DeclaredName> variables;
    size_t tokenIndex = parsedType.nextTokenIndex;
    while (true) {
        if (tokens[tokenIndex].kind != TokenKind::Identifier) {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[tokenIndex].span.startColumn,
                "expected variable name after " + typeName,
                sourceLines
            );
            return {true, false, "", {}};
        }

        const std::string variableName = tokens[tokenIndex].text;
        const int variableColumn = tokens[tokenIndex].span.startColumn;
        if (!isIdentifier(variableName)) {
            recordSourceError(
                inputFile,
                lineNumber,
                sourceColumn(variableColumn),
                "expected variable name after " + typeName,
                sourceLines
            );
            return {true, false, "", {}};
        }

        if (declaredVariables.count(variableName) != 0) {
            recordSourceError(
                inputFile,
                lineNumber,
                variableColumn,
                "variable '" + variableName + "' is already declared",
                sourceLines
            );
            return {true, false, "", {}};
        }

        for (const DeclaredName& variable : variables) {
            if (variable.name == variableName) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    variableColumn,
                    "variable '" + variableName + "' is already declared",
                    sourceLines
                );
                return {true, false, "", {}};
            }
        }

        variables.push_back({variableName, variableColumn});
        ++tokenIndex;

        if (tokens[tokenIndex].kind != TokenKind::Comma) {
            break;
        }

        ++tokenIndex;
        if (tokens[tokenIndex].kind == TokenKind::EndOfFile) {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[tokenIndex - 1].span.endColumn + 1,
                "expected variable name after ','",
                sourceLines
            );
            rememberInvalidVariables(declaredVariables, variables);
            return {true, false, "", {}};
        }
    }

    std::string assignedValue;
    int assignedValueColumn = 1;
    if (tokens[tokenIndex].kind == TokenKind::Equals) {
        ++tokenIndex;
        if (tokens[tokenIndex].kind == TokenKind::EndOfFile) {
            recordSourceError(
                inputFile,
                lineNumber,
                tokens[tokenIndex - 1].span.endColumn + 1,
                "expected value after '='",
                sourceLines
            );
            rememberInvalidVariables(declaredVariables, variables);
            return {true, false, "", {}};
        }

        assignedValueColumn = tokens[tokenIndex].span.startColumn;
        const int assignedValueStartColumn = tokens[tokenIndex].span.startColumn;
        int assignedValueEndColumn = tokens[tokenIndex].span.endColumn;
        while (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
            assignedValueEndColumn = tokens[tokenIndex].span.endColumn;
            ++tokenIndex;
        }

        assignedValue = trim(statementBody.substr(
            static_cast<size_t>(assignedValueStartColumn - 1),
            static_cast<size_t>(assignedValueEndColumn - assignedValueStartColumn + 1)
        ));
    } else if (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[tokenIndex].span.startColumn,
            "expected ';' or '=' after variable name",
            sourceLines
        );
        rememberInvalidVariables(declaredVariables, variables);
        return {true, false, "", {}};
    }

    std::string emittedValue = typeInfo.defaultValue;
    std::vector<std::string> perVariableValues;
    if (!assignedValue.empty()) {
        emittedValue = assignedValue;
        const std::vector<Token> valueTokens = tokenize(assignedValue);

        std::vector<InputArgument> inputArguments;
        if (parseInputCall(assignedValue, assignedValueColumn, inputArguments)) {
            if (!emitInputCallForType(
                    inputFile,
                    lineNumber,
                    assignedValue,
                    assignedValueColumn,
                    targetType,
                    sourceLines,
                    declaredVariables,
                    emittedValue)) {
                return {true, false, "", {}};
            }
        } else if (variables.size() > 1 && splitTopLevelCommaValues(assignedValue, assignedValueColumn).size() > 1) {
            const std::vector<std::pair<std::string, int>> values = splitTopLevelCommaValues(assignedValue, assignedValueColumn);
            if (values.size() > 1) {
                if (values.size() != variables.size()) {
                    recordSourceError(
                        inputFile,
                        lineNumber,
                        assignedValueColumn,
                        "multi-declaration requires the same number of values as variables",
                        sourceLines
                    );
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }

                for (const auto& value : values) {
                    if (value.first.empty()) {
                        recordSourceError(inputFile, lineNumber, value.second, "expected value after ','", sourceLines);
                        rememberInvalidVariables(declaredVariables, variables);
                        return {true, false, "", {}};
                    }

                    std::string emitted;
                    if (!finishExpressionAssignment(
                            inputFile,
                            lineNumber,
                            value.first,
                            value.second,
                            targetType,
                            sourceLines,
                            declaredVariables,
                            emitted)) {
                        rememberInvalidVariables(declaredVariables, variables);
                        return {true, false, "", {}};
                    }
                    perVariableValues.push_back(emitted);
                }
            } else if (targetType == PrimitiveType::Char) {
                if (isCharLiteral(assignedValue)) {
                    emittedValue = "CPPPChar(" + assignedValue + ")";
                } else {
                    if (!finishExpressionAssignment(
                            inputFile,
                            lineNumber,
                            assignedValue,
                            assignedValueColumn,
                            targetType,
                            sourceLines,
                            declaredVariables,
                            emittedValue)) {
                        rememberInvalidVariables(declaredVariables, variables);
                        return {true, false, "", {}};
                    }
                }
            }
        } else if (targetType == PrimitiveType::Char) {
            if (isCharLiteral(assignedValue)) {
                emittedValue = "CPPPChar(" + assignedValue + ")";
            } else if (isMalformedCharLiteral(assignedValue)) {
                std::string message = "char values must be a single character in single quotes";
                const size_t closingQuote = assignedValue.find('\'', 1);
                if (closingQuote != std::string::npos && closingQuote + 1 < assignedValue.size()) {
                    message = "unexpected characters after char literal";
                } else if (!assignedValue.empty() && assignedValue.back() == '\'') {
                    message = "char literal contains too many characters";
                } else {
                    message = "unterminated char literal";
                }

                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn,
                    message,
                    sourceLines
                );
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            } else {
                if (!finishExpressionAssignment(
                        inputFile,
                        lineNumber,
                        assignedValue,
                        assignedValueColumn,
                        targetType,
                        sourceLines,
                        declaredVariables,
                        emittedValue)) {
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }
            }
        } else if (targetType == PrimitiveType::Bool) {
            if (isBoolLiteral(assignedValue)) {
                emittedValue = assignedValue;
            } else if (shouldParseAsExpression(valueTokens) || nonEndTokenCount(valueTokens) > 1) {
                if (!finishExpressionAssignment(
                        inputFile,
                        lineNumber,
                        assignedValue,
                        assignedValueColumn,
                        targetType,
                        sourceLines,
                        declaredVariables,
                        emittedValue)) {
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }
            } else {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn,
                    "bool requires true or false",
                    sourceLines
                );
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
        } else if (isListType(targetType)) {
            size_t listTokenIndex = 0;
            if (valueTokens.size() > 1 && valueTokens[0].kind == TokenKind::LeftBracket) {
                if (!emitTypedListLiteralAt(
                        inputFile,
                        lineNumber,
                        assignedValue,
                        assignedValueColumn,
                        sourceLines,
                        declaredVariables,
                        valueTokens,
                        listTokenIndex,
                        targetType,
                        emittedValue)) {
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }

                if (valueTokens[listTokenIndex].kind != TokenKind::EndOfFile) {
                    recordSourceError(
                        inputFile,
                        lineNumber,
                        assignedValueColumn + valueTokens[listTokenIndex].span.startColumn - 1,
                        "unexpected token in list literal",
                        sourceLines
                    );
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }
            } else if (valueTokens.size() > 1 &&
                       valueTokens[0].kind == TokenKind::String &&
                       isStringType(targetType)) {
                if (!finishExpressionAssignment(
                        inputFile,
                        lineNumber,
                        assignedValue,
                        assignedValueColumn,
                        targetType,
                        sourceLines,
                        declaredVariables,
                        emittedValue)) {
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }
            } else if (shouldParseAsExpression(valueTokens)) {
                if (!finishExpressionAssignment(
                        inputFile,
                        lineNumber,
                        assignedValue,
                        assignedValueColumn,
                        targetType,
                        sourceLines,
                        declaredVariables,
                        emittedValue)) {
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }
            } else {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn,
                    "List values must use a list literal like [1, 2] or another List expression",
                    sourceLines
                );
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
        } else if (isSetType(targetType) || isMapType(targetType) || isPairType(targetType)) {
            const bool isSet = isSetType(targetType);
            const bool isMap = isMapType(targetType);
            const std::string typeLabel = isSet ? "Set" : (isMap ? "Map" : "Pair");
            const std::string literalHint = isSet
                ? "{1, 2}"
                : (isMap ? "{1:'a'}" : "(1,2)");

            if ((isSet || isMap) &&
                valueTokens.size() > 2 &&
                valueTokens[0].kind == TokenKind::Unknown &&
                valueTokens[0].text == "{" &&
                valueTokens[1].kind == TokenKind::Unknown &&
                valueTokens[1].text == "}") {
                emittedValue = typeInfo.defaultValue;
            } else if ((isSet || isMap) &&
                       valueTokens.size() > 1 &&
                       valueTokens[0].kind == TokenKind::LeftBracket) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn,
                    typeLabel + " values must use a " + std::string(isSet ? "set" : "map") + " literal like " + literalHint + " or another " + typeLabel + " expression",
                    sourceLines
                );
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            } else if (shouldParseAsExpression(valueTokens)) {
                if (!finishExpressionAssignment(
                        inputFile,
                        lineNumber,
                        assignedValue,
                        assignedValueColumn,
                        targetType,
                        sourceLines,
                        declaredVariables,
                        emittedValue)) {
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }
            } else {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn,
                    typeLabel + " values must use a " + std::string(isSet ? "set" : (isMap ? "map" : "pair")) + " literal like " + literalHint + " or another " + typeLabel + " expression",
                    sourceLines
                );
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
        } else if (isStructType(targetType)) {
            if (!finishExpressionAssignment(
                    inputFile,
                    lineNumber,
                    assignedValue,
                    assignedValueColumn,
                    targetType,
                    sourceLines,
                    declaredVariables,
                    emittedValue)) {
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
        } else if (targetType == PrimitiveType::Range) {
            if (!finishExpressionAssignment(
                    inputFile,
                    lineNumber,
                    assignedValue,
                    assignedValueColumn,
                    targetType,
                    sourceLines,
                    declaredVariables,
                    emittedValue)) {
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
        } else if (targetType == PrimitiveType::Int) {
            if (shouldParseAsExpression(valueTokens) || nonEndTokenCount(valueTokens) > 1) {
                if (!finishExpressionAssignment(
                        inputFile,
                        lineNumber,
                        assignedValue,
                        assignedValueColumn,
                        targetType,
                        sourceLines,
                        declaredVariables,
                        emittedValue)) {
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }
            } else {
                if (!isIntegerLiteral(assignedValue)) {
                    recordSourceError(
                        inputFile,
                        lineNumber,
                        assignedValueColumn,
                        "int requires an integer literal",
                        sourceLines
                    );
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }

                if (!fitsLongLong(assignedValue)) {
                    recordSourceError(
                        inputFile,
                        lineNumber,
                        assignedValueColumn,
                        "integer literal overflows CP++ int",
                        sourceLines
                    );
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }
            }
        } else if (targetType == PrimitiveType::Float) {
            if (shouldParseAsExpression(valueTokens) || nonEndTokenCount(valueTokens) > 1) {
                if (!finishExpressionAssignment(
                        inputFile,
                        lineNumber,
                        assignedValue,
                        assignedValueColumn,
                        targetType,
                        sourceLines,
                        declaredVariables,
                        emittedValue)) {
                    rememberInvalidVariables(declaredVariables, variables);
                    return {true, false, "", {}};
                }
            } else if (assignedValue.find('.') != std::string::npos && assignedValue.find_first_of("lL") == std::string::npos) {
                emittedValue += "L";
            }
        }
    }

    std::string generatedStatement = "    " + typeInfo.cppType + " ";
    std::vector<SourceRange> ranges;
    for (size_t i = 0; i < variables.size(); ++i) {
        if (i > 0) {
            generatedStatement += ", ";
        }

        const int generatedStartColumn = static_cast<int>(generatedStatement.size()) + 1;
        const std::string initializer = !perVariableValues.empty() ? perVariableValues[i] : emittedValue;
        generatedStatement += variables[i].name + " = " + initializer;
        ranges.push_back({
            lineNumber,
            variables[i].column,
            generatedStartColumn,
            generatedStartColumn + static_cast<int>(variables[i].name.size()) - 1
        });
    }
    generatedStatement += ";";

    for (const DeclaredName& variable : variables) {
        declaredVariables[variable.name] = targetType;
    }

    if (needsCharRuntimeHelper(targetType)) {
        requireRuntimeHelper("CPPPCharType");
    }
    if (needsRangeRuntimeHelper(targetType)) {
        requireRuntimeHelper("CPPPRangeType");
    }

    return {
        true,
        true,
        generatedStatement,
        ranges
    };
}

// cppTypeForType implements the cppTypeForType behavior for the typeDeclarations.cpp module.
std::string cppTypeForType(const Type& type) {
    return typeInfoFor(type).cppType;
}

ParsedTypeResult parseDeclaredTypeTokens(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& tokens,
    size_t startIndex,
    const std::map<int, std::string>& sourceLines,
    bool allowVoid
) {
    ParsedTypeResult result;
    if (allowVoid && isVoidTypeToken(tokens, startIndex)) {
        result.matched = true;
        result.ok = true;
        result.type = PrimitiveType::Void;
        result.name = "void";
        result.nextTokenIndex = startIndex + 1;
        return result;
    }

    ParsedTypeName parsedType;
    if (!parseTypeAt(inputFile, lineNumber, tokens, startIndex, sourceLines, parsedType)) {
        return result;
    }

    result.matched = true;
    result.ok = parsedType.ok;
    result.type = parsedType.type;
    result.name = parsedType.name;
    result.nextTokenIndex = parsedType.nextTokenIndex;

    if (result.ok && parsedType.pendingRightClosers > 0) {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[startIndex].span.startColumn,
            "unexpected '>' after type " + cpppTypeName(parsedType.type),
            sourceLines
        );
        result.ok = false;
    }

    return result;
}
