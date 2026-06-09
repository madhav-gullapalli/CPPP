#include "typesCppp.h"

#include "tokenizer.h"

#include <cctype>
#include <map>
#include <regex>

namespace {
struct TypeInfo {
    std::string cppType;
    std::string defaultValue;
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

bool isFloatLiteral(const std::string& text) {
    static const std::regex pattern(R"([+-]?(([0-9]+(\.[0-9]*)?)|(\.[0-9]+))([eE][+-]?[0-9]+)?)");
    return std::regex_match(text, pattern) && text.find('.') != std::string::npos;
}

std::string quotedString(const std::string& text) {
    return "\"" + text + "\"";
}

const std::map<std::string, TypeInfo>& primitiveTypes() {
    static const std::map<std::string, TypeInfo> types = {
        {"bool", {"bool", "false"}},
        {"int", {"long long", "0"}},
        {"bigint", {"CPPPBigInt", "CPPPBigInt()"}},
        {"char", {"CPPPChar", "CPPPChar()"}},
        {"float", {"long double", "0.0L"}},
        {"bigfloat", {"CPPPBigFloat", "CPPPBigFloat()"}},
    };

    return types;
}
}

std::vector<std::string> typeSupportPreamble() {
    return {
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
        "struct CPPPBigInt {",
        "    string value = \"0\";",
        "    CPPPBigInt() = default;",
        "    CPPPBigInt(string initialValue) : value(initialValue) {}",
        "};",
        "",
        "ostream& operator<<(ostream& output, const CPPPBigInt& value) {",
        "    return output << value.value;",
        "}",
        "",
        "struct CPPPBigFloat {",
        "    string value = \"0\";",
        "    CPPPBigFloat() = default;",
        "    CPPPBigFloat(string initialValue) : value(initialValue) {}",
        "};",
        "",
        "ostream& operator<<(ostream& output, const CPPPBigFloat& value) {",
        "    return output << value.value;",
        "}",
        ""
    };
}

TypeEmitResult emitTypeDeclaration(
    const std::string& inputFile,
    int lineNumber,
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    std::set<std::string>& declaredVariables
) {
    const size_t firstSpace = statementBody.find_first_of(" \t");
    const std::vector<Token> tokens = tokenize(statementBody);
    if (tokens.size() < 2 || tokens[0].kind != TokenKind::Identifier) {
        return {false, true, "", {}};
    }

    const std::string typeName = tokens[0].text;
    const auto type = primitiveTypes().find(typeName);
    if (type == primitiveTypes().end()) {
        return {false, true, "", {}};
    }

    if (tokens[1].kind != TokenKind::Identifier) {
        recordSourceError(
            inputFile,
            lineNumber,
            tokens[1].span.startColumn,
            "expected variable name after " + typeName,
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

    size_t tokenIndex = 2;
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
    if (!assignedValue.empty()) {
        emittedValue = assignedValue;

        if (typeName == "char") {
            if (!isCharLiteral(assignedValue)) {
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

            emittedValue = "CPPPChar(" + assignedValue + ")";
        } else if (typeName == "int") {
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
        } else if (typeName == "bigint") {
            if (!isIntegerLiteral(assignedValue)) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn,
                    "bigint requires an integer literal",
                    sourceLines
                );
                return {true, false, "", {}};
            }

            emittedValue = "CPPPBigInt(" + quotedString(assignedValue) + ")";
        } else if (typeName == "bigfloat") {
            if (!isFloatLiteral(assignedValue)) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn,
                    "bigfloat requires a floating-point literal",
                    sourceLines
                );
                return {true, false, "", {}};
            }

            emittedValue = "CPPPBigFloat(" + quotedString(assignedValue) + ")";
        } else if (typeName == "float") {
            emittedValue = assignedValue;
            if (assignedValue.find('.') != std::string::npos && assignedValue.find_first_of("lL") == std::string::npos) {
                emittedValue += "L";
            }
        }
    }

    const std::string generatedStatement =
        "    " + type->second.cppType + " " + variableName + " = " + emittedValue + ";";

    declaredVariables.insert(variableName);

    const int generatedStartColumn = 5 + static_cast<int>(type->second.cppType.size()) + 1;
    return {
        true,
        true,
        generatedStatement,
        {{
            lineNumber,
            variableColumn,
            generatedStartColumn,
            generatedStartColumn + static_cast<int>(variableName.size()) - 1
        }}
    };
}
