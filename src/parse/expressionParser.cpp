/*
 * expressionParser.cpp
 *
 * Parses expression syntax into the expression AST.
 */

#include "expressionParser.h"

#include <memory>

ExpressionParser::ExpressionParser(
    const std::string& inputFile,
    int lineNumber,
    const std::vector<Token>& expressionTokens,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    bool syntaxOnly
) :
    inputFile(inputFile),
    lineNumber(lineNumber),
    expressionColumn(expressionColumn),
    sourceLines(sourceLines),
    syntaxOnly(syntaxOnly),
    tokens(expressionTokens) {
    if (tokens.empty() || tokens.back().kind != TokenKind::EndOfFile) {
        SourceSpan endSpan;
        int endColumn = expressionColumn;
        if (!tokens.empty()) {
            endColumn = tokens.back().span.endColumn + 1;
            if (tokens.back().sourceSpan.valid()) {
                endSpan = {
                    tokens.back().sourceSpan.source,
                    tokens.back().sourceSpan.endOffset,
                    tokens.back().sourceSpan.endOffset
                };
            }
        }
        tokens.push_back({TokenKind::EndOfFile, "", {1, endColumn, 1, endColumn, 0, 0}, endSpan});
    }
}

std::unique_ptr<Expr> ExpressionParser::parseAst(bool& ok) {
    std::unique_ptr<Expr> expression = parseExpression(ok);
    if (!ok) {
        return nullptr;
    }
    if (!atEnd()) {
        reportUnexpectedTrailingToken(peek());
        ok = false;
        return nullptr;
    }
    return expression;
}

int ExpressionParser::failureColumn() const {
    return firstFailureColumn;
}

SourceSpan ExpressionParser::failureSpan() const {
    return firstFailureSpan;
}

const std::string& ExpressionParser::failureMessage() const {
    return firstFailureMessage;
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

int ExpressionParser::absoluteEndColumn(const Token& token) const {
    return expressionColumn + token.span.endColumn - 1;
}

void ExpressionParser::recordFailure(const Token& token, const std::string& message) const {
    if (firstFailureColumn != 0) return;
    firstFailureColumn = absoluteColumn(token);
    firstFailureSpan = token.sourceSpan;
    firstFailureMessage = message;
}

void ExpressionParser::report(const Token& token, const std::string& message) const {
    recordFailure(token, message);
    if (syntaxOnly) return;
    const SourceSpan tokenSpan = token.sourceSpan.valid()
        ? token.sourceSpan
        : sourceSpanForColumns(
            inputFile,
            sourceLines,
            lineNumber,
            absoluteColumn(token),
            absoluteEndColumn(token)
        );
    Diagnostic diagnostic;
    diagnostic.message = message;
    diagnostic.labels.push_back({tokenSpan, "", true});
    if ((message == "unterminated string literal" || message == "unterminated char literal") &&
        !token.text.empty()) {
        const char quote = token.kind == TokenKind::Char ? '\'' : '"';
        const SourceSpan insertion = sourceInsertionSpan(
            inputFile,
            sourceLines,
            lineNumber,
            absoluteEndColumn(token) + 1
        );
        diagnostic.suggestions.push_back({
            insertion,
            std::string(1, quote),
            std::string("add a closing `") + quote + "`",
            SuggestionApplicability::MachineApplicable
        });
    }
    addAutomaticSyntaxSuggestion(
        diagnostic,
        inputFile,
        lineNumber,
        absoluteColumn(token),
        sourceLines
    );
    recordDiagnostic(std::move(diagnostic));
}

void ExpressionParser::reportUnexpectedTrailingToken(const Token& token) const {
    recordFailure(token, "unexpected token in expression");
    if (syntaxOnly) return;
    Diagnostic diagnostic;
    diagnostic.message = "unexpected token in expression";
    const SourceSpan span = token.sourceSpan.valid()
        ? token.sourceSpan
        : sourceSpanForColumns(
            inputFile,
            sourceLines,
            lineNumber,
            absoluteColumn(token),
            absoluteEndColumn(token)
        );
    diagnostic.labels.push_back({span, "", true});
    if (token.kind == TokenKind::Equals) {
        diagnostic.suggestions.push_back({
            span,
            "==",
            "use `==` to compare values",
            SuggestionApplicability::MachineApplicable
        });
    } else if (current + 1 < tokens.size() &&
        tokens[current + 1].kind == TokenKind::EndOfFile) {
        diagnostic.suggestions.push_back({
            span,
            "",
            "remove unexpected '" + token.text + "'",
            SuggestionApplicability::MachineApplicable
        });
    }
    recordDiagnostic(std::move(diagnostic));
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

bool ExpressionParser::isTypeName(const std::string& name) const {
    return declaredTypeForName(name) != PrimitiveType::Unknown;
}

std::unique_ptr<Expr> ExpressionParser::parseExpression(bool& ok) { return parseLogicalOr(ok); }
std::unique_ptr<Expr> ExpressionParser::parseLogicalOr(bool& ok) {
    std::unique_ptr<Expr> expression = parseLogicalAnd(ok);
    while (ok && isOperator("||")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseLogicalAnd(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op), op.sourceSpan);
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseLogicalAnd(bool& ok) {
    std::unique_ptr<Expr> expression = parseBitwiseOr(ok);
    while (ok && isOperator("&&")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseBitwiseOr(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op), op.sourceSpan);
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseBitwiseOr(bool& ok) {
    std::unique_ptr<Expr> expression = parseBitwiseXor(ok);
    while (ok && isOperator("|")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseBitwiseXor(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op), op.sourceSpan);
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseBitwiseXor(bool& ok) {
    std::unique_ptr<Expr> expression = parseBitwiseAnd(ok);
    while (ok && isOperator("^")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseBitwiseAnd(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op), op.sourceSpan);
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseBitwiseAnd(bool& ok) {
    std::unique_ptr<Expr> expression = parseEquality(ok);
    while (ok && isOperator("&")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseEquality(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op), op.sourceSpan);
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseEquality(bool& ok) {
    std::unique_ptr<Expr> expression = parseComparison(ok);
    while (ok && (isOperator("==") || isOperator("!="))) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseComparison(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op), op.sourceSpan);
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseComparison(bool& ok) {
    std::unique_ptr<Expr> expression = parseShift(ok);
    while (ok && (isOperator("<") || isOperator("<=") || isOperator(">") || isOperator(">=") || check(TokenKind::Identifier, "in"))) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseShift(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op), op.sourceSpan);
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseShift(bool& ok) {
    std::unique_ptr<Expr> expression = parseAdditive(ok);
    while (ok && (isOperator("<<") || isOperator(">>"))) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseAdditive(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op), op.sourceSpan);
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseAdditive(bool& ok) {
    std::unique_ptr<Expr> expression = parseMultiplicative(ok);
    while (ok && (isOperator("+") || isOperator("-"))) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseMultiplicative(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op), op.sourceSpan);
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseMultiplicative(bool& ok) {
    std::unique_ptr<Expr> expression = parseUnary(ok);
    while (ok && (isOperator("*") || isOperator("/") || isOperator("%"))) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseUnary(ok);
        if (!ok) return nullptr;
        expression = std::make_unique<BinaryExpr>(op.text, std::move(expression), std::move(right), absoluteColumn(op), op.sourceSpan);
    }
    return expression;
}
std::unique_ptr<Expr> ExpressionParser::parseUnary(bool& ok) {
    if (isOperator("++") || isOperator("--") || isOperator("+") || isOperator("-") || isOperator("!")) {
        const Token op = peek();
        ++current;
        std::unique_ptr<Expr> right = parseUnary(ok);
        if (!ok) return nullptr;
        return std::make_unique<UnaryExpr>(op.text, std::move(right), absoluteColumn(op), false, op.sourceSpan);
    }
    return parsePostfix(ok);
}
std::unique_ptr<Expr> ExpressionParser::parsePostfix(bool& ok) {
    std::unique_ptr<Expr> expression = parsePrimary(ok);
    while (ok && (check(TokenKind::LeftBracket) ||
           (check(TokenKind::Operator, ".") && current + 1 < tokens.size() && tokens[current + 1].kind == TokenKind::Identifier) ||
           isOperator("++") || isOperator("--"))) {
        if (match(TokenKind::LeftBracket)) {
            const Token& leftBracket = previous();
            std::unique_ptr<Expr> start;
            if (!isOperator(":")) {
                start = parseLogicalOr(ok);
                if (!ok) return nullptr;
            }
            if (match(TokenKind::Operator, ":")) {
                std::unique_ptr<Expr> end;
                if (!check(TokenKind::RightBracket)) {
                    end = parseLogicalOr(ok);
                    if (!ok) return nullptr;
                }
                if (!match(TokenKind::RightBracket)) {
                    report(leftBracket, "unclosed bracket in list slice");
                    ok = false;
                    return nullptr;
                }
                expression = std::make_unique<SliceExpr>(std::move(expression), std::move(start), std::move(end), absoluteColumn(leftBracket), leftBracket.sourceSpan);
                continue;
            }
            if (!start) {
                report(leftBracket, "expected index or ':' after '['");
                ok = false;
                return nullptr;
            }
            if (!match(TokenKind::RightBracket)) {
                report(leftBracket, "unclosed bracket in list index");
                ok = false;
                return nullptr;
            }
            expression = std::make_unique<IndexExpr>(std::move(expression), std::move(start), absoluteColumn(leftBracket), leftBracket.sourceSpan);
            continue;
        }
        if (check(TokenKind::Operator, ".")) {
            expression = parseMethodCall(std::move(expression), ok);
            continue;
        }

        const Token op = peek();
        ++current;
        expression = std::make_unique<UnaryExpr>(op.text, std::move(expression), absoluteColumn(op), true, op.sourceSpan);
    }
    return expression;
}

std::unique_ptr<Expr> ExpressionParser::parseMethodCall(std::unique_ptr<Expr> expression, bool& ok) {
    const Token& dot = peek();
    ++current;
    if (!match(TokenKind::Identifier)) {
        report(dot, "expected method name after '.'");
        ok = false;
        return nullptr;
    }
    const Token& method = previous();
    if (!check(TokenKind::LeftParen)) {
        return std::make_unique<FieldExpr>(std::move(expression), method.text, absoluteColumn(method), method.sourceSpan);
    }
    if (syntaxOnly || (method.text != "remove" && method.text != "find" && method.text != "at" && method.text != "split" &&
        method.text != "prev" && method.text != "next" && method.text != "hasPrev" && method.text != "hasNext")) {
        if (match(TokenKind::LeftParen)) {
            const Token& leftParen = previous();
            std::vector<std::unique_ptr<Expr>> arguments;
            if (!match(TokenKind::RightParen)) {
                arguments.push_back(parseExpression(ok));
                while (ok && match(TokenKind::Comma)) arguments.push_back(parseExpression(ok));
                if (ok && !match(TokenKind::RightParen)) {
                    report(leftParen, "unclosed parenthesis in method call");
                    ok = false;
                    return nullptr;
                }
            }
            return std::make_unique<CallExpr>(method.text, std::move(expression), std::move(arguments), absoluteColumn(method), method.sourceSpan);
        }
        report(method, "unknown method '" + method.text + "'");
        ok = false;
        return nullptr;
    }
    if (!match(TokenKind::LeftParen)) {
        report(method, method.text + " must be called with parentheses");
        ok = false;
        return nullptr;
    }
    const Token& leftParen = previous();
    std::vector<std::unique_ptr<Expr>> arguments;
    if (!match(TokenKind::RightParen)) {
        arguments.push_back(parseExpression(ok));
        if (!ok) return nullptr;
        while (match(TokenKind::Comma)) {
            arguments.push_back(parseExpression(ok));
            if (!ok) return nullptr;
        }
        if (!match(TokenKind::RightParen)) {
            if (method.text == "remove") {
                report(leftParen, "remove() expects no arguments or index");
            } else if (method.text == "at") {
                report(leftParen, "at() expects exactly one key");
            } else if (method.text == "prev" || method.text == "next" || method.text == "hasPrev" || method.text == "hasNext") {
                report(leftParen, "unclosed parenthesis in " + method.text);
            } else if (method.text == "split") {
                report(leftParen, "split() expects exactly one delimiter");
            } else {
                report(leftParen, "find() expects exactly one value or sublist");
            }
            ok = false;
            return nullptr;
        }
    }
    if (method.text == "remove" && arguments.size() > 1) {
        report(leftParen, "remove() expects no arguments or index");
        ok = false;
        return nullptr;
    }
    if (method.text == "at" && arguments.size() != 1) {
        report(leftParen, "at() expects exactly one key");
        ok = false;
        return nullptr;
    }
    if ((method.text == "prev" || method.text == "next" || method.text == "hasPrev" || method.text == "hasNext") && arguments.size() != 1) {
        report(leftParen, method.text + "() expects exactly one key");
        ok = false;
        return nullptr;
    }
    if (method.text == "split" && arguments.size() != 1) {
        report(leftParen, "split() expects exactly one delimiter");
        ok = false;
        return nullptr;
    }
    if (method.text == "find" && arguments.size() != 1) {
        report(leftParen, "find() expects exactly one value or sublist");
        ok = false;
        return nullptr;
    }
    return std::make_unique<CallExpr>(method.text, std::move(expression), std::move(arguments), absoluteColumn(method), method.sourceSpan);
}

std::unique_ptr<Expr> ExpressionParser::parseBraceLiteral(bool& ok) {
    const Token& leftBrace = peek();
    ++current;
    if (check(TokenKind::RightBrace, "}")) {
        if (syntaxOnly) {
            ++current;
            return std::make_unique<SetLiteralExpr>(
                std::vector<std::unique_ptr<Expr>>{},
                absoluteColumn(leftBrace),
                leftBrace.sourceSpan
            );
        }
        report(leftBrace, "empty set or map literal needs a declared type");
        ok = false;
        return nullptr;
    }

    const size_t contentStart = current;
    int braceDepth = 1;
    while (current < tokens.size()) {
        const Token& token = tokens[current];
        if (token.kind == TokenKind::EndOfFile) {
            break;
        }
        if (token.kind == TokenKind::LeftBrace) {
            ++braceDepth;
        } else if (token.kind == TokenKind::RightBrace) {
            --braceDepth;
            if (braceDepth == 0) {
                break;
            }
        }
        ++current;
    }

    if (current >= tokens.size() || tokens[current].kind == TokenKind::EndOfFile) {
        report(leftBrace, "unclosed brace in set or map literal");
        ok = false;
        return nullptr;
    }

    const size_t contentEnd = current;
    ++current;

    auto parseSlice = [&](size_t startIndex, size_t endIndex) -> std::unique_ptr<Expr> {
        if (startIndex >= endIndex) return nullptr;
        const int startColumn = tokens[startIndex].span.startColumn;
        const size_t startOffset = tokens[startIndex].span.startOffset;
        std::vector<Token> slice;
        for (size_t index = startIndex; index < endIndex; ++index) {
            Token token = tokens[index];
            token.span.startColumn -= startColumn - 1;
            token.span.endColumn -= startColumn - 1;
            token.span.startOffset -= startOffset;
            token.span.endOffset -= startOffset;
            slice.push_back(std::move(token));
        }
        ExpressionParser parser(
            inputFile,
            lineNumber,
            slice,
            expressionColumn + startColumn - 1,
            sourceLines,
            syntaxOnly
        );
        bool sliceOk = true;
        std::unique_ptr<Expr> expression = parser.parseAst(sliceOk);
        return sliceOk ? std::move(expression) : nullptr;
    };

    struct EntrySlice {
        size_t start = 0;
        size_t end = 0;
        size_t colonIndex = 0;
        bool hasColon = false;
    };

    std::vector<EntrySlice> entries;
    size_t segmentStart = contentStart;
    int parenDepth = 0;
    int bracketDepth = 0;
    int nestedBraceDepth = 0;
    size_t topLevelColon = tokens.size();

    for (size_t index = contentStart; index < contentEnd; ++index) {
        const Token& token = tokens[index];
        if (token.kind == TokenKind::LeftParen) {
            ++parenDepth;
            continue;
        }
        if (token.kind == TokenKind::RightParen) {
            if (parenDepth > 0) {
                --parenDepth;
            }
            continue;
        }
        if (token.kind == TokenKind::LeftBracket) {
            ++bracketDepth;
            continue;
        }
        if (token.kind == TokenKind::RightBracket) {
            if (bracketDepth > 0) {
                --bracketDepth;
            }
            continue;
        }
        if (token.kind == TokenKind::LeftBrace) {
            ++nestedBraceDepth;
            continue;
        }
        if (token.kind == TokenKind::RightBrace) {
            if (nestedBraceDepth > 0) {
                --nestedBraceDepth;
            }
            continue;
        }

        if (parenDepth == 0 && bracketDepth == 0 && nestedBraceDepth == 0) {
            if (token.kind == TokenKind::Operator && token.text == ":" && topLevelColon == tokens.size()) {
                topLevelColon = index;
                continue;
            }
            if (token.kind == TokenKind::Comma) {
                if (segmentStart == index) {
                    report(token, "expected expression after ',' in set or map literal");
                    ok = false;
                    return nullptr;
                }
                entries.push_back({segmentStart, index, topLevelColon, topLevelColon != tokens.size()});
                segmentStart = index + 1;
                topLevelColon = tokens.size();
            }
        }
    }

    if (segmentStart >= contentEnd) {
        report(tokens[contentEnd - 1], "expected expression after ',' in set or map literal");
        ok = false;
        return nullptr;
    }
    entries.push_back({segmentStart, contentEnd, topLevelColon, topLevelColon != tokens.size()});

    bool mapLiteral = false;
    bool setLiteral = false;
    for (const EntrySlice& entry : entries) {
        mapLiteral = mapLiteral || entry.hasColon;
        setLiteral = setLiteral || !entry.hasColon;
    }

    if (mapLiteral && setLiteral) {
        report(leftBrace, "map literal entries must use key:value pairs");
        ok = false;
        return nullptr;
    }

    if (!mapLiteral) {
        std::vector<std::unique_ptr<Expr>> elements;
        for (const EntrySlice& entry : entries) {
            std::unique_ptr<Expr> element = parseSlice(entry.start, entry.end);
            if (!element) {
                ok = false;
                return nullptr;
            }
            elements.push_back(std::move(element));
        }
        return std::make_unique<SetLiteralExpr>(std::move(elements), absoluteColumn(leftBrace), leftBrace.sourceSpan);
    }

    std::vector<MapLiteralEntry> mapEntries;
    for (const EntrySlice& entry : entries) {
        if (!entry.hasColon || entry.colonIndex <= entry.start || entry.colonIndex + 1 >= entry.end) {
            report(leftBrace, "map literal entries must use key:value pairs");
            ok = false;
            return nullptr;
        }

        std::unique_ptr<Expr> key = parseSlice(entry.start, entry.colonIndex);
        std::unique_ptr<Expr> value = parseSlice(entry.colonIndex + 1, entry.end);
        if (!key || !value) {
            ok = false;
            return nullptr;
        }
        mapEntries.push_back({std::move(key), std::move(value)});
    }
    return std::make_unique<MapLiteralExpr>(std::move(mapEntries), absoluteColumn(leftBrace), leftBrace.sourceSpan);
}

std::unique_ptr<Expr> ExpressionParser::parsePrimary(bool& ok) {
    if (check(TokenKind::Identifier) &&
        peek().text != "range" &&
        isTypeName(peek().text) &&
        !isStructType(declaredTypeForName(peek().text)) &&
        current + 1 < tokens.size() &&
        tokens[current + 1].kind == TokenKind::LeftParen) {
        const Token typeToken = peek();
        const Token leftParen = tokens[current + 1];
        current += 2;
        std::unique_ptr<Expr> operand = parseExpression(ok);
        if (!ok) {
            return nullptr;
        }
        if (!match(TokenKind::RightParen)) {
            report(leftParen, "unclosed parenthesis in cast");
            ok = false;
            return nullptr;
        }
        return std::make_unique<CastExpr>(declaredTypeForName(typeToken.text), std::move(operand), absoluteColumn(typeToken), typeToken.sourceSpan);
    }

    if (match(TokenKind::LeftParen)) {
        const Token leftParen = previous();
        std::unique_ptr<Expr> expression = parseExpression(ok);
        if (!ok) {
            return nullptr;
        }

        if (match(TokenKind::Comma)) {
            std::unique_ptr<Expr> second = parseExpression(ok);
            if (!ok) {
                return nullptr;
            }
            if (!match(TokenKind::RightParen)) {
                report(leftParen, "unclosed parenthesis in pair literal");
                ok = false;
                return nullptr;
            }
            return std::make_unique<PairLiteralExpr>(std::move(expression), std::move(second), absoluteColumn(leftParen), leftParen.sourceSpan);
        }

        if (!match(TokenKind::RightParen)) {
            report(leftParen, "unclosed parenthesis in expression");
            ok = false;
            return nullptr;
        }
        return expression;
    }

    if (match(TokenKind::Identifier)) {
        const Token& identifier = previous();
        if (identifier.text == "true" || identifier.text == "false") {
            return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Bool, identifier.text, absoluteColumn(identifier), identifier.sourceSpan);
        }
        if (identifier.text == "NULL") {
            return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Null, identifier.text, absoluteColumn(identifier), identifier.sourceSpan);
        }
        if (identifier.text == "len") {
            if (!match(TokenKind::LeftParen)) {
                report(identifier, "len must be called as len(list)");
                ok = false;
                return nullptr;
            }
            const Token& leftParen = previous();
            std::vector<std::unique_ptr<Expr>> arguments;
            arguments.push_back(parseExpression(ok));
            if (!ok) return nullptr;
            if (!match(TokenKind::RightParen)) {
                report(leftParen, "unclosed parenthesis in len");
                ok = false;
                return nullptr;
            }
            return std::make_unique<CallExpr>("len", nullptr, std::move(arguments), absoluteColumn(identifier), identifier.sourceSpan);
        }
        if (!syntaxOnly && identifier.text == "split") {
            report(identifier, "split must be called as list.split(delimiter)");
            ok = false;
            return nullptr;
        }
        if (!syntaxOnly && identifier.text == "copy" &&
            check(TokenKind::LeftParen) &&
            current + 2 < tokens.size() &&
            tokens[current + 1].kind == TokenKind::LeftBracket &&
            tokens[current + 2].kind == TokenKind::RightBracket) {
            report(tokens[current + 1], "copy() cannot infer the type of an empty list; declare the list type first");
            ok = false;
            return nullptr;
        }
        if (identifier.text == "min" || identifier.text == "max" || identifier.text == "sum" || identifier.text == "abs" || identifier.text == "range") {
            if (!match(TokenKind::LeftParen)) {
                if (!syntaxOnly && identifier.text == "range") {
                    report(identifier, "range must be called as range(stop), range(start, stop), or range(start, stop, step)");
                    ok = false;
                    return nullptr;
                }
                return std::make_unique<VariableExpr>(identifier.text, absoluteColumn(identifier), identifier.sourceSpan);
            }
            const Token& leftParen = previous();
            std::vector<std::unique_ptr<Expr>> arguments;
            if (!match(TokenKind::RightParen)) {
                arguments.push_back(parseExpression(ok));
                if (!ok) return nullptr;
                while (match(TokenKind::Comma)) {
                    arguments.push_back(parseExpression(ok));
                    if (!ok) return nullptr;
                }
                if (!match(TokenKind::RightParen)) {
                    report(leftParen, "unclosed parenthesis in " + identifier.text);
                    ok = false;
                    return nullptr;
                }
            }
            return std::make_unique<CallExpr>(identifier.text, nullptr, std::move(arguments), absoluteColumn(identifier), identifier.sourceSpan);
        }
        if (!syntaxOnly && identifier.text == "input") {
            reportInputUsageError(identifier);
            ok = false;
            return nullptr;
        }
        if (match(TokenKind::LeftParen)) {
            const Token& leftParen = previous();
            std::vector<std::unique_ptr<Expr>> arguments;
            std::vector<std::string> argumentNames;
            const auto parseArgument = [&]() {
                std::string name;
                if (syntaxOnly && check(TokenKind::Identifier) && current + 1 < tokens.size() &&
                    tokens[current + 1].kind == TokenKind::Equals) {
                    name = tokens[current].text;
                    current += 2;
                }
                arguments.push_back(parseExpression(ok));
                argumentNames.push_back(std::move(name));
            };
            if (!match(TokenKind::RightParen)) {
                parseArgument();
                if (!ok) return nullptr;
                while (match(TokenKind::Comma)) {
                    parseArgument();
                    if (!ok) return nullptr;
                }
                if (!match(TokenKind::RightParen)) {
                    if (atEnd()) {
                        report(leftParen, "unclosed parenthesis in function call");
                    } else {
                        report(peek(), identifier.text == "print"
                            ? "expected ',' between print arguments"
                            : "expected ',' or ')' after function argument");
                    }
                    ok = false;
                    return nullptr;
                }
            }
            return std::make_unique<CallExpr>(
                identifier.text,
                nullptr,
                std::move(arguments),
                absoluteColumn(identifier),
                identifier.sourceSpan,
                std::move(argumentNames)
            );
        }
        return std::make_unique<VariableExpr>(identifier.text, absoluteColumn(identifier), identifier.sourceSpan);
    }

    if (match(TokenKind::Integer)) {
        const Token& literal = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Int, literal.text, absoluteColumn(literal), literal.sourceSpan);
    }
    if (match(TokenKind::Float)) {
        const Token& literal = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Float, literal.text, absoluteColumn(literal), literal.sourceSpan);
    }
    if (match(TokenKind::String)) {
        const Token& literal = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::String, literal.text, absoluteColumn(literal), literal.sourceSpan);
    }
    if (match(TokenKind::Char)) {
        const Token& literal = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Char, literal.text, absoluteColumn(literal), literal.sourceSpan);
    }
    if (match(TokenKind::LeftBracket)) {
        const Token& leftBracket = previous();
        if (match(TokenKind::RightBracket)) {
            if (syntaxOnly) {
                return std::make_unique<ListLiteralExpr>(
                    std::vector<std::unique_ptr<Expr>>{},
                    absoluteColumn(leftBracket),
                    leftBracket.sourceSpan
                );
            }
            report(leftBracket, "empty list literal needs a declared List type");
            ok = false;
            return nullptr;
        }
        std::vector<std::unique_ptr<Expr>> elements;
        elements.push_back(parseExpression(ok));
        if (!ok) return nullptr;
        while (match(TokenKind::Comma)) {
            const Token& comma = previous();
            if (check(TokenKind::RightBracket) || atEnd()) {
                report(comma, "expected expression after ',' in list literal");
                ok = false;
                return nullptr;
            }
            elements.push_back(parseExpression(ok));
            if (!ok) return nullptr;
        }
        if (!match(TokenKind::RightBracket)) {
            report(leftBracket, "unclosed bracket in list literal");
            ok = false;
            return nullptr;
        }
        return std::make_unique<ListLiteralExpr>(std::move(elements), absoluteColumn(leftBracket), leftBracket.sourceSpan);
    }
    if (check(TokenKind::LeftBrace, "{")) {
        return parseBraceLiteral(ok);
    }
    if (match(TokenKind::LeftParen)) {
        const Token& leftParen = previous();
        std::unique_ptr<Expr> expression = parseExpression(ok);
        if (!ok) return nullptr;
        if (!match(TokenKind::RightParen)) {
            report(leftParen, "unclosed parenthesis in expression");
            ok = false;
            return nullptr;
        }
        return expression;
    }

    report(peek(), "expected expression");
    ok = false;
    return nullptr;
}
