#include "expressions.h"

namespace {
struct ParsedExpression {
    bool ok;
    std::string text;
    CpppType type;
    bool explicitCast;
    int sourceColumn;
};

class ExpressionParser {
public:
    ExpressionParser(
        const std::string& inputFile,
        int lineNumber,
        const std::string& expressionText,
        int expressionColumn,
        const std::map<int, std::string>& sourceLines,
        const std::map<std::string, CpppType>& declaredVariables
    ) :
        inputFile(inputFile),
        lineNumber(lineNumber),
        expressionText(expressionText),
        expressionColumn(expressionColumn),
        sourceLines(sourceLines),
        declaredVariables(declaredVariables),
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

    ParsedExpression parseExpression() {
        return parseAdditive();
    }

    ParsedExpression parseAdditive() {
        ParsedExpression expression = parseMultiplicative();
        if (!expression.ok) {
            return expression;
        }

        while (isOperator("+") || isOperator("-")) {
            const std::string op = peek().text;
            ++current;
            const ParsedExpression right = parseMultiplicative();
            if (!right.ok) {
                return right;
            }

            const CpppType leftType = expression.type;
            const CpppType rightType = right.type;
            expression.text = "(" + expression.text + " " + op + " " + right.text + ")";
            expression.type = binaryResultType(leftType, rightType, op);
            expression.explicitCast = false;
            if (expression.type == CpppType::Unknown) {
                report(previous(), "cannot mix " + cpppTypeName(leftType) + " and " + cpppTypeName(rightType) + " with '" + op + "'");
                return {false, "", CpppType::Unknown, false, 0};
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
            const std::string op = peek().text;
            ++current;
            const ParsedExpression right = parseUnary();
            if (!right.ok) {
                return right;
            }

            const CpppType leftType = expression.type;
            const CpppType rightType = right.type;
            expression.text = "(" + expression.text + " " + op + " " + right.text + ")";
            expression.type = binaryResultType(leftType, rightType, op);
            expression.explicitCast = false;
            if (expression.type == CpppType::Unknown) {
                report(previous(), "cannot mix " + cpppTypeName(leftType) + " and " + cpppTypeName(rightType) + " with '" + op + "'");
                return {false, "", CpppType::Unknown, false, 0};
            }
        }

        return expression;
    }

    ParsedExpression parseUnary() {
        if (isOperator("+") || isOperator("-")) {
            const Token op = peek();
            ++current;
            const ParsedExpression right = parseUnary();
            if (!right.ok) {
                return right;
            }

            return {true, "(" + op.text + right.text + ")", right.type, false, absoluteColumn(op)};
        }

        return parsePrimary();
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
                absoluteColumn(typeToken)
            };
        }

        if (match(TokenKind::Identifier)) {
            const Token& identifier = previous();
            if (identifier.text == "true" || identifier.text == "false") {
                return {true, identifier.text, CpppType::Bool, false, absoluteColumn(identifier)};
            }

            const auto variable = declaredVariables.find(identifier.text);
            if (variable == declaredVariables.end()) {
                report(identifier, "use of undeclared variable '" + identifier.text + "'");
                return {false, "", CpppType::Unknown, false, 0};
            }

            return {true, identifier.text, variable->second, false, absoluteColumn(identifier)};
        }

        if (match(TokenKind::Integer)) {
            const Token& literal = previous();
            return {true, literal.text, CpppType::Int, false, absoluteColumn(literal)};
        }

        if (match(TokenKind::Float)) {
            const Token& literal = previous();
            return {true, literal.text, CpppType::Float, false, absoluteColumn(literal)};
        }

        if (match(TokenKind::String)) {
            const Token& literal = previous();
            return {true, literal.text, CpppType::Unknown, false, absoluteColumn(literal)};
        }

        if (match(TokenKind::Char)) {
            const Token& literal = previous();
            return {true, "CPPPChar(" + literal.text + ")", CpppType::Char, false, absoluteColumn(literal)};
        }

        if (match(TokenKind::LeftParen)) {
            const Token& leftParen = previous();
            const ParsedExpression expression = parseExpression();
            if (!expression.ok) {
                return expression;
            }

            if (!match(TokenKind::RightParen)) {
                report(leftParen, "unclosed parenthesis in expression");
                return {false, "", CpppType::Unknown, false, 0};
            }

            return {true, "(" + expression.text + ")", expression.type, expression.explicitCast, absoluteColumn(leftParen)};
        }

        report(peek(), "expected expression");
        return {false, "", CpppType::Unknown, false, 0};
    }

    bool isTypeName(const std::string& name) const {
        return declaredTypeForName(name) != CpppType::Unknown;
    }

    CpppType binaryResultType(CpppType left, CpppType right, const std::string& op) const {
        if (op == "%" && (isFloatType(left) || isFloatType(right))) {
            return CpppType::Unknown;
        }

        if ((isBigIntType(left) && isFloatType(right)) ||
            (isFloatType(left) && isBigIntType(right))) {
            return CpppType::Unknown;
        }

        if (left == CpppType::BigFloat || right == CpppType::BigFloat) {
            return CpppType::BigFloat;
        }

        if (left == CpppType::Float || right == CpppType::Float) {
            return CpppType::Float;
        }

        if (left == CpppType::BigInt || right == CpppType::BigInt) {
            return CpppType::BigInt;
        }

        return CpppType::Int;
    }

    bool isBigIntType(CpppType type) const {
        return type == CpppType::BigInt;
    }

    bool isFloatType(CpppType type) const {
        return type == CpppType::Float || type == CpppType::BigFloat;
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
        case CpppType::BigInt:
            return "bigint";
        case CpppType::Float:
            return "float";
        case CpppType::BigFloat:
            return "bigfloat";
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
        return to == CpppType::Char || to == CpppType::Int || to == CpppType::BigInt || to == CpppType::Float || to == CpppType::BigFloat;
    }

    if (from == CpppType::Char) {
        return to == CpppType::Int || to == CpppType::BigInt || to == CpppType::Float || to == CpppType::BigFloat;
    }

    if (from == CpppType::Int) {
        return to == CpppType::BigInt || to == CpppType::Float || to == CpppType::BigFloat;
    }

    if (from == CpppType::Float) {
        return to == CpppType::BigFloat;
    }

    return false;
}

std::string castExpressionTo(const std::string& expression, CpppType to) {
    switch (to) {
        case CpppType::Bool:
            return "static_cast<bool>(" + expression + ")";
        case CpppType::Char:
            return "CPPPChar(static_cast<char>(" + expression + "))";
        case CpppType::Int:
            return "static_cast<long long>(" + expression + ")";
        case CpppType::BigInt:
            return "CPPPBigInt(static_cast<long long>(" + expression + "))";
        case CpppType::Float:
        case CpppType::BigFloat:
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
    if (name == "bigint" || name == "Bigint") {
        return CpppType::BigInt;
    }
    if (name == "float") {
        return CpppType::Float;
    }
    if (name == "bigfloat" || name == "BigFloat") {
        return CpppType::BigFloat;
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
        case CpppType::BigInt:
            return "CPPPInputBigInt()";
        case CpppType::Float:
        case CpppType::BigFloat:
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
    const std::map<std::string, CpppType>& declaredVariables
) {
    ExpressionParser parser(inputFile, lineNumber, expressionText, expressionColumn, sourceLines, declaredVariables);
    return parser.parse();
}

bool hasArithmeticOperator(const std::vector<Token>& tokens) {
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::Operator &&
            (token.text == "+" || token.text == "-" || token.text == "*" || token.text == "/" || token.text == "%")) {
            return true;
        }
    }

    return false;
}
