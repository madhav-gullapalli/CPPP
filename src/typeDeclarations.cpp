/*
 * typeDeclarations.cpp
 *
 * Parses and validates type declarations and user-defined type syntax.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "typesCppp.h"

#include "listsCppp.h"
#include "tokenizer.h"

#include <algorithm>
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
            token.kind == TokenKind::LeftBrace || token.kind == TokenKind::RightBrace) {
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
    if (isFunctionType(type)) {
        const TypeInfo returnInfo = typeInfoFor(type.subtypes[0]);
        if (returnInfo.cppType.empty() && type.subtypes[0] != PrimitiveType::Void) return {"", ""};
        std::string signature = type.subtypes[0] == PrimitiveType::Void ? "void" : returnInfo.cppType;
        signature += "(";
        for (size_t i = 1; i < type.subtypes.size(); ++i) {
            const TypeInfo parameterInfo = typeInfoFor(type.subtypes[i]);
            if (parameterInfo.cppType.empty()) return {"", ""};
            if (i > 1) signature += ", ";
            signature += parameterInfo.cppType;
        }
        signature += ")";
        requireRuntimeHelper("CPPPFunctionType");
        return {"CPPPFunction<" + signature + ">", "{}"};
    }
    if (isListType(type)) {
        const TypeInfo subtypeInfo = typeInfoFor(type.subtypes[0]);
        if (subtypeInfo.cppType.empty()) {
            return {"", ""};
        }
        return {"CPPPList<" + subtypeInfo.cppType + ">", "{}"};
    }

    if (isStackType(type)) {
        const TypeInfo subtypeInfo = typeInfoFor(type.subtypes[0]);
        return subtypeInfo.cppType.empty()
            ? TypeInfo{"", ""}
            : TypeInfo{"CPPPStack<" + subtypeInfo.cppType + ">", "{}"};
    }

    if (isQueueType(type)) {
        const TypeInfo subtypeInfo = typeInfoFor(type.subtypes[0]);
        return subtypeInfo.cppType.empty()
            ? TypeInfo{"", ""}
            : TypeInfo{"CPPPQueue<" + subtypeInfo.cppType + ">", "{}"};
    }

    if (isDequeType(type)) {
        const TypeInfo subtypeInfo = typeInfoFor(type.subtypes[0]);
        return subtypeInfo.cppType.empty()
            ? TypeInfo{"", ""}
            : TypeInfo{"CPPPDeque<" + subtypeInfo.cppType + ">", "{}"};
    }

    if (isHeapType(type)) {
        const TypeInfo subtypeInfo = typeInfoFor(type.subtypes[0]);
        return subtypeInfo.cppType.empty() ? TypeInfo{"", ""} : TypeInfo{"CPPPHeap<" + subtypeInfo.cppType + ">", "{}"};
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

    if (isClassType(type)) {
        return {"cppp_smart_pointer<" + type.name + ">", "nullptr"};
    }

    if (isInlineStructType(type)) {
        return {type.name, type.name + "()"};
    }

    const auto primitive = primitiveTypes().find(cpppTypeName(type));
    if (primitive != primitiveTypes().end()) {
        return primitive->second;
    }

    return {"", ""};
}

std::vector<Token> tokenRange(
    const std::vector<Token>& tokens,
    size_t startIndex,
    size_t endIndex
) {
    std::vector<Token> result;
    if (startIndex >= endIndex || startIndex >= tokens.size()) return result;
    const int startColumn = tokens[startIndex].span.startColumn;
    const size_t startOffset = tokens[startIndex].span.startOffset;
    for (size_t index = startIndex; index < endIndex && index < tokens.size(); ++index) {
        Token token = tokens[index];
        token.span.startColumn -= startColumn - 1;
        token.span.endColumn -= startColumn - 1;
        token.span.startOffset -= startOffset;
        token.span.endOffset -= startOffset;
        result.push_back(std::move(token));
        if (tokens[index].kind == TokenKind::EndOfFile) break;
    }
    return result;
}

std::string tokenText(const std::vector<Token>& tokens) {
    if (tokens.empty()) return "";
    std::string result;
    size_t previousEnd = tokens.front().span.startOffset;
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::EndOfFile) break;
        if (token.span.startOffset > previousEnd) {
            result.append(token.span.startOffset - previousEnd, ' ');
        }
        result += token.text;
        previousEnd = token.span.endOffset;
    }
    return result;
}

bool emitTypedListLiteralAt(
    const std::string& inputFile,
    int lineNumber,
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

            const std::vector<Token> elementTokens = tokenRange(tokens, elementStart, tokenIndex);
            if (elementTokens.empty()) {
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
                elementTokens,
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

void recordConversionDiagnostic(
    const std::string& inputFile,
    int lineNumber,
    int expressionColumn,
    const std::string& expressionText,
    const std::string& message,
    Type sourceType,
    Type targetType,
    const std::map<int, std::string>& sourceLines
) {
    Diagnostic diagnostic;
    diagnostic.message = message;
    diagnostic.labels.push_back({
        sourceSpanForColumns(
            inputFile,
            sourceLines,
            lineNumber,
            expressionColumn,
            expressionColumn + std::max(1, static_cast<int>(expressionText.size())) - 1
        ),
        "",
        true
    });
    if (sourceType != PrimitiveType::Unknown &&
        canExplicitlyCastType(sourceType, targetType)) {
        const std::string replacement =
            cpppTypeName(targetType) + "(" + expressionText + ")";
        diagnostic.suggestions.push_back({
            diagnostic.labels.front().span,
            replacement,
            "use `" + replacement + "` to explicitly convert " +
                cpppTypeName(sourceType) +
                " to " +
                cpppTypeName(targetType),
            SuggestionApplicability::MaybeIncorrect
        });
    }
    recordDiagnostic(std::move(diagnostic));
}

bool finishExpressionAssignment(
    const std::string& inputFile,
    int lineNumber,
    const std::string& assignedValue,
    const std::vector<Token>& assignedValueTokens,
    int assignedValueColumn,
    Type targetType,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    std::string& emittedValue
) {
    const ExpressionEmitResult expression = emitExpression(
        inputFile,
        lineNumber,
        assignedValueTokens,
        assignedValueColumn,
        sourceLines,
        declaredVariables
    );
    if (!expression.ok) {
        return false;
    }

    if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, targetType)) {
        recordConversionDiagnostic(
            inputFile,
            lineNumber,
            assignedValueColumn,
            assignedValue,
            "cannot implicitly convert " + cpppTypeName(expression.type) + " to " + cpppTypeName(targetType),
            expression.type,
            targetType,
            sourceLines
        );
        return false;
    }

    if (expression.explicitCast &&
        expression.type != targetType &&
        !canExplicitlyCastType(expression.type, targetType)) {
        Type diagnosticSourceType = expression.type;
        const size_t tokenCount = nonEndTokenCount(assignedValueTokens);
        if (tokenCount >= 4 &&
            assignedValueTokens[0].kind == TokenKind::Identifier &&
            assignedValueTokens[1].kind == TokenKind::LeftParen &&
            assignedValueTokens[tokenCount - 1].kind == TokenKind::RightParen) {
                const std::vector<Token> operandTokens = tokenRange(assignedValueTokens, 2, tokenCount - 1);
                const ExpressionEmitResult sourceExpression = emitExpression(
                    inputFile,
                    lineNumber,
                    operandTokens,
                    assignedValueColumn + assignedValueTokens[2].span.startColumn - 1,
                    sourceLines,
                    declaredVariables
                );
                if (sourceExpression.ok &&
                    (isLinearDataStructureType(sourceExpression.type) ||
                     isLinearDataStructureType(targetType))) {
                    diagnosticSourceType = sourceExpression.type;
                }
        }
        recordSourceError(
            inputFile,
            lineNumber,
            assignedValueColumn,
            "cannot cast " + cpppTypeName(diagnosticSourceType) + " to " + cpppTypeName(targetType),
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

struct InitializerValue {
    std::string text;
    std::vector<Token> tokens;
    int column;
};

std::vector<InitializerValue> splitTopLevelCommaValues(
    const std::string& text,
    const std::vector<Token>& tokens,
    int startColumn
) {
    std::vector<InitializerValue> values;
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;
    size_t startIndex = 0;
    int valueColumn = startColumn;

    size_t startToken = 0;
    for (size_t tokenIndex = 0; tokenIndex < tokens.size(); ++tokenIndex) {
        const Token& token = tokens[tokenIndex];
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
        } else if (token.kind == TokenKind::LeftBrace) {
            ++braceDepth;
        } else if (token.kind == TokenKind::RightBrace && braceDepth > 0) {
            --braceDepth;
        } else if (token.kind == TokenKind::Comma && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
            std::string value = trim(text.substr(startIndex, static_cast<size_t>(token.span.startColumn - 1) - startIndex));
            values.push_back({value, tokenRange(tokens, startToken, tokenIndex), valueColumn});
            startIndex = static_cast<size_t>(token.span.endColumn);
            startToken = tokenIndex + 1;
            valueColumn = startColumn + token.span.endColumn;
        }
    }

    values.push_back({trim(text.substr(startIndex)), tokenRange(tokens, startToken, nonEndTokenCount(tokens)), valueColumn});
    return values;
}

bool isEmptyContainerLiteral(const std::vector<Token>& tokens) {
    const size_t count = nonEndTokenCount(tokens);
    return count == 2 &&
        ((tokens[0].kind == TokenKind::LeftBracket && tokens[1].kind == TokenKind::RightBracket) ||
         (tokens[0].kind == TokenKind::LeftBrace && tokens[1].kind == TokenKind::RightBrace));
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

void recordListSizeDiagnostic(
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
        sourceSpanForColumns(inputFile, sourceLines, lineNumber, startColumn, std::max(startColumn, endColumn)),
        "",
        true
    });
    if (!help.empty()) diagnostic.helps.push_back(help);
    recordDiagnostic(std::move(diagnostic));
}

int listSizeInitializerDepth(Type type) {
    int depth = 0;
    while (isListType(type) && !type.subtypes.empty()) {
        ++depth;
        type = type.subtypes[0];
    }
    return depth;
}

struct ListSizeInitializerResult {
    bool ok = false;
    size_t nextTokenIndex = 0;
    std::string generatedExpression;
};

ListSizeInitializerResult parseListSizeInitializer(
    const std::string& inputFile,
    int lineNumber,
    int statementStartColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::vector<Token>& tokens,
    size_t openParenIndex,
    const Type& targetType,
    const TypeInfo& typeInfo
) {
    const auto sourceColumn = [statementStartColumn](int tokenColumn) {
        return statementStartColumn + tokenColumn - 1;
    };
    size_t closeParenIndex = tokens.size();
    int depth = 0;
    for (size_t index = openParenIndex; index < tokens.size(); ++index) {
        if (tokens[index].kind == TokenKind::LeftParen) {
            ++depth;
        } else if (tokens[index].kind == TokenKind::RightParen && --depth == 0) {
            closeParenIndex = index;
            break;
        } else if (tokens[index].kind == TokenKind::EndOfFile) {
            break;
        }
    }

    if (closeParenIndex == tokens.size()) {
        const int endColumn = tokens[openParenIndex].span.endColumn;
        recordListSizeDiagnostic(
            inputFile,
            lineNumber,
            sourceColumn(tokens[openParenIndex].span.startColumn),
            sourceColumn(endColumn),
            "unclosed parenthesis in List size initializer",
            "add `)` after the final size",
            sourceLines
        );
        return {};
    }

    if (!isListType(targetType)) {
        recordListSizeDiagnostic(
            inputFile,
            lineNumber,
            sourceColumn(tokens[openParenIndex].span.startColumn),
            sourceColumn(tokens[closeParenIndex].span.endColumn),
            "size initialization is only supported for List and string declarations",
            "declare this container empty, then add its values explicitly",
            sourceLines
        );
        return {};
    }

    if (closeParenIndex == openParenIndex + 1) {
        recordListSizeDiagnostic(
            inputFile,
            lineNumber,
            sourceColumn(tokens[openParenIndex].span.startColumn),
            sourceColumn(tokens[closeParenIndex].span.endColumn),
            "List size initializer requires at least one size",
            "provide an int size, such as `(5)`, or remove the parentheses for an empty List",
            sourceLines
        );
        return {};
    }

    std::vector<std::pair<size_t, size_t>> arguments;
    size_t argumentStart = openParenIndex + 1;
    int nestedDepth = 0;
    for (size_t index = argumentStart; index < closeParenIndex; ++index) {
        if (tokens[index].kind == TokenKind::LeftParen) ++nestedDepth;
        else if (tokens[index].kind == TokenKind::RightParen) --nestedDepth;
        else if (tokens[index].kind == TokenKind::Comma && nestedDepth == 0) {
            arguments.push_back({argumentStart, index});
            argumentStart = index + 1;
        }
    }
    arguments.push_back({argumentStart, closeParenIndex});

    for (const auto& argument : arguments) {
        if (argument.first == argument.second) {
            const int column = sourceColumn(tokens[argument.first == closeParenIndex ? closeParenIndex : argument.first].span.startColumn);
            recordListSizeDiagnostic(
                inputFile,
                lineNumber,
                column,
                column,
                "expected an int size in List size initializer",
                "remove the extra comma or provide the missing size",
                sourceLines
            );
            return {};
        }
    }

    const int supportedDepth = listSizeInitializerDepth(targetType);
    if (static_cast<int>(arguments.size()) > supportedDepth) {
        const auto& extra = arguments[static_cast<size_t>(supportedDepth)];
        recordListSizeDiagnostic(
            inputFile,
            lineNumber,
            sourceColumn(tokens[extra.first].span.startColumn),
            sourceColumn(tokens[extra.second - 1].span.endColumn),
            "List size initializer has more sizes than its List/string depth",
            "nested sizes can only descend through another List or string element type",
            sourceLines
        );
        return {};
    }

    std::vector<std::string> generatedSizes;
    for (const auto& argument : arguments) {
        const std::vector<Token> expressionTokens = tokenRange(tokens, argument.first, argument.second);
        const int expressionColumn = sourceColumn(tokens[argument.first].span.startColumn);
        const ExpressionEmitResult expression = emitExpression(
            inputFile,
            lineNumber,
            expressionTokens,
            expressionColumn,
            sourceLines,
            declaredVariables
        );
        if (!expression.ok) return {};
        if (expression.type != PrimitiveType::Int) {
            recordListSizeDiagnostic(
                inputFile,
                lineNumber,
                expressionColumn,
                sourceColumn(tokens[argument.second - 1].span.endColumn),
                "List size initializer requires int sizes, got " + cpppTypeName(expression.type),
                "use an int expression for every List dimension",
                sourceLines
            );
            return {};
        }
        generatedSizes.push_back(expression.generatedExpression);
    }

    // Construction requirements are inferred from the emitted expression.

    std::string generated = typeInfo.cppType + "(";
    for (size_t index = 0; index < generatedSizes.size(); ++index) {
        if (index > 0) generated += ", ";
        generated += generatedSizes[index];
    }
    generated += ")";
    return {true, closeParenIndex + 1, generated};
}
}

TypeEmitResult emitResolvedTypeDeclaration(
    const std::string& inputFile,
    int lineNumber,
    int statementStartColumn,
    const std::map<int, std::string>& sourceLines,
    std::map<std::string, Type>& declaredVariables,
    const ResolvedDeclarationSyntax& declaration,
    const std::vector<Token>& sourceTokens
) {
    const auto sourceColumn = [statementStartColumn](int tokenColumn) { return statementStartColumn + tokenColumn - 1; };

    const std::vector<Token>& tokens = sourceTokens;
    if (tokens.size() < 2 || tokens[0].kind != TokenKind::Identifier) {
        return {false, true, "", {}};
    }

    if (declaration.inferred) {
        if (declaration.names.empty()) {
            recordSourceError(
                inputFile,
                lineNumber,
                sourceColumn(tokens[0].span.startColumn),
                "var declarations require a variable name",
                sourceLines
            );
            return {true, false, "", {}};
        }

        const std::string variableName = declaration.names.front().name;
        const int variableColumn = declaration.names.front().column;
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

        const size_t continuation = declaration.continuationTokenIndex;
        if (declaration.names.size() > 1 || tokens[continuation].kind == TokenKind::Comma) {
            recordSourceError(
                inputFile,
                lineNumber,
                sourceColumn(tokens[continuation].span.startColumn),
                "var declarations support exactly one variable",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        if (tokens[continuation].kind == TokenKind::EndOfFile) {
            recordSourceError(
                inputFile,
                lineNumber,
                sourceColumn(tokens[1].span.endColumn + 1),
                "var declarations require an initializer so the type can be inferred",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        if (tokens[continuation].kind != TokenKind::Equals) {
            recordSourceError(
                inputFile,
                lineNumber,
                sourceColumn(tokens[continuation].span.startColumn),
                "var declarations must use '=' with an initializer",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        const size_t valueStart = continuation + 1;
        if (tokens[valueStart].kind == TokenKind::EndOfFile) {
            recordSourceError(
                inputFile,
                lineNumber,
                sourceColumn(tokens[continuation].span.endColumn + 1),
                "var declarations require an initializer so the type can be inferred",
                sourceLines
            );
            rememberInvalidVariable(declaredVariables, variableName);
            return {true, false, "", {}};
        }

        const int assignedValueStartColumn = tokens[valueStart].span.startColumn;
        const int assignedValueColumn = sourceColumn(assignedValueStartColumn);
        size_t tokenIndex = valueStart;
        while (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
            ++tokenIndex;
        }

        const std::vector<Token> assignedValueTokens = tokenRange(tokens, valueStart, tokenIndex + 1);
        const std::string assignedValue = tokenText(assignedValueTokens);

        std::vector<InputArgument> inputArguments;
        if (parseInputCall(assignedValueTokens, assignedValueColumn, inputArguments)) {
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

        if (isEmptyContainerLiteral(assignedValueTokens)) {
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
            assignedValueTokens,
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

    const Type& targetType = declaration.type;
    if (targetType == PrimitiveType::Void) {
        recordSourceError(inputFile, lineNumber, sourceColumn(tokens[0].span.startColumn), "variables cannot have void type", sourceLines);
        return {true, false, "", {}};
    }
    const TypeInfo typeInfo = typeInfoFor(targetType);
    if (typeInfo.cppType.empty()) {
        recordSourceError(
            inputFile,
            lineNumber,
            sourceColumn(tokens[0].span.startColumn),
            "unsupported type " + cpppTypeName(targetType),
            sourceLines
        );
        return {true, false, "", {}};
    }

    std::vector<DeclaredName> variables;
    std::string sizedInitializer;
    size_t tokenIndex = declaration.continuationTokenIndex;
    {
        for (const ResolvedDeclaredName& name : declaration.names) {
            if (declaredVariables.count(name.name) != 0) {
                recordSourceError(inputFile, lineNumber, sourceColumn(name.column),
                    "variable '" + name.name + "' is already declared", sourceLines);
                return {true, false, "", {}};
            }
            for (const DeclaredName& variable : variables) {
                if (variable.name == name.name) {
                    recordSourceError(inputFile, lineNumber, sourceColumn(name.column),
                        "variable '" + name.name + "' is already declared", sourceLines);
                    return {true, false, "", {}};
                }
            }
            variables.push_back({name.name, name.column});
        }
    }

    if ( tokens[tokenIndex].kind == TokenKind::LeftParen) {
        if (isSetType(targetType) || isMapType(targetType) || isHeapType(targetType)) {
            if (variables.size() != 1) {
                recordSourceError(inputFile, lineNumber, sourceColumn(tokens[tokenIndex].span.startColumn),
                    "a collection comparator can declare only one variable", sourceLines);
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
            const size_t leftParenIndex = tokenIndex++;
            const size_t comparatorStart = tokenIndex;
            int depth = 1;
            while (tokens[tokenIndex].kind != TokenKind::EndOfFile && depth > 0) {
                if (tokens[tokenIndex].kind == TokenKind::LeftParen) ++depth;
                if (tokens[tokenIndex].kind == TokenKind::RightParen) --depth;
                if (depth == 0) break;
                ++tokenIndex;
            }
            if (tokens[tokenIndex].kind != TokenKind::RightParen || depth != 0) {
                recordSourceError(inputFile, lineNumber, sourceColumn(tokens[leftParenIndex].span.startColumn),
                    "unclosed collection comparator", sourceLines);
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
            const std::vector<Token> comparatorTokens = tokenRange(tokens, comparatorStart, tokenIndex);
            const ComparatorEmitResult comparator = emitCollectionComparator(
                inputFile, lineNumber, comparatorTokens,
                sourceColumn(tokens[comparatorStart].span.startColumn), targetType.subtypes[0],
                sourceLines, declaredVariables);
            if (!comparator.ok) {
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
            sizedInitializer = typeInfo.cppType + "(" + comparator.expression + ")";
            ++tokenIndex;
            if (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
                recordSourceError(inputFile, lineNumber, sourceColumn(tokens[tokenIndex].span.startColumn),
                    "unexpected token after collection comparator", sourceLines);
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
        } else {
            if (variables.size() != 1) {
                recordListSizeDiagnostic(
                    inputFile, lineNumber,
                    sourceColumn(tokens[tokenIndex].span.startColumn),
                    sourceColumn(tokens[tokenIndex].span.endColumn),
                    "a List size initializer can declare only one variable",
                    "split sized List declarations into separate statements", sourceLines);
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
            const ListSizeInitializerResult initializer = parseListSizeInitializer(
                inputFile, lineNumber, statementStartColumn, sourceLines,
                declaredVariables, tokens, tokenIndex, targetType, typeInfo);
            if (!initializer.ok) {
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
            sizedInitializer = initializer.generatedExpression;
            tokenIndex = initializer.nextTokenIndex;
            if (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
                recordListSizeDiagnostic(
                    inputFile, lineNumber,
                    sourceColumn(tokens[tokenIndex].span.startColumn),
                    sourceColumn(tokens[tokenIndex].span.endColumn),
                    "unexpected token after List size initializer",
                    "end the declaration after the closing `)`", sourceLines);
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
        }
    }

    std::string assignedValue;
    std::vector<Token> valueTokens;
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

        const int assignedValueStartColumn = tokens[tokenIndex].span.startColumn;
        assignedValueColumn = sourceColumn(assignedValueStartColumn);
        const size_t assignedValueStartIndex = tokenIndex;
        while (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
            ++tokenIndex;
        }

        valueTokens = tokenRange(tokens, assignedValueStartIndex, tokenIndex + 1);
        assignedValue = tokenText(valueTokens);
    } else if (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
        recordSourceError(
            inputFile,
            lineNumber,
            sourceColumn(tokens[tokenIndex].span.startColumn),
            "expected ';' or '=' after variable name",
            sourceLines
        );
        rememberInvalidVariables(declaredVariables, variables);
        return {true, false, "", {}};
    }

    std::string emittedValue = sizedInitializer.empty() ? typeInfo.defaultValue : sizedInitializer;
    if (sizedInitializer.empty() && (isSetType(targetType) || isMapType(targetType) || isHeapType(targetType))) {
        const ComparatorEmitResult comparator = emitCollectionComparator(
            inputFile, lineNumber, {}, statementStartColumn, targetType.subtypes[0], sourceLines, declaredVariables
        );
        if (!comparator.ok) {
            rememberInvalidVariables(declaredVariables, variables);
            return {true, false, "", {}};
        }
        if (isHeapType(targetType) &&
            (isCollectionType(targetType.subtypes[0]) || isPairType(targetType.subtypes[0]))) {
            requireContainerMember(targetType.subtypes[0], "compare_gt");
        }
        if (assignedValue.empty() && !isHeapType(targetType)) {
            emittedValue = typeInfo.cppType + "(" + comparator.expression + ")";
        }
    }
    std::vector<std::string> perVariableValues;
    if (!assignedValue.empty()) {
        emittedValue = assignedValue;
        std::vector<InputArgument> inputArguments;
        if (parseInputCall(valueTokens, assignedValueColumn, inputArguments)) {
            if (!emitInputCallForType(
                    inputFile,
                    lineNumber,
                    valueTokens,
                    assignedValueColumn,
                    targetType,
                    sourceLines,
                    declaredVariables,
                    emittedValue)) {
                return {true, false, "", {}};
            }
        } else if (variables.size() > 1 && splitTopLevelCommaValues(assignedValue, valueTokens, assignedValueColumn).size() > 1) {
            const std::vector<InitializerValue> values = splitTopLevelCommaValues(assignedValue, valueTokens, assignedValueColumn);
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
                    if (value.text.empty()) {
                        recordSourceError(inputFile, lineNumber, value.column, "expected value after ','", sourceLines);
                        rememberInvalidVariables(declaredVariables, variables);
                        return {true, false, "", {}};
                    }

                    std::string emitted;
                    if (!finishExpressionAssignment(
                            inputFile,
                            lineNumber,
                            value.text,
                            value.tokens,
                            value.column,
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
                            valueTokens,
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
                        valueTokens,
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
                        valueTokens,
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
                        valueTokens,
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
                        valueTokens,
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
        } else if (isSetType(targetType) || isMapType(targetType) || isHeapType(targetType) || isPairType(targetType) ||
                   isLinearDataStructureType(targetType)) {
            const bool isSet = isSetType(targetType);
            const bool isMap = isMapType(targetType);
            const bool isLinear = isLinearDataStructureType(targetType) || isHeapType(targetType);
            const std::string typeLabel = isSet ? "Set" : (isMap ? "Map" :
                (isLinear ? cpppTypeName(Type(targetType.primitive)) : "Pair"));
            const std::string literalHint = isSet
                ? "{1, 2}"
                : (isMap ? "{1:'a'}" : "(1,2)");

            if ((isSet || isMap) &&
                valueTokens.size() > 2 &&
                valueTokens[0].kind == TokenKind::LeftBrace &&
                valueTokens[1].kind == TokenKind::RightBrace) {
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
                        valueTokens,
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
        } else if (isFunctionType(targetType)) {
            if (!finishExpressionAssignment(
                    inputFile,
                    lineNumber,
                    assignedValue,
                    valueTokens,
                    assignedValueColumn,
                    targetType,
                    sourceLines,
                    declaredVariables,
                    emittedValue)) {
                rememberInvalidVariables(declaredVariables, variables);
                return {true, false, "", {}};
            }
        } else if (isStructType(targetType)) {
            if (!finishExpressionAssignment(
                    inputFile,
                    lineNumber,
                    assignedValue,
                    valueTokens,
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
                    valueTokens,
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
                        valueTokens,
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
                    Type sourceType = PrimitiveType::Unknown;
                    if (!valueTokens.empty()) {
                        if (valueTokens[0].kind == TokenKind::String) {
                            sourceType = declaredTypeForName("string");
                        } else if (valueTokens[0].kind == TokenKind::Float) {
                            sourceType = PrimitiveType::Float;
                        } else if (valueTokens[0].kind == TokenKind::Char) {
                            sourceType = PrimitiveType::Char;
                        }
                    }
                    recordConversionDiagnostic(
                        inputFile,
                        lineNumber,
                        assignedValueColumn,
                        assignedValue,
                        "int requires an integer literal",
                        sourceType,
                        targetType,
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
                        valueTokens,
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
            sourceColumn(variables[i].column),
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
