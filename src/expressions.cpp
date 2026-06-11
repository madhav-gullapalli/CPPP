#include "expressions.h"

namespace {
struct ParsedExpression {
    bool ok;
    std::string text;
    CpppType type;
    bool explicitCast;
    int sourceColumn;
    bool mutableValue;
};

bool& expressionRuntimeChecksEnabled() {
    static bool enabled = false;
    return enabled;
}

class ExpressionParser {
public:
    ExpressionParser(
        const std::string& inputFile,
        int lineNumber,
        const std::string& expressionText,
        int expressionColumn,
        const std::map<int, std::string>& sourceLines,
        const std::map<std::string, CpppType>& declaredVariables,
        bool emitRuntimeChecks
    ) :
        inputFile(inputFile),
        lineNumber(lineNumber),
        expressionText(expressionText),
        expressionColumn(expressionColumn),
        sourceLines(sourceLines),
        declaredVariables(declaredVariables),
        emitRuntimeChecks(emitRuntimeChecks),
        tokens(tokenize(expressionText)) {}

    ExpressionEmitResult parse() {
        for (const Token& token : tokens) {
            if (isUnterminatedQuotedToken(token)) {
                report(token, token.kind == TokenKind::Char ? "unterminated char literal" : "unterminated string literal");
                return {false, "", CpppType::Unknown, false, {}};
            }
        }

        const ParsedExpression expression = parseExpression();
        if (!expression.ok) {
            return {false, "", CpppType::Unknown, false, {}};
        }

        if (!atEnd()) {
            report(peek(), "unexpected token in expression");
            return {false, "", CpppType::Unknown, false, {}};
        }

        return {
            true,
            expression.text,
            expression.type,
            expression.explicitCast,
            {{
                lineNumber,
                expression.sourceColumn,
                0,
                0
            }}
        };
    }

private:
    const std::string& inputFile;
    int lineNumber;
    const std::string& expressionText;
    int expressionColumn;
    const std::map<int, std::string>& sourceLines;
    const std::map<std::string, CpppType>& declaredVariables;
    bool emitRuntimeChecks;
    std::vector<Token> tokens;
    size_t current = 0;

    bool atEnd() const {
        return peek().kind == TokenKind::EndOfFile;
    }

    const Token& peek() const {
        return tokens[current];
    }

    const Token& previous() const {
        return tokens[current - 1];
    }

    bool match(TokenKind kind, const std::string& text = "") {
        if (peek().kind != kind || (!text.empty() && peek().text != text)) {
            return false;
        }

        ++current;
        return true;
    }

    bool check(TokenKind kind, const std::string& text = "") const {
        return peek().kind == kind && (text.empty() || peek().text == text);
    }

    bool isOperator(const std::string& text) const {
        return check(TokenKind::Operator, text);
    }

    bool isUnterminatedQuotedToken(const Token& token) const {
        if (token.kind != TokenKind::String && token.kind != TokenKind::Char) {
            return false;
        }

        return token.text.size() < 2 || token.text.front() != token.text.back();
    }

    int absoluteColumn(const Token& token) const {
        return expressionColumn + token.span.startColumn - 1;
    }

    void report(const Token& token, const std::string& message) const {
        recordSourceError(inputFile, lineNumber, absoluteColumn(token), message, sourceLines);
    }

    bool reportInputUsageError(const Token& inputToken) const {
        if (!check(TokenKind::LeftParen)) {
            report(inputToken, "input must be called as input()");
            return true;
        }

        const Token& leftParen = peek();
        if (current + 1 >= tokens.size() || tokens[current + 1].kind == TokenKind::EndOfFile) {
            report(leftParen, "unclosed parenthesis in input");
            return true;
        }

        if (tokens[current + 1].kind != TokenKind::RightParen) {
            report(tokens[current + 1], "input() does not take arguments");
            return true;
        }

        report(inputToken, "input() can only be used as the entire value in an assignment or declaration");
        return true;
    }

    ParsedExpression parseExpression() {
        return parseLogicalOr();
    }

    ParsedExpression parseLogicalOr() {
        ParsedExpression expression = parseLogicalAnd();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("||")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseLogicalAnd();
            if (!right.ok) {
                return right;
            }

            if (!isValueType(expression.type) || !isValueType(right.type)) {
                report(op, "cannot use '" + op.text + "' with " + cpppTypeName(expression.type) + " and " + cpppTypeName(right.type));
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            expression.text = "(" + castExpressionTo(expression.text, CpppType::Bool) + " " + op.text + " " + castExpressionTo(right.text, CpppType::Bool) + ")";
            expression.type = CpppType::Bool;
            expression.explicitCast = false;
        }

        return expression;
    }

    ParsedExpression parseLogicalAnd() {
        ParsedExpression expression = parseBitwiseOr();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("&&")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseBitwiseOr();
            if (!right.ok) {
                return right;
            }

            if (!isValueType(expression.type) || !isValueType(right.type)) {
                report(op, "cannot use '" + op.text + "' with " + cpppTypeName(expression.type) + " and " + cpppTypeName(right.type));
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            expression.text = "(" + castExpressionTo(expression.text, CpppType::Bool) + " " + op.text + " " + castExpressionTo(right.text, CpppType::Bool) + ")";
            expression.type = CpppType::Bool;
            expression.explicitCast = false;
        }

        return expression;
    }

    ParsedExpression parseBitwiseOr() {
        ParsedExpression expression = parseBitwiseXor();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("|")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseBitwiseXor();
            if (!right.ok) {
                return right;
            }

            if (!isBitwiseType(expression.type) || !isBitwiseType(right.type)) {
                report(op, "cannot use '" + op.text + "' with " + cpppTypeName(expression.type) + " and " + cpppTypeName(right.type));
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == CpppType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = CpppType::Int;
            expression.explicitCast = false;
        }

        return expression;
    }

    ParsedExpression parseBitwiseXor() {
        ParsedExpression expression = parseBitwiseAnd();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("^")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseBitwiseAnd();
            if (!right.ok) {
                return right;
            }

            if (!isBitwiseType(expression.type) || !isBitwiseType(right.type)) {
                report(op, "cannot use '" + op.text + "' with " + cpppTypeName(expression.type) + " and " + cpppTypeName(right.type));
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == CpppType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = CpppType::Int;
            expression.explicitCast = false;
        }

        return expression;
    }

    ParsedExpression parseBitwiseAnd() {
        ParsedExpression expression = parseEquality();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("&")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseEquality();
            if (!right.ok) {
                return right;
            }

            if (!isBitwiseType(expression.type) || !isBitwiseType(right.type)) {
                report(op, "cannot use '" + op.text + "' with " + cpppTypeName(expression.type) + " and " + cpppTypeName(right.type));
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == CpppType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = CpppType::Int;
            expression.explicitCast = false;
        }

        return expression;
    }

    ParsedExpression parseEquality() {
        ParsedExpression expression = parseComparison();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("==") || isOperator("!=")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseComparison();
            if (!right.ok) {
                return right;
            }

            if (!isComparable(expression.type, right.type)) {
                report(op, "cannot compare " + cpppTypeName(expression.type) + " and " + cpppTypeName(right.type));
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == CpppType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = CpppType::Bool;
            expression.explicitCast = false;
        }

        return expression;
    }

    ParsedExpression parseComparison() {
        ParsedExpression expression = parseShift();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("<") || isOperator("<=") || isOperator(">") || isOperator(">=")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseShift();
            if (!right.ok) {
                return right;
            }

            if (!isComparable(expression.type, right.type)) {
                report(op, "cannot compare " + cpppTypeName(expression.type) + " and " + cpppTypeName(right.type));
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == CpppType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = CpppType::Bool;
            expression.explicitCast = false;
        }

        return expression;
    }

    ParsedExpression parseShift() {
        ParsedExpression expression = parseAdditive();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("<<") || isOperator(">>")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseAdditive();
            if (!right.ok) {
                return right;
            }

            if (!isBitwiseType(expression.type) || !isBitwiseType(right.type)) {
                report(op, "cannot use '" + op.text + "' with " + cpppTypeName(expression.type) + " and " + cpppTypeName(right.type));
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == CpppType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = CpppType::Int;
            expression.explicitCast = false;
        }

        return expression;
    }

    ParsedExpression parseAdditive() {
        ParsedExpression expression = parseMultiplicative();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("+") || isOperator("-")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseMultiplicative();
            if (!right.ok) {
                return right;
            }

            const CpppType leftType = expression.type;
            const CpppType rightType = right.type;
            expression.type = binaryResultType(leftType, rightType, op.text);
            expression.explicitCast = false;
            if (expression.type == CpppType::Unknown) {
                report(op, "cannot mix " + cpppTypeName(leftType) + " and " + cpppTypeName(rightType) + " with '" + op.text + "'");
                return {false, "", CpppType::Unknown, false, 0, false};
            }
            if (emitRuntimeChecks && expression.type == CpppType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
        }

        return expression;
    }

    ParsedExpression parseMultiplicative() {
        ParsedExpression expression = parseUnary();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("*") || isOperator("/") || isOperator("%")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseUnary();
            if (!right.ok) {
                return right;
            }

            const CpppType leftType = expression.type;
            const CpppType rightType = right.type;
            expression.type = binaryResultType(leftType, rightType, op.text);
            expression.explicitCast = false;
            if (expression.type == CpppType::Unknown) {
                report(op, "cannot mix " + cpppTypeName(leftType) + " and " + cpppTypeName(rightType) + " with '" + op.text + "'");
                return {false, "", CpppType::Unknown, false, 0, false};
            }
            if (emitRuntimeChecks && expression.type == CpppType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
        }

        return expression;
    }

    ParsedExpression parseUnary() {
        if (isOperator("++") || isOperator("--")) {
            const Token op = peek();
            ++current;
            if (!match(TokenKind::Identifier)) {
                report(op, "expected variable after '" + op.text + "'");
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            const Token& identifier = previous();
            const auto variable = declaredVariables.find(identifier.text);
            if (variable == declaredVariables.end()) {
                report(identifier, "use of undeclared variable '" + identifier.text + "'");
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            if (!isIncrementableType(variable->second)) {
                report(op, "cannot use '" + op.text + "' with " + cpppTypeName(variable->second));
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            return {true, "(" + op.text + identifier.text + ")", variable->second, false, absoluteColumn(op), false};
        }

        if (isOperator("+") || isOperator("-") || isOperator("!")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseUnary();
            if (!right.ok) {
                return right;
            }

            if (op.text == "!") {
                if (!isValueType(right.type)) {
                    report(op, "cannot use '!' with " + cpppTypeName(right.type));
                    return {false, "", CpppType::Unknown, false, 0, false};
                }

                return {true, "(!" + castExpressionTo(right.text, CpppType::Bool) + ")", CpppType::Bool, false, absoluteColumn(op), false};
            }

            if (!isNumericType(right.type)) {
                report(op, "cannot use unary '" + op.text + "' with " + cpppTypeName(right.type));
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            return {true, "(" + op.text + right.text + ")", right.type, false, absoluteColumn(op), false};
        }

        return parsePostfix();
    }

    ParsedExpression parsePostfix() {
        ParsedExpression expression = parsePrimary();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("++") || isOperator("--")) {
            const Token op = peek();
            ++current;
            if (!expression.mutableValue) {
                report(op, "expected variable before '" + op.text + "'");
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            if (!isIncrementableType(expression.type)) {
                report(op, "cannot use '" + op.text + "' with " + cpppTypeName(expression.type));
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            expression.text = "(" + expression.text + op.text + ")";
            expression.explicitCast = false;
            expression.mutableValue = false;
        }

        return expression;
    }

    ParsedExpression parsePrimary() {
        if (check(TokenKind::LeftParen) &&
            current + 2 < tokens.size() &&
            tokens[current + 1].kind == TokenKind::Identifier &&
            isTypeName(tokens[current + 1].text) &&
            tokens[current + 2].kind == TokenKind::RightParen) {
            ++current;
            const Token typeToken = peek();
            ++current;
            ++current;
            const ParsedExpression expression = parseUnary();
            if (!expression.ok) {
                return expression;
            }

            const CpppType targetType = declaredTypeForName(typeToken.text);
            return {
                true,
                castExpressionTo(expression.text, targetType),
                targetType,
                true,
                absoluteColumn(typeToken),
                false
            };
        }

        if (match(TokenKind::Identifier)) {
            const Token& identifier = previous();
            if (identifier.text == "true" || identifier.text == "false") {
                return {true, identifier.text, CpppType::Bool, false, absoluteColumn(identifier), false};
            }

            if (identifier.text == "input") {
                reportInputUsageError(identifier);
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            const auto variable = declaredVariables.find(identifier.text);
            if (variable == declaredVariables.end()) {
                report(identifier, "use of undeclared variable '" + identifier.text + "'");
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            return {true, identifier.text, variable->second, false, absoluteColumn(identifier), true};
        }

        if (match(TokenKind::Integer)) {
            const Token& literal = previous();
            return {true, literal.text, CpppType::Int, false, absoluteColumn(literal), false};
        }

        if (match(TokenKind::Float)) {
            const Token& literal = previous();
            return {true, literal.text, CpppType::Float, false, absoluteColumn(literal), false};
        }

        if (match(TokenKind::String)) {
            const Token& literal = previous();
            return {true, literal.text, CpppType::Unknown, false, absoluteColumn(literal), false};
        }

        if (match(TokenKind::Char)) {
            const Token& literal = previous();
            return {true, "CPPPChar(" + literal.text + ")", CpppType::Char, false, absoluteColumn(literal), false};
        }

        if (match(TokenKind::LeftParen)) {
            const Token& leftParen = previous();
            const ParsedExpression expression = parseExpression();
            if (!expression.ok) {
                return expression;
            }

            if (!match(TokenKind::RightParen)) {
                report(leftParen, "unclosed parenthesis in expression");
                return {false, "", CpppType::Unknown, false, 0, false};
            }

            return {true, "(" + expression.text + ")", expression.type, expression.explicitCast, absoluteColumn(leftParen), false};
        }

        report(peek(), "expected expression");
        return {false, "", CpppType::Unknown, false, 0, false};
    }

    bool isTypeName(const std::string& name) const {
        return declaredTypeForName(name) != CpppType::Unknown;
    }

    CpppType binaryResultType(CpppType left, CpppType right, const std::string& op) const {
        if (!isNumericType(left) || !isNumericType(right)) {
            return CpppType::Unknown;
        }

        if (op == "%" && (isFloatType(left) || isFloatType(right))) {
            return CpppType::Unknown;
        }

        if (left == CpppType::Float || right == CpppType::Float) {
            return CpppType::Float;
        }

        return CpppType::Int;
    }

    bool isValueType(CpppType type) const {
        return type != CpppType::Unknown;
    }

    bool isNumericType(CpppType type) const {
        return type == CpppType::Bool ||
            type == CpppType::Char ||
            type == CpppType::Int ||
            type == CpppType::Float;
    }

    bool isBitwiseType(CpppType type) const {
        return type == CpppType::Bool || type == CpppType::Char || type == CpppType::Int;
    }

    bool isIncrementableType(CpppType type) const {
        return type == CpppType::Char ||
            type == CpppType::Int ||
            type == CpppType::Float;
    }

    bool isComparable(CpppType left, CpppType right) const {
        if (!isValueType(left) || !isValueType(right)) {
            return false;
        }

        return true;
    }

    bool isFloatType(CpppType type) const {
        return type == CpppType::Float;
    }

    std::string runtimeErrorThrowExpression(int column, const std::string& message) const {
        return "throw runtime_error(\"" + std::to_string(lineNumber) + ":" + std::to_string(column) + ":" + message + "\")";
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

};
}

std::string cpppTypeName(CpppType type) {
    switch (type) {
        case CpppType::Bool:
            return "bool";
        case CpppType::Char:
            return "char";
        case CpppType::Int:
            return "int";
        case CpppType::Float:
            return "float";
        case CpppType::Unknown:
            return "unknown";
    }

    return "unknown";
}

bool isImplicitlyConvertible(CpppType from, CpppType to) {
    if (from == to) {
        return true;
    }

    if (from == CpppType::Bool) {
        return to == CpppType::Char || to == CpppType::Int || to == CpppType::Float;
    }

    if (from == CpppType::Char) {
        return to == CpppType::Bool || to == CpppType::Int || to == CpppType::Float;
    }

    if (from == CpppType::Int) {
        return to == CpppType::Bool || to == CpppType::Float;
    }

    if (from == CpppType::Float) {
        return to == CpppType::Bool;
    }

    return false;
}

std::string castExpressionTo(const std::string& expression, CpppType to) {
    switch (to) {
        case CpppType::Bool:
            return "CPPPToBool(" + expression + ")";
        case CpppType::Char:
            return "CPPPChar(static_cast<char>(" + expression + "))";
        case CpppType::Int:
            return "static_cast<long long>(" + expression + ")";
        case CpppType::Float:
            return "static_cast<long double>(" + expression + ")";
        case CpppType::Unknown:
            return expression;
    }

    return expression;
}

CpppType declaredTypeForName(const std::string& name) {
    if (name == "bool") {
        return CpppType::Bool;
    }
    if (name == "char") {
        return CpppType::Char;
    }
    if (name == "int") {
        return CpppType::Int;
    }
    if (name == "float") {
        return CpppType::Float;
    }

    return CpppType::Unknown;
}

bool isInputCall(const std::vector<Token>& tokens) {
    return tokens.size() == 4 &&
        tokens[0].kind == TokenKind::Identifier &&
        tokens[0].text == "input" &&
        tokens[1].kind == TokenKind::LeftParen &&
        tokens[2].kind == TokenKind::RightParen &&
        tokens[3].kind == TokenKind::EndOfFile;
}

std::string inputFunctionForType(CpppType type) {
    switch (type) {
        case CpppType::Bool:
            return "CPPPInputBool()";
        case CpppType::Char:
            return "CPPPInputChar()";
        case CpppType::Int:
            return "CPPPInputInt()";
        case CpppType::Float:
            return "CPPPInputFloat()";
        case CpppType::Unknown:
            return "";
    }

    return "";
}

ExpressionEmitResult emitExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, CpppType>& declaredVariables,
    bool emitRuntimeChecks
) {
    ExpressionParser parser(inputFile, lineNumber, expressionText, expressionColumn, sourceLines, declaredVariables, emitRuntimeChecks || expressionRuntimeChecksEnabled());
    return parser.parse();
}

void setExpressionRuntimeChecksEnabled(bool enabled) {
    expressionRuntimeChecksEnabled() = enabled;
}

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
    }

    return false;
}
