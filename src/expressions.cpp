#include "expressions.h"

namespace {
struct ParsedExpression {
    bool ok;
    std::string text;
    Type type;
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
        const std::map<std::string, Type>& declaredVariables,
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
                return {false, "", PrimitiveType::Unknown, false, {}};
            }
        }

        const ParsedExpression expression = parseExpression();
        if (!expression.ok) {
            return {false, "", PrimitiveType::Unknown, false, {}};
        }

        if (!atEnd()) {
            report(peek(), "unexpected token in expression");
            return {false, "", PrimitiveType::Unknown, false, {}};
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
    const std::map<std::string, Type>& declaredVariables;
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
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            expression.text = "(" + castExpressionTo(expression.text, expression.type, PrimitiveType::Bool) + " " + op.text + " " + castExpressionTo(right.text, right.type, PrimitiveType::Bool) + ")";
            expression.type = PrimitiveType::Bool;
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
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            expression.text = "(" + castExpressionTo(expression.text, expression.type, PrimitiveType::Bool) + " " + op.text + " " + castExpressionTo(right.text, right.type, PrimitiveType::Bool) + ")";
            expression.type = PrimitiveType::Bool;
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
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == PrimitiveType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = PrimitiveType::Int;
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
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == PrimitiveType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = PrimitiveType::Int;
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
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == PrimitiveType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = PrimitiveType::Int;
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
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == PrimitiveType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = PrimitiveType::Bool;
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
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == PrimitiveType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = PrimitiveType::Bool;
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
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks && expression.type == PrimitiveType::Int) {
                expression.text = checkedIntegerExpression(expression.text, right.text, op.text, absoluteColumn(op));
            } else {
                expression.text = "(" + expression.text + " " + op.text + " " + right.text + ")";
            }
            expression.type = PrimitiveType::Int;
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

            const Type leftType = expression.type;
            const Type rightType = right.type;
            expression.type = binaryResultType(leftType, rightType, op.text);
            expression.explicitCast = false;
            if (expression.type == PrimitiveType::Unknown) {
                report(op, "cannot mix " + cpppTypeName(leftType) + " and " + cpppTypeName(rightType) + " with '" + op.text + "'");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }
            if (emitRuntimeChecks && expression.type == PrimitiveType::Int) {
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

            const Type leftType = expression.type;
            const Type rightType = right.type;
            expression.type = binaryResultType(leftType, rightType, op.text);
            expression.explicitCast = false;
            if (expression.type == PrimitiveType::Unknown) {
                report(op, "cannot mix " + cpppTypeName(leftType) + " and " + cpppTypeName(rightType) + " with '" + op.text + "'");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }
            if (emitRuntimeChecks && expression.type == PrimitiveType::Int) {
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
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            const Token& identifier = previous();
            const auto variable = declaredVariables.find(identifier.text);
            if (variable == declaredVariables.end()) {
                report(identifier, "use of undeclared variable '" + identifier.text + "'");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            if (!isIncrementableType(variable->second)) {
                report(op, "cannot use '" + op.text + "' with " + cpppTypeName(variable->second));
                return {false, "", PrimitiveType::Unknown, false, 0, false};
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
                    return {false, "", PrimitiveType::Unknown, false, 0, false};
                }

                return {true, "(!" + castExpressionTo(right.text, right.type, PrimitiveType::Bool) + ")", PrimitiveType::Bool, false, absoluteColumn(op), false};
            }

            if (!isNumericType(right.type)) {
                report(op, "cannot use unary '" + op.text + "' with " + cpppTypeName(right.type));
                return {false, "", PrimitiveType::Unknown, false, 0, false};
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
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            if (!isIncrementableType(expression.type)) {
                report(op, "cannot use '" + op.text + "' with " + cpppTypeName(expression.type));
                return {false, "", PrimitiveType::Unknown, false, 0, false};
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

            const Type targetType = declaredTypeForName(typeToken.text);
            return {
                true,
                castExpressionTo(expression.text, expression.type, targetType),
                targetType,
                true,
                absoluteColumn(typeToken),
                false
            };
        }

        if (match(TokenKind::Identifier)) {
            const Token& identifier = previous();
            if (identifier.text == "true" || identifier.text == "false") {
                return {true, identifier.text, PrimitiveType::Bool, false, absoluteColumn(identifier), false};
            }

            if (identifier.text == "input") {
                reportInputUsageError(identifier);
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            const auto variable = declaredVariables.find(identifier.text);
            if (variable == declaredVariables.end()) {
                report(identifier, "use of undeclared variable '" + identifier.text + "'");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            return {true, identifier.text, variable->second, false, absoluteColumn(identifier), true};
        }

        if (match(TokenKind::Integer)) {
            const Token& literal = previous();
            return {true, literal.text, PrimitiveType::Int, false, absoluteColumn(literal), false};
        }

        if (match(TokenKind::Float)) {
            const Token& literal = previous();
            return {true, literal.text, PrimitiveType::Float, false, absoluteColumn(literal), false};
        }

        if (match(TokenKind::String)) {
            const Token& literal = previous();
            return {true, literal.text, PrimitiveType::Unknown, false, absoluteColumn(literal), false};
        }

        if (match(TokenKind::Char)) {
            const Token& literal = previous();
            return {true, "CPPPChar(" + literal.text + ")", PrimitiveType::Char, false, absoluteColumn(literal), false};
        }

        if (match(TokenKind::LeftParen)) {
            const Token& leftParen = previous();
            const ParsedExpression expression = parseExpression();
            if (!expression.ok) {
                return expression;
            }

            if (!match(TokenKind::RightParen)) {
                report(leftParen, "unclosed parenthesis in expression");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            return {true, "(" + expression.text + ")", expression.type, expression.explicitCast, absoluteColumn(leftParen), false};
        }

        report(peek(), "expected expression");
        return {false, "", PrimitiveType::Unknown, false, 0, false};
    }

    bool isTypeName(const std::string& name) const {
        return declaredTypeForName(name) != PrimitiveType::Unknown;
    }

    Type binaryResultType(Type left, Type right, const std::string& op) const {
        if (!isNumericType(left) || !isNumericType(right)) {
            return PrimitiveType::Unknown;
        }

        if (op == "%" && (isFloatType(left) || isFloatType(right))) {
            return PrimitiveType::Unknown;
        }

        if (left == PrimitiveType::Float || right == PrimitiveType::Float) {
            return PrimitiveType::Float;
        }

        return PrimitiveType::Int;
    }

    bool isValueType(Type type) const {
        return type != PrimitiveType::Unknown;
    }

    bool isNumericType(Type type) const {
        return type == PrimitiveType::Bool ||
            type == PrimitiveType::Char ||
            type == PrimitiveType::Int ||
            type == PrimitiveType::Float;
    }

    bool isBitwiseType(Type type) const {
        return type == PrimitiveType::Bool || type == PrimitiveType::Char || type == PrimitiveType::Int;
    }

    bool isIncrementableType(Type type) const {
        return type == PrimitiveType::Char ||
            type == PrimitiveType::Int ||
            type == PrimitiveType::Float;
    }

    bool isComparable(Type left, Type right) const {
        if (!isValueType(left) || !isValueType(right)) {
            return false;
        }

        return true;
    }

    bool isFloatType(Type type) const {
        return type == PrimitiveType::Float;
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

int primitiveArity(PrimitiveType primitive) {
    switch (primitive) {
        case PrimitiveType::Bool:
        case PrimitiveType::Char:
        case PrimitiveType::Int:
        case PrimitiveType::Float:
            return 0;
        case PrimitiveType::Unknown:
            return 0;
    }

    return 0;
}

std::string cpppTypeName(const Type& type) {
    switch (type.primitive) {
        case PrimitiveType::Bool:
            return "bool";
        case PrimitiveType::Char:
            return "char";
        case PrimitiveType::Int:
            return "int";
        case PrimitiveType::Float:
            return "float";
        case PrimitiveType::Unknown:
            return "unknown";
    }

    return "unknown";
}

bool isImplicitlyConvertible(const Type& from, const Type& to) {
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
        return to == PrimitiveType::Bool || to == PrimitiveType::Float;
    }

    if (from == PrimitiveType::Float) {
        return to == PrimitiveType::Bool;
    }

    return false;
}

std::string castExpressionTo(const std::string& expression, const Type& to) {
    return castExpressionTo(expression, PrimitiveType::Unknown, to);
}

std::string castExpressionTo(const std::string& expression, const Type& from, const Type& to) {
    switch (to.primitive) {
        case PrimitiveType::Bool:
            switch (from.primitive) {
                case PrimitiveType::Bool:
                    return "CPPPToBoolBool(" + expression + ")";
                case PrimitiveType::Char:
                    return "CPPPToBoolChar(" + expression + ")";
                case PrimitiveType::Int:
                    return "CPPPToBoolInt(" + expression + ")";
                case PrimitiveType::Float:
                    return "CPPPToBoolFloat(" + expression + ")";
                case PrimitiveType::Unknown:
                    return "CPPPToBool(" + expression + ")";
            }
            return "CPPPToBool(" + expression + ")";
        case PrimitiveType::Char:
            return "CPPPChar(static_cast<char>(" + expression + "))";
        case PrimitiveType::Int:
            return "static_cast<long long>(" + expression + ")";
        case PrimitiveType::Float:
            return "static_cast<long double>(" + expression + ")";
        case PrimitiveType::Unknown:
            return expression;
    }

    return expression;
}

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

    return PrimitiveType::Unknown;
}

bool isInputCall(const std::vector<Token>& tokens) {
    return tokens.size() == 4 &&
        tokens[0].kind == TokenKind::Identifier &&
        tokens[0].text == "input" &&
        tokens[1].kind == TokenKind::LeftParen &&
        tokens[2].kind == TokenKind::RightParen &&
        tokens[3].kind == TokenKind::EndOfFile;
}

std::string inputFunctionForType(const Type& type) {
    switch (type.primitive) {
        case PrimitiveType::Bool:
            return "CPPPInputBool()";
        case PrimitiveType::Char:
            return "CPPPInputChar()";
        case PrimitiveType::Int:
            return "CPPPInputInt()";
        case PrimitiveType::Float:
            return "CPPPInputFloat()";
        case PrimitiveType::Unknown:
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
    const std::map<std::string, Type>& declaredVariables,
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
