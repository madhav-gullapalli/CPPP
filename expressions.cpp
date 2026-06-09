#include "expressions.h"

namespace {
struct ParsedExpression {
    bool ok;
    std::string text;
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
        const std::set<std::string>& declaredVariables
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
                return {false, "", {}};
            }
        }

        const ParsedExpression expression = parseExpression();
        if (!expression.ok) {
            return {false, "", {}};
        }

        if (!atEnd()) {
            report(peek(), "unexpected token in expression");
            return {false, "", {}};
        }

        return {
            true,
            expression.text,
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
    const std::set<std::string>& declaredVariables;
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

            expression.text = "(" + expression.text + " " + op + " " + right.text + ")";
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

            expression.text = "(" + expression.text + " " + op + " " + right.text + ")";
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

            return {true, "(" + op.text + right.text + ")", absoluteColumn(op)};
        }

        return parsePrimary();
    }

    ParsedExpression parsePrimary() {
        if (match(TokenKind::Identifier)) {
            const Token& identifier = previous();
            if (declaredVariables.count(identifier.text) == 0) {
                report(identifier, "use of undeclared variable '" + identifier.text + "'");
                return {false, "", 0};
            }

            return {true, identifier.text, absoluteColumn(identifier)};
        }

        if (match(TokenKind::Integer) || match(TokenKind::Float) || match(TokenKind::String) || match(TokenKind::Char)) {
            const Token& literal = previous();
            return {true, literal.text, absoluteColumn(literal)};
        }

        if (match(TokenKind::LeftParen)) {
            const Token& leftParen = previous();
            const ParsedExpression expression = parseExpression();
            if (!expression.ok) {
                return expression;
            }

            if (!match(TokenKind::RightParen)) {
                report(leftParen, "unclosed parenthesis in expression");
                return {false, "", 0};
            }

            return {true, "(" + expression.text + ")", absoluteColumn(leftParen)};
        }

        report(peek(), "expected expression");
        return {false, "", 0};
    }
};
}

ExpressionEmitResult emitExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::set<std::string>& declaredVariables
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
