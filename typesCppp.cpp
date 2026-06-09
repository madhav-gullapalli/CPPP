#include "typesCppp.h"

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
    if (firstSpace == std::string::npos) {
        return {false, true, "", {}};
    }

    const std::string typeName = statementBody.substr(0, firstSpace);
    const auto type = primitiveTypes().find(typeName);
    if (type == primitiveTypes().end()) {
        return {false, true, "", {}};
    }

    const std::string rest = trim(statementBody.substr(firstSpace + 1));
    const size_t equals = rest.find('=');
    const std::string variableName = trim(equals == std::string::npos ? rest : rest.substr(0, equals));
    const std::string assignedValue = equals == std::string::npos ? "" : trim(rest.substr(equals + 1));
    const size_t variableColumn = sourceLine.find(variableName);
    if (!isIdentifier(variableName)) {
        recordSourceError(
            inputFile,
            lineNumber,
            static_cast<int>(variableColumn == std::string::npos ? 1 : variableColumn + 1),
            "expected variable name after " + typeName,
            sourceLines
        );
        return {true, false, "", {}};
    }

    if (declaredVariables.count(variableName) != 0) {
        recordSourceError(
            inputFile,
            lineNumber,
            static_cast<int>(variableColumn == std::string::npos ? 1 : variableColumn + 1),
            "variable '" + variableName + "' is already declared",
            sourceLines
        );
        return {true, false, "", {}};
    }

    if (equals != std::string::npos && assignedValue.empty()) {
        recordSourceError(
            inputFile,
            lineNumber,
            static_cast<int>(sourceLine.find('=') + 2),
            "expected value after '='",
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
                    static_cast<int>(sourceLine.find(assignedValue) + 1),
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
                    static_cast<int>(sourceLine.find(assignedValue) + 1),
                    "int requires an integer literal",
                    sourceLines
                );
                return {true, false, "", {}};
            }

            if (!fitsLongLong(assignedValue)) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    static_cast<int>(sourceLine.find(assignedValue) + 1),
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
                    static_cast<int>(sourceLine.find(assignedValue) + 1),
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
                    static_cast<int>(sourceLine.find(assignedValue) + 1),
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
    const int sourceColumn = static_cast<int>(variableColumn == std::string::npos ? 1 : variableColumn + 1);
    return {
        true,
        true,
        generatedStatement,
        {{
            lineNumber,
            sourceColumn,
            generatedStartColumn,
            generatedStartColumn + static_cast<int>(variableName.size()) - 1
        }}
    };
}
