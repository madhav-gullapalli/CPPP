#include "typesCppp.h"

#include "expressions.h"
#include "tokenizer.h"

#include <cctype>
#include <map>
#include <regex>

namespace {
struct TypeInfo {
    std::string cppType;
    std::string defaultValue;
};

struct DeclaredName {
    std::string name;
    int column;
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

std::string quotedString(const std::string& text) {
    return "\"" + text + "\"";
}

const std::map<std::string, TypeInfo>& primitiveTypes() {
    static const std::map<std::string, TypeInfo> types = {
        {"bool", {"bool", "false"}},
        {"int", {"long long", "0"}},
        {"bigint", {"CPPPBigInt", "CPPPBigInt()"}},
        {"Bigint", {"CPPPBigInt", "CPPPBigInt()"}},
        {"char", {"CPPPChar", "CPPPChar()"}},
        {"float", {"long double", "0.0L"}},
        {"bigfloat", {"CPPPBigFloat", "CPPPBigFloat()"}},
        {"BigFloat", {"CPPPBigFloat", "CPPPBigFloat()"}},
    };

    return types;
}

bool isBoolLiteral(const std::string& text) {
    return text == "true" || text == "false";
}

bool finishExpressionAssignment(
    const std::string& inputFile,
    int lineNumber,
    const std::string& assignedValue,
    int assignedValueColumn,
    CpppType targetType,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, CpppType>& declaredVariables,
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
        emittedValue = castExpressionTo(emittedValue, targetType);
    }

    return true;
}
}

std::vector<std::string> typeSupportPreamble() {
    return {
        "using CPPPBigFloat = long double;",
        "",
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
        "",
        "bool CPPPToBool(bool value) { return value; }",
        "bool CPPPToBool(int value) { return value != 0; }",
        "bool CPPPToBool(long long value) { return value != 0; }",
        "bool CPPPToBool(long double value) { return value != 0.0L && !isnan(value); }",
        "bool CPPPToBool(const CPPPChar& value) { return value.value != '\\0'; }",
        "",
        "struct CPPPBigInt {",
        "    bool negative = false;",
        "    string digits = \"0\";",
        "",
        "    CPPPBigInt() = default;",
        "    CPPPBigInt(long long value) {",
        "        if (value < 0) {",
        "            negative = true;",
        "            unsigned long long magnitude = static_cast<unsigned long long>(-(value + 1)) + 1;",
        "            digits = to_string(magnitude);",
        "        } else {",
        "            digits = to_string(value);",
        "        }",
        "    }",
        "    CPPPBigInt(const string& value) { assign(value); }",
        "    CPPPBigInt(const char* value) { assign(string(value)); }",
        "    explicit operator long long() const {",
        "        long long result = 0;",
        "        for (char digit : digits) { result = result * 10 + (digit - '0'); }",
        "        return negative ? -result : result;",
        "    }",
        "    explicit operator long double() const {",
        "        long double result = 0;",
        "        for (char digit : digits) { result = result * 10 + (digit - '0'); }",
        "        return negative ? -result : result;",
        "    }",
        "    explicit operator bool() const { return digits != \"0\"; }",
        "",
        "    void assign(string value) {",
        "        negative = false;",
        "        if (!value.empty() && (value[0] == '-' || value[0] == '+')) {",
        "            negative = value[0] == '-';",
        "            value = value.substr(1);",
        "        }",
        "        digits = value.empty() ? \"0\" : value;",
        "        normalize();",
        "    }",
        "",
        "    void normalize() {",
        "        size_t first = digits.find_first_not_of('0');",
        "        digits = first == string::npos ? \"0\" : digits.substr(first);",
        "        if (digits == \"0\") {",
        "            negative = false;",
        "        }",
        "    }",
        "",
        "    static int compareAbs(const CPPPBigInt& left, const CPPPBigInt& right) {",
        "        if (left.digits.size() != right.digits.size()) {",
        "            return left.digits.size() < right.digits.size() ? -1 : 1;",
        "        }",
        "        if (left.digits == right.digits) {",
        "            return 0;",
        "        }",
        "        return left.digits < right.digits ? -1 : 1;",
        "    }",
        "",
        "    static string addAbs(string left, string right) {",
        "        string result;",
        "        int carry = 0;",
        "        int i = static_cast<int>(left.size()) - 1;",
        "        int j = static_cast<int>(right.size()) - 1;",
        "        while (i >= 0 || j >= 0 || carry != 0) {",
        "            int sum = carry;",
        "            if (i >= 0) { sum += left[i--] - '0'; }",
        "            if (j >= 0) { sum += right[j--] - '0'; }",
        "            result.push_back(static_cast<char>('0' + (sum % 10)));",
        "            carry = sum / 10;",
        "        }",
        "        reverse(result.begin(), result.end());",
        "        return result;",
        "    }",
        "",
        "    static string subAbs(string left, string right) {",
        "        string result;",
        "        int borrow = 0;",
        "        int i = static_cast<int>(left.size()) - 1;",
        "        int j = static_cast<int>(right.size()) - 1;",
        "        while (i >= 0) {",
        "            int digit = (left[i--] - '0') - borrow;",
        "            if (j >= 0) { digit -= right[j--] - '0'; }",
        "            if (digit < 0) {",
        "                digit += 10;",
        "                borrow = 1;",
        "            } else {",
        "                borrow = 0;",
        "            }",
        "            result.push_back(static_cast<char>('0' + digit));",
        "        }",
        "        while (result.size() > 1 && result.back() == '0') { result.pop_back(); }",
        "        reverse(result.begin(), result.end());",
        "        return result;",
        "    }",
        "",
        "    static string mulAbs(const string& left, const string& right) {",
        "        vector<int> result(left.size() + right.size(), 0);",
        "        for (int i = static_cast<int>(left.size()) - 1; i >= 0; --i) {",
        "            for (int j = static_cast<int>(right.size()) - 1; j >= 0; --j) {",
        "                int product = (left[i] - '0') * (right[j] - '0') + result[i + j + 1];",
        "                result[i + j + 1] = product % 10;",
        "                result[i + j] += product / 10;",
        "            }",
        "        }",
        "        string text;",
        "        size_t index = 0;",
        "        while (index < result.size() - 1 && result[index] == 0) { ++index; }",
        "        for (; index < result.size(); ++index) { text.push_back(static_cast<char>('0' + result[index])); }",
        "        return text;",
        "    }",
        "",
        "    static pair<string, string> divModAbs(const CPPPBigInt& left, const CPPPBigInt& right) {",
        "        if (right.digits == \"0\") { throw runtime_error(\"division by zero\"); }",
        "        CPPPBigInt remainder;",
        "        string quotient;",
        "        for (char digit : left.digits) {",
        "            remainder.digits.push_back(digit);",
        "            remainder.normalize();",
        "            int q = 0;",
        "            while (compareAbs(remainder, right) >= 0) {",
        "                remainder.digits = subAbs(remainder.digits, right.digits);",
        "                remainder.normalize();",
        "                ++q;",
        "            }",
        "            quotient.push_back(static_cast<char>('0' + q));",
        "        }",
        "        CPPPBigInt cleanQuotient(quotient);",
        "        return {cleanQuotient.digits, remainder.digits};",
        "    }",
        "};",
        "",
        "CPPPBigInt operator+(const CPPPBigInt& left, const CPPPBigInt& right) {",
        "    CPPPBigInt result;",
        "    if (left.negative == right.negative) {",
        "        result.negative = left.negative;",
        "        result.digits = CPPPBigInt::addAbs(left.digits, right.digits);",
        "    } else if (CPPPBigInt::compareAbs(left, right) >= 0) {",
        "        result.negative = left.negative;",
        "        result.digits = CPPPBigInt::subAbs(left.digits, right.digits);",
        "    } else {",
        "        result.negative = right.negative;",
        "        result.digits = CPPPBigInt::subAbs(right.digits, left.digits);",
        "    }",
        "    result.normalize();",
        "    return result;",
        "}",
        "",
        "CPPPBigInt operator-(const CPPPBigInt& left, const CPPPBigInt& right) {",
        "    CPPPBigInt negated = right;",
        "    if (negated.digits != \"0\") { negated.negative = !negated.negative; }",
        "    return left + negated;",
        "}",
        "",
        "CPPPBigInt operator*(const CPPPBigInt& left, const CPPPBigInt& right) {",
        "    CPPPBigInt result;",
        "    result.negative = left.negative != right.negative;",
        "    result.digits = CPPPBigInt::mulAbs(left.digits, right.digits);",
        "    result.normalize();",
        "    return result;",
        "}",
        "",
        "CPPPBigInt operator/(const CPPPBigInt& left, const CPPPBigInt& right) {",
        "    auto parts = CPPPBigInt::divModAbs(left, right);",
        "    CPPPBigInt result(parts.first);",
        "    result.negative = left.negative != right.negative;",
        "    result.normalize();",
        "    return result;",
        "}",
        "",
        "CPPPBigInt operator%(const CPPPBigInt& left, const CPPPBigInt& right) {",
        "    auto parts = CPPPBigInt::divModAbs(left, right);",
        "    CPPPBigInt result(parts.second);",
        "    result.negative = left.negative;",
        "    result.normalize();",
        "    return result;",
        "}",
        "",
        "int compare(const CPPPBigInt& left, const CPPPBigInt& right) {",
        "    if (left.negative != right.negative) { return left.negative ? -1 : 1; }",
        "    int result = CPPPBigInt::compareAbs(left, right);",
        "    return left.negative ? -result : result;",
        "}",
        "",
        "bool operator==(const CPPPBigInt& left, const CPPPBigInt& right) { return compare(left, right) == 0; }",
        "bool operator!=(const CPPPBigInt& left, const CPPPBigInt& right) { return compare(left, right) != 0; }",
        "bool operator<(const CPPPBigInt& left, const CPPPBigInt& right) { return compare(left, right) < 0; }",
        "bool operator<=(const CPPPBigInt& left, const CPPPBigInt& right) { return compare(left, right) <= 0; }",
        "bool operator>(const CPPPBigInt& left, const CPPPBigInt& right) { return compare(left, right) > 0; }",
        "bool operator>=(const CPPPBigInt& left, const CPPPBigInt& right) { return compare(left, right) >= 0; }",
        "bool CPPPToBool(const CPPPBigInt& value) { return static_cast<bool>(value); }",
        "",
        "CPPPBigInt& operator+=(CPPPBigInt& left, const CPPPBigInt& right) { left = left + right; return left; }",
        "CPPPBigInt& operator-=(CPPPBigInt& left, const CPPPBigInt& right) { left = left - right; return left; }",
        "CPPPBigInt& operator*=(CPPPBigInt& left, const CPPPBigInt& right) { left = left * right; return left; }",
        "CPPPBigInt& operator/=(CPPPBigInt& left, const CPPPBigInt& right) { left = left / right; return left; }",
        "CPPPBigInt& operator%=(CPPPBigInt& left, const CPPPBigInt& right) { left = left % right; return left; }",
        "CPPPBigInt& operator++(CPPPBigInt& value) { value += CPPPBigInt(1); return value; }",
        "CPPPBigInt operator++(CPPPBigInt& value, int) { CPPPBigInt old = value; ++value; return old; }",
        "CPPPBigInt& operator--(CPPPBigInt& value) { value -= CPPPBigInt(1); return value; }",
        "CPPPBigInt operator--(CPPPBigInt& value, int) { CPPPBigInt old = value; --value; return old; }",
        "",
        "ostream& operator<<(ostream& output, const CPPPBigInt& value) {",
        "    if (value.negative) { output << '-'; }",
        "    return output << value.digits;",
        "}",
        "",
        "istream& operator>>(istream& input, CPPPBigInt& value) {",
        "    string text;",
        "    input >> text;",
        "    value = CPPPBigInt(text);",
        "    return input;",
        "}",
        "",
        "bool CPPPInputBool() { bool value; cin >> value; return value; }",
        "CPPPChar CPPPInputChar() { CPPPChar value; cin >> value; return value; }",
        "long long CPPPInputInt() { long long value; cin >> value; return value; }",
        "CPPPBigInt CPPPInputBigInt() { CPPPBigInt value; cin >> value; return value; }",
        "long double CPPPInputFloat() { long double value; cin >> value; return value; }",
        ""
    };
}

TypeEmitResult emitTypeDeclaration(
    const std::string& inputFile,
    int lineNumber,
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    std::map<std::string, CpppType>& declaredVariables
) {
    (void)sourceLine;

    const std::vector<Token> tokens = tokenize(statementBody);
    if (tokens.size() < 2 || tokens[0].kind != TokenKind::Identifier) {
        return {false, true, "", {}};
    }

    const std::string typeName = tokens[0].text;
    const auto type = primitiveTypes().find(typeName);
    if (type == primitiveTypes().end()) {
        return {false, true, "", {}};
    }
    const CpppType targetType = declaredTypeForName(typeName);

    std::vector<DeclaredName> variables;
    size_t tokenIndex = 1;
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
    bool assignsInput = false;
    if (!assignedValue.empty()) {
        emittedValue = assignedValue;
        const std::vector<Token> valueTokens = tokenize(assignedValue);

        if (isInputCall(valueTokens)) {
            assignsInput = true;
            emittedValue = inputFunctionForType(targetType);
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
        } else if (typeName == "bigint" || typeName == "Bigint") {
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
            } else if (!isIntegerLiteral(assignedValue)) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn,
                    "bigint requires an integer literal",
                    sourceLines
                );
                return {true, false, "", {}};
            } else {
                emittedValue = "CPPPBigInt(" + quotedString(assignedValue) + ")";
            }
        } else if (typeName == "bigfloat" || typeName == "BigFloat") {
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
            } else if (!isFloatLiteral(assignedValue)) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    assignedValueColumn,
                    "bigfloat requires a floating-point literal",
                    sourceLines
                );
                return {true, false, "", {}};
            } else {
                emittedValue = assignedValue;
                if (assignedValue.find_first_of("lL") == std::string::npos) {
                    emittedValue += "L";
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
        generatedStatement += variables[i].name + " = " + (assignsInput ? inputFunctionForType(targetType) : emittedValue);
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
