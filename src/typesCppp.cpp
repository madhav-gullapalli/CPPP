#include "typesCppp.h"

#include "expressions.h"
#include "tokenizer.h"

#include <cctype>
#include <map>
#include <regex>
#include <set>

namespace {
struct TypeInfo {
    std::string cppType;
    std::string defaultValue;
};

struct DeclaredName {
    std::string name;
    int column;
};

struct ParsedTypeName {
    bool ok = true;
    Type type;
    std::string name;
    size_t nextTokenIndex = 0;
};

std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

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

bool isCharLiteral(const std::string& text) {
    return text.size() == 3 && text.front() == '\'' && text.back() == '\'';
}

bool isMalformedCharLiteral(const std::string& text) {
    return !text.empty() && text.front() == '\'';
}

bool isIntegerLiteral(const std::string& text) {
    static const std::regex pattern(R"([+-]?[0-9]+)");
    return std::regex_match(text, pattern);
}

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

bool shouldParseAsExpression(const std::vector<Token>& tokens) {
    if (hasArithmeticOperator(tokens)) {
        return true;
    }

    for (const Token& token : tokens) {
        if (token.kind == TokenKind::Identifier || token.kind == TokenKind::LeftParen || token.kind == TokenKind::RightParen) {
            return true;
        }
    }

    return false;
}

const std::map<std::string, TypeInfo>& primitiveTypes() {
    static const std::map<std::string, TypeInfo> types = {
        {"bool", {"bool", "false"}},
        {"int", {"long long", "0"}},
        {"char", {"CPPPChar", "CPPPChar()"}},
        {"float", {"long double", "0.0L"}},
    };

    return types;
}

bool isBoolLiteral(const std::string& text) {
    return text == "true" || text == "false";
}

bool parseTypeName(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& tokens,
    const std::map<int, std::string>& sourceLines,
    ParsedTypeName& parsedType
) {
    if (tokens.empty() || tokens[0].kind != TokenKind::Identifier) {
        return false;
    }

    const std::string typeName = tokens[0].text;
    if (typeName == "bigint" || typeName == "Bigint" || typeName == "bigfloat" || typeName == "BigFloat") {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[0].span.startColumn,
            typeName + " has been removed from CP++; use int or float instead",
            sourceLines
        );
        parsedType.ok = false;
        parsedType.nextTokenIndex = 1;
        return true;
    }

    const auto type = primitiveTypes().find(typeName);
    if (type == primitiveTypes().end()) {
        return false;
    }

    const Type rootType = declaredTypeForName(typeName);
    parsedType.ok = true;
    parsedType.type = rootType;
    parsedType.name = typeName;
    parsedType.nextTokenIndex = 1;

    if (tokens.size() > 1 && tokens[1].kind == TokenKind::Operator && tokens[1].text == "<") {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[1].span.startColumn,
            typeName + " expects " + std::to_string(primitiveArity(rootType.primitive)) + " subtypes",
            sourceLines
        );
        parsedType.ok = false;
        parsedType.nextTokenIndex = 1;
    }

    return true;
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
    int depth = 0;
    size_t startIndex = 0;
    int valueColumn = startColumn;

    for (const Token& token : tokens) {
        if (token.kind == TokenKind::EndOfFile) {
            break;
        }
        if (token.kind == TokenKind::LeftParen) {
            ++depth;
        } else if (token.kind == TokenKind::RightParen && depth > 0) {
            --depth;
        } else if (token.kind == TokenKind::Comma && depth == 0) {
            std::string value = trim(text.substr(startIndex, static_cast<size_t>(token.span.startColumn - 1) - startIndex));
            values.push_back({value, valueColumn});
            startIndex = static_cast<size_t>(token.span.endColumn);
            valueColumn = startColumn + token.span.endColumn;
        }
    }

    values.push_back({trim(text.substr(startIndex)), valueColumn});
    return values;
}
}

std::vector<RuntimeHelper> runtimeHelpers() {
    return {
        {
            "CPPPCharCore",
            {
                "struct CPPPChar {",
                "    char value = '\\0';",
                "    CPPPChar() = default;",
                "    CPPPChar(char initialValue) : value(initialValue) {}",
                "    operator char() const { return value; }",
                "};",
                "",
                "ostream& operator<<(ostream& output, const CPPPChar& value) {",
                "    if (value.value == '\\0') {",
                "        return output << 0;",
                "    }",
                "",
                "    return output << value.value;",
                "}",
                "",
                "istream& operator>>(istream& input, CPPPChar& value) {",
                "    char ch;",
                "    input >> ch;",
                "    value = CPPPChar(ch);",
                "    return input;",
                "}",
                "",
                "CPPPChar& operator++(CPPPChar& value) { ++value.value; return value; }",
                "CPPPChar operator++(CPPPChar& value, int) { CPPPChar old = value; ++value; return old; }",
                "CPPPChar& operator--(CPPPChar& value) { --value.value; return value; }",
                "CPPPChar operator--(CPPPChar& value, int) { CPPPChar old = value; --value; return old; }",
                ""
            },
            {},
            {"CPPPChar"}
        },
        {
            "CPPPToBoolBool",
            {
                "bool CPPPToBoolBool(bool value) { return value; }",
                ""
            },
            {},
            {"CPPPToBoolBool("}
        },
        {
            "CPPPToBoolInt",
            {
                "bool CPPPToBoolInt(long long value) { return value != 0; }",
                ""
            },
            {},
            {"CPPPToBoolInt("}
        },
        {
            "CPPPToBoolFloat",
            {
                "bool CPPPToBoolFloat(long double value) { return value != 0.0L && !isnan(value); }",
                ""
            },
            {},
            {"CPPPToBoolFloat("}
        },
        {
            "CPPPToBoolChar",
            {
                "bool CPPPToBoolChar(const CPPPChar& value) { return value.value != '\\0'; }",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPToBoolChar("}
        },
        {
            "CPPPToBoolFallback",
            {
                "bool CPPPToBool(bool value) { return value; }",
                "bool CPPPToBool(int value) { return value != 0; }",
                "bool CPPPToBool(long long value) { return value != 0; }",
                "bool CPPPToBool(long double value) { return value != 0.0L && !isnan(value); }",
                "bool CPPPToBool(const CPPPChar& value) { return value.value != '\\0'; }",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPToBool("}
        },
        {
            "CPPPInputBool",
            {
                "bool CPPPInputBool() { bool value; cin >> value; return value; }",
                ""
            },
            {},
            {"CPPPInputBool("}
        },
        {
            "CPPPInputChar",
            {
                "CPPPChar CPPPInputChar() { CPPPChar value; cin >> value; return value; }",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPInputChar("}
        },
        {
            "CPPPInputInt",
            {
                "long long CPPPInputInt() { long long value; cin >> value; return value; }",
                ""
            },
            {},
            {"CPPPInputInt("}
        },
        {
            "CPPPInputFloat",
            {
                "long double CPPPInputFloat() { long double value; cin >> value; return value; }",
                ""
            },
            {},
            {"CPPPInputFloat("}
        }
    };
}

std::vector<std::string> typeSupportPreamble() {
    std::vector<std::string> preamble;
    for (const RuntimeHelper& helper : runtimeHelpers()) {
        preamble.insert(preamble.end(), helper.code.begin(), helper.code.end());
    }
    return preamble;
}

std::vector<std::string> typeSupportPreambleForSubmit(const std::string& generatedProgramText) {
    const std::vector<RuntimeHelper> helpers = runtimeHelpers();
    std::map<std::string, RuntimeHelper> helpersByName;
    for (const RuntimeHelper& helper : helpers) {
        helpersByName[helper.name] = helper;
    }

    std::set<std::string> requiredHelpers;
    std::vector<std::string> worklist;
    for (const RuntimeHelper& helper : helpers) {
        for (const std::string& trigger : helper.triggers) {
            if (generatedProgramText.find(trigger) != std::string::npos) {
                if (requiredHelpers.insert(helper.name).second) {
                    worklist.push_back(helper.name);
                }
                break;
            }
        }
    }

    for (size_t i = 0; i < worklist.size(); ++i) {
        const RuntimeHelper& helper = helpersByName.at(worklist[i]);
        for (const std::string& dep : helper.deps) {
            if (requiredHelpers.insert(dep).second) {
                worklist.push_back(dep);
            }
        }
    }

    std::vector<std::string> preamble;
    for (const RuntimeHelper& helper : helpers) {
        if (requiredHelpers.count(helper.name) == 0) {
            continue;
        }

        preamble.insert(preamble.end(), helper.code.begin(), helper.code.end());
    }

    return preamble;
}
 
TypeEmitResult emitTypeDeclaration(
    const std::string& inputFile,
    int lineNumber,
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    std::map<std::string, Type>& declaredVariables
) {
    (void)sourceLine;

    const std::vector<Token> tokens = tokenize(statementBody);
    if (tokens.size() < 2 || tokens[0].kind != TokenKind::Identifier) {
        return {false, true, "", {}};
    }

    ParsedTypeName parsedType;
    if (!parseTypeName(inputFile, lineNumber, tokens, sourceLines, parsedType)) {
        return {false, true, "", {}};
    }
    if (!parsedType.ok) {
        return {true, false, "", {}};
    }

    const std::string typeName = parsedType.name;
    const auto type = primitiveTypes().find(typeName);
    const Type targetType = parsedType.type;

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
                variableColumn,
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
        return {true, false, "", {}};
    }

    std::string emittedValue = type->second.defaultValue;
    std::vector<std::string> perVariableValues;
    bool assignsInput = false;
    if (!assignedValue.empty()) {
        emittedValue = assignedValue;
        const std::vector<Token> valueTokens = tokenize(assignedValue);

        if (isInputCall(valueTokens)) {
            assignsInput = true;
            emittedValue = inputFunctionForType(targetType);
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
                    return {true, false, "", {}};
                }

                for (const auto& value : values) {
                    if (value.first.empty()) {
                        recordSourceError(inputFile, lineNumber, value.second, "expected value after ','", sourceLines);
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
                        return {true, false, "", {}};
                    }
                    perVariableValues.push_back(emitted);
                }
            } else if (typeName == "char") {
                if (isCharLiteral(assignedValue)) {
                    emittedValue = "CPPPChar(" + assignedValue + ")";
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
                        return {true, false, "", {}};
                    }
                }
            }
        } else if (typeName == "char") {
            if (isCharLiteral(assignedValue)) {
                emittedValue = "CPPPChar(" + assignedValue + ")";
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
                    return {true, false, "", {}};
                }
            } else {
                std::string message = "char values must be a single character in single quotes";
                if (isMalformedCharLiteral(assignedValue)) {
                    const size_t closingQuote = assignedValue.find('\'', 1);
                    if (closingQuote != std::string::npos && closingQuote + 1 < assignedValue.size()) {
                        message = "unexpected characters after char literal";
                    } else if (assignedValue.back() == '\'') {
                        message = "char literal contains too many characters";
                    } else {
                        message = "unterminated char literal";
                    }
                }

                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn,
                    message,
                    sourceLines
                );
                return {true, false, "", {}};
            }
        } else if (typeName == "bool") {
            if (isBoolLiteral(assignedValue)) {
                emittedValue = assignedValue;
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
                return {true, false, "", {}};
            }
        } else if (typeName == "int") {
            if (shouldParseAsExpression(valueTokens)) {
                if (!finishExpressionAssignment(
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
            } else {
                if (!isIntegerLiteral(assignedValue)) {
                    recordSourceError(
                        inputFile,
                        lineNumber,
                        assignedValueColumn,
                        "int requires an integer literal",
                        sourceLines
                    );
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
                    return {true, false, "", {}};
                }
            }
        } else if (typeName == "float") {
            if (shouldParseAsExpression(valueTokens)) {
                if (!finishExpressionAssignment(
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
            } else if (assignedValue.find('.') != std::string::npos && assignedValue.find_first_of("lL") == std::string::npos) {
                emittedValue += "L";
            }
        }
    }

    std::string generatedStatement = "    " + type->second.cppType + " ";
    std::vector<SourceRange> ranges;
    for (size_t i = 0; i < variables.size(); ++i) {
        if (i > 0) {
            generatedStatement += ", ";
        }

        const int generatedStartColumn = static_cast<int>(generatedStatement.size()) + 1;
        const std::string initializer = !perVariableValues.empty() ? perVariableValues[i] : (assignsInput ? inputFunctionForType(targetType) : emittedValue);
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

    return {
        true,
        true,
        generatedStatement,
        ranges
    };
}
