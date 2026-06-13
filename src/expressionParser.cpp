#include "expressionParser.h"

namespace {
std::string cppTypeForExpressionType(const Type& type) {
    switch (type.primitive) {
        case PrimitiveType::Bool:
            return "bool";
        case PrimitiveType::Char:
            return "CPPPChar";
        case PrimitiveType::Int:
            return "long long";
        case PrimitiveType::Float:
            return "long double";
        case PrimitiveType::List:
            if (type.subtypes.size() == 1) {
                return "vector<" + cppTypeForExpressionType(type.subtypes[0]) + ">";
            }
            return "";
        case PrimitiveType::Unknown:
            return "";
    }

    return "";
}
}

ExpressionParser::ExpressionParser(
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

ExpressionEmitResult ExpressionParser::parse() {
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

bool ExpressionParser::atEnd() const {
    return peek().kind == TokenKind::EndOfFile;
}

const Token& ExpressionParser::peek() const {
    return tokens[current];
}

const Token& ExpressionParser::previous() const {
    return tokens[current - 1];
}

bool ExpressionParser::match(TokenKind kind, const std::string& text) {
    if (peek().kind != kind || (!text.empty() && peek().text != text)) {
        return false;
    }

    ++current;
    return true;
}

bool ExpressionParser::check(TokenKind kind, const std::string& text) const {
    return peek().kind == kind && (text.empty() || peek().text == text);
}

bool ExpressionParser::isOperator(const std::string& text) const {
    return check(TokenKind::Operator, text);
}

bool ExpressionParser::isUnterminatedQuotedToken(const Token& token) const {
    if (token.kind != TokenKind::String && token.kind != TokenKind::Char) {
        return false;
    }

    return token.text.size() < 2 || token.text.front() != token.text.back();
}

int ExpressionParser::absoluteColumn(const Token& token) const {
    return expressionColumn + token.span.startColumn - 1;
}

void ExpressionParser::report(const Token& token, const std::string& message) const {
    recordSourceError(inputFile, lineNumber, absoluteColumn(token), message, sourceLines);
}

bool ExpressionParser::reportInputUsageError(const Token& inputToken) const {
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

ExpressionParser::ParsedExpression ExpressionParser::parseExpression() {
    return parseLogicalOr();
}

ExpressionParser::ParsedExpression ExpressionParser::parseLogicalOr() {
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

ExpressionParser::ParsedExpression ExpressionParser::parseLogicalAnd() {
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

ExpressionParser::ParsedExpression ExpressionParser::parseBitwiseOr() {
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

ExpressionParser::ParsedExpression ExpressionParser::parseBitwiseXor() {
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

ExpressionParser::ParsedExpression ExpressionParser::parseBitwiseAnd() {
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

ExpressionParser::ParsedExpression ExpressionParser::parseEquality() {
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

ExpressionParser::ParsedExpression ExpressionParser::parseComparison() {
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

ExpressionParser::ParsedExpression ExpressionParser::parseShift() {
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

ExpressionParser::ParsedExpression ExpressionParser::parseAdditive() {
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

ExpressionParser::ParsedExpression ExpressionParser::parseMultiplicative() {
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

ExpressionParser::ParsedExpression ExpressionParser::parseUnary() {
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

ExpressionParser::ParsedExpression ExpressionParser::parsePostfix() {
    ParsedExpression expression = parsePrimary();
    if (!expression.ok) {
        return expression;
    }

    while (check(TokenKind::LeftBracket) ||
           (check(TokenKind::Operator, ".") && current + 1 < tokens.size() && tokens[current + 1].kind == TokenKind::Identifier) ||
           isOperator("++") || isOperator("--")) {
        if (match(TokenKind::LeftBracket)) {
            const Token& leftBracket = previous();
            if (expression.type.primitive != PrimitiveType::List || expression.type.subtypes.size() != 1) {
                report(leftBracket, "indexing requires a List value");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            const ParsedExpression index = parseExpression();
            if (!index.ok) {
                return index;
            }

            if (!match(TokenKind::RightBracket)) {
                report(leftBracket, "unclosed bracket in list index");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            if (index.type != PrimitiveType::Int) {
                report(leftBracket, "list index must be int");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            if (emitRuntimeChecks) {
                expression.text = "CPPPListAt(" + expression.text + ", " + index.text + ", " + std::to_string(lineNumber) + ", " + std::to_string(absoluteColumn(leftBracket)) + ")";
            } else {
                expression.text = "(" + expression.text + "[" + index.text + "])";
            }
            expression.type = expression.type.subtypes[0];
            expression.explicitCast = false;
            expression.mutableValue = false;
            continue;
        }

        if (match(TokenKind::Operator, ".")) {
            if (!match(TokenKind::Identifier)) {
                report(previous(), "expected method name after '.'");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            const Token& method = previous();
            if (method.text != "remove") {
                report(method, "unexpected token in expression");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            current -= 2;
            expression = parseListMethodCall(expression);
            if (!expression.ok) {
                return expression;
            }
            continue;
        }

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

ExpressionParser::ParsedExpression ExpressionParser::parseListMethodCall(ParsedExpression expression) {
    const Token& dot = peek();
    ++current;
    const Token& method = peek();
    ++current;

    if (expression.type.primitive != PrimitiveType::List || expression.type.subtypes.size() != 1) {
        report(method, "remove() can only be used on List values");
        return {false, "", PrimitiveType::Unknown, false, 0, false};
    }

    if (!expression.mutableValue) {
        report(method, "remove() requires a mutable List variable");
        return {false, "", PrimitiveType::Unknown, false, 0, false};
    }

    if (!match(TokenKind::LeftParen)) {
        report(method, "remove must be called as remove() or remove(index)");
        return {false, "", PrimitiveType::Unknown, false, 0, false};
    }

    const Token& leftParen = previous();
    const Type elementType = expression.type.subtypes[0];

    if (match(TokenKind::RightParen)) {
        if (emitRuntimeChecks) {
            return {
                true,
                "CPPPListPop(" + expression.text + ", " + std::to_string(lineNumber) + ", " + std::to_string(absoluteColumn(method)) + ")",
                elementType,
                false,
                absoluteColumn(dot),
                false
            };
        }

        return {
            true,
            "([&]() { auto __cppp_removed = (" + expression.text + ").back(); (" + expression.text + ").pop_back(); return __cppp_removed; }())",
            elementType,
            false,
            absoluteColumn(dot),
            false
        };
    }

    const ParsedExpression index = parseExpression();
    if (!index.ok) {
        return index;
    }

    if (!match(TokenKind::RightParen)) {
        report(leftParen, "remove() expects no arguments or index");
        return {false, "", PrimitiveType::Unknown, false, 0, false};
    }

    if (index.type != PrimitiveType::Int && !index.explicitCast && !isImplicitlyConvertible(index.type, PrimitiveType::Int)) {
        report(method, "list index must be int");
        return {false, "", PrimitiveType::Unknown, false, 0, false};
    }

    std::string emittedIndex = index.text;
    if (!isImplicitlyConvertible(index.type, PrimitiveType::Int) || index.type != PrimitiveType::Int) {
        emittedIndex = castExpressionTo(emittedIndex, index.type, PrimitiveType::Int);
    }

    if (emitRuntimeChecks) {
        return {
            true,
            "CPPPListRemoveAt(" + expression.text + ", " + emittedIndex + ", " + std::to_string(lineNumber) + ", " + std::to_string(absoluteColumn(method)) + ")",
            elementType,
            false,
            absoluteColumn(dot),
            false
        };
    }

    return {
        true,
        "([&]() { auto __cppp_removed = (" + expression.text + ")[" + emittedIndex + "]; (" + expression.text + ").erase((" + expression.text + ").begin() + " + emittedIndex + "); return __cppp_removed; }())",
        elementType,
        false,
        absoluteColumn(dot),
        false
    };
}

ExpressionParser::ParsedExpression ExpressionParser::parsePrimary() {
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

        if (identifier.text == "len") {
            if (!match(TokenKind::LeftParen)) {
                report(identifier, "len must be called as len(list)");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            const Token& leftParen = previous();
            const ParsedExpression list = parseExpression();
            if (!list.ok) {
                return list;
            }

            if (!match(TokenKind::RightParen)) {
                report(leftParen, "unclosed parenthesis in len");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            if (list.type.primitive != PrimitiveType::List || list.type.subtypes.size() != 1) {
                report(identifier, "len() expects a List value");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            return {
                true,
                "static_cast<long long>((" + list.text + ").size())",
                PrimitiveType::Int,
                false,
                absoluteColumn(identifier),
                false
            };
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

    if (match(TokenKind::LeftBracket)) {
        const Token& leftBracket = previous();
        if (match(TokenKind::RightBracket)) {
            report(leftBracket, "empty list literal needs a declared List type");
            return {false, "", PrimitiveType::Unknown, false, 0, false};
        }

        std::vector<std::string> elements;
        ParsedExpression element = parseExpression();
        if (!element.ok) {
            return element;
        }

        if (element.type == PrimitiveType::Unknown) {
            report(leftBracket, "list literal elements must have a known CP++ type");
            return {false, "", PrimitiveType::Unknown, false, 0, false};
        }

        Type elementType = element.type;
        elements.push_back(element.text);

        while (match(TokenKind::Comma)) {
            const Token& comma = previous();
            if (check(TokenKind::RightBracket) || atEnd()) {
                report(comma, "expected expression after ',' in list literal");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            ParsedExpression nextElement = parseExpression();
            if (!nextElement.ok) {
                return nextElement;
            }

            if (!isImplicitlyConvertible(nextElement.type, elementType)) {
                report(comma, "cannot implicitly convert " + cpppTypeName(nextElement.type) + " to " + cpppTypeName(elementType) + " in list literal");
                return {false, "", PrimitiveType::Unknown, false, 0, false};
            }

            std::string emittedElement = nextElement.text;
            if (nextElement.type != elementType) {
                emittedElement = castExpressionTo(emittedElement, nextElement.type, elementType);
            }
            elements.push_back(emittedElement);
        }

        if (!match(TokenKind::RightBracket)) {
            report(leftBracket, "unclosed bracket in list literal");
            return {false, "", PrimitiveType::Unknown, false, 0, false};
        }

        const std::string cppType = cppTypeForExpressionType(elementType);
        if (cppType.empty()) {
            report(leftBracket, "list literal elements must have a known CP++ type");
            return {false, "", PrimitiveType::Unknown, false, 0, false};
        }

        std::string generated = "vector<" + cppType + ">{";
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) {
                generated += ", ";
            }
            generated += elements[i];
        }
        generated += "}";

        return {
            true,
            generated,
            Type(PrimitiveType::List, {elementType}),
            false,
            absoluteColumn(leftBracket),
            false
        };
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

bool ExpressionParser::isTypeName(const std::string& name) const {
    return declaredTypeForName(name) != PrimitiveType::Unknown;
}

Type ExpressionParser::binaryResultType(Type left, Type right, const std::string& op) const {
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

bool ExpressionParser::isValueType(Type type) const {
    return type != PrimitiveType::Unknown;
}

bool ExpressionParser::isNumericType(Type type) const {
    return type == PrimitiveType::Bool ||
        type == PrimitiveType::Char ||
        type == PrimitiveType::Int ||
        type == PrimitiveType::Float;
}

bool ExpressionParser::isBitwiseType(Type type) const {
    return type == PrimitiveType::Bool || type == PrimitiveType::Char || type == PrimitiveType::Int;
}

bool ExpressionParser::isIncrementableType(Type type) const {
    return type == PrimitiveType::Char ||
        type == PrimitiveType::Int ||
        type == PrimitiveType::Float;
}

bool ExpressionParser::isComparable(Type left, Type right) const {
    return isValueType(left) && isValueType(right);
}

bool ExpressionParser::isFloatType(Type type) const {
    return type == PrimitiveType::Float;
}

std::string ExpressionParser::runtimeErrorThrowExpression(int column, const std::string& message) const {
    return "throw runtime_error(\"" + std::to_string(lineNumber) + ":" + std::to_string(column) + ":" + message + "\")";
}

std::string ExpressionParser::checkedIntegerExpression(
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
