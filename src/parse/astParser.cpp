/*
 * astParser.cpp
 *
 * Builds a recursive, syntax-only full-program AST from canonical tokens.
 * The parser deliberately does not consult CompileContext or symbol tables.
 */

#include "astParser.h"

#include "controlFlow.h"
#include "expressions.h"
#include "sourceSplitter.h"
#include "statementParser.h"

#include <algorithm>
#include <set>
#include <utility>

namespace {
SourceSpan mergeSpans(SourceSpan left, SourceSpan right) {
    if (!left.valid()) return right;
    if (!right.valid()) return left;
    if (left.source != right.source) return left;
    left.startOffset = std::min(left.startOffset, right.startOffset);
    left.endOffset = std::max(left.endOffset, right.endOffset);
    return left;
}

SourceSpan tokensSpan(const std::vector<Token>& tokens, size_t begin, size_t end) {
    SourceSpan span;
    end = std::min(end, tokens.size());
    for (size_t index = begin; index < end; ++index) {
        if (tokens[index].kind == TokenKind::EndOfFile || !tokens[index].sourceSpan.valid()) continue;
        span = mergeSpans(span, tokens[index].sourceSpan);
    }
    return span;
}

size_t codeTokenCount(const SourceFragment& fragment) {
    size_t count = fragment.tokens.size();
    while (count > 0 && fragment.tokens[count - 1].kind == TokenKind::EndOfFile) --count;
    return count;
}

SyntaxSite syntaxSite(const SourceFragment& fragment) {
    SyntaxSite site;
    site.sourceSpan = fragment.sourceSpan;
    site.lineNumber = fragment.lineNumber;
    site.startColumn = fragment.startColumn;
    site.endLineNumber = fragment.endLineNumber;
    site.endColumn = fragment.endColumn;
    site.commentText = fragment.commentText;
    return site;
}

StatementSyntax statementSyntax(const SourceFragment& fragment) {
    StatementSyntax syntax;
    static_cast<SyntaxSite&>(syntax) = syntaxSite(fragment);
    syntax.tokens = fragment.tokens;
    const size_t count = codeTokenCount(fragment);
    syntax.terminated = count > 0 && fragment.tokens[count - 1].kind == TokenKind::Semicolon;
    syntax.opensBlock = count > 0 && fragment.tokens[count - 1].kind == TokenKind::LeftBrace;
    syntax.codeLength = fragment.tokens.empty() ? 0 : fragment.tokens.back().span.startOffset;
    return syntax;
}

std::vector<Token> tokenSlice(const std::vector<Token>& tokens, size_t begin, size_t end) {
    std::vector<Token> result;
    end = std::min(end, tokens.size());
    for (size_t index = begin; index < end; ++index) {
        if (tokens[index].kind == TokenKind::EndOfFile) break;
        result.push_back(tokens[index]);
    }
    return result;
}

std::vector<Token> rebasedTokenSlice(const std::vector<Token>& tokens, size_t begin, size_t end) {
    std::vector<Token> result = tokenSlice(tokens, begin, end);
    if (result.empty()) return result;
    const int startColumn = result.front().span.startColumn;
    const size_t startOffset = result.front().span.startOffset;
    for (Token& token : result) {
        token.span.startColumn -= startColumn - 1;
        token.span.endColumn -= startColumn - 1;
        token.span.startOffset -= startOffset;
        token.span.endOffset -= startOffset;
    }
    return result;
}

std::string tokensSpelling(const std::vector<Token>& tokens, size_t begin, size_t end) {
    std::string result;
    end = std::min(end, tokens.size());
    for (size_t index = begin; index < end; ++index) {
        const Token& token = tokens[index];
        if (token.kind == TokenKind::EndOfFile) break;
        if (!result.empty() && token.kind == TokenKind::Identifier &&
            tokens[index - 1].kind == TokenKind::Identifier) {
            result += ' ';
        }
        result += token.text;
    }
    return result;
}

bool tokenIs(const Token& token, TokenKind kind, const std::string& text = "") {
    return token.kind == kind && (text.empty() || token.text == text);
}

std::vector<std::pair<size_t, size_t>> splitTopLevel(
    const std::vector<Token>& tokens,
    size_t begin,
    size_t end,
    TokenKind separatorKind,
    const std::string& separatorText = ""
) {
    std::vector<std::pair<size_t, size_t>> parts;
    int paren = 0;
    int bracket = 0;
    int brace = 0;
    size_t start = begin;
    end = std::min(end, tokens.size());
    for (size_t index = begin; index < end; ++index) {
        const Token& token = tokens[index];
        if (token.kind == TokenKind::LeftParen) ++paren;
        else if (token.kind == TokenKind::RightParen && paren > 0) --paren;
        else if (token.kind == TokenKind::LeftBracket) ++bracket;
        else if (token.kind == TokenKind::RightBracket && bracket > 0) --bracket;
        else if (token.kind == TokenKind::LeftBrace) ++brace;
        else if (token.kind == TokenKind::RightBrace && brace > 0) --brace;
        if (paren == 0 && bracket == 0 && brace == 0 &&
            token.kind == separatorKind && (separatorText.empty() || token.text == separatorText)) {
            parts.push_back({start, index});
            start = index + 1;
        }
    }
    parts.push_back({start, end});
    return parts;
}

std::vector<std::pair<size_t, size_t>> splitTypeComponents(
    const std::vector<Token>& tokens,
    size_t begin,
    size_t end
) {
    std::vector<std::pair<size_t, size_t>> parts;
    int paren = 0;
    int bracket = 0;
    int angle = 0;
    size_t start = begin;
    end = std::min(end, tokens.size());
    for (size_t index = begin; index < end; ++index) {
        const Token& token = tokens[index];
        if (token.kind == TokenKind::LeftParen) ++paren;
        else if (token.kind == TokenKind::RightParen && paren > 0) --paren;
        else if (token.kind == TokenKind::LeftBracket) ++bracket;
        else if (token.kind == TokenKind::RightBracket && bracket > 0) --bracket;
        else if (token.kind == TokenKind::Operator && token.text == "<") ++angle;
        else if (token.kind == TokenKind::Operator && token.text == ">" && angle > 0) --angle;
        else if (token.kind == TokenKind::Operator && token.text == ">>" && angle > 0) angle = std::max(0, angle - 2);
        if (paren == 0 && bracket == 0 && angle == 0 && token.kind == TokenKind::Comma) {
            parts.push_back({start, index});
            start = index + 1;
        }
    }
    parts.push_back({start, end});
    return parts;
}

size_t findMatchingParen(const std::vector<Token>& tokens, size_t left, size_t end) {
    int depth = 0;
    for (size_t index = left; index < end; ++index) {
        if (tokens[index].kind == TokenKind::LeftParen) ++depth;
        else if (tokens[index].kind == TokenKind::RightParen && --depth == 0) return index;
    }
    return end;
}

bool parseTypeSyntax(
    const std::vector<Token>& tokens,
    size_t begin,
    size_t end,
    TypeSyntax& type,
    size_t& next,
    bool validateClosure = true
) {
    if (begin >= end || tokens[begin].kind != TokenKind::Identifier) return false;
    type.name = tokens[begin].text;
    type.nameSpan = tokens[begin].sourceSpan;
    next = begin + 1;

    if (next < end && tokenIs(tokens[next], TokenKind::Operator, "<")) {
        const size_t open = next;
        const size_t argumentsBegin = next + 1;
        int depth = 1;
        size_t close = end;
        for (++next; next < end; ++next) {
            const Token& token = tokens[next];
            if (token.kind != TokenKind::Operator) continue;
            if (token.text == "<") ++depth;
            else if (token.text == ">") --depth;
            else if (token.text == ">>") depth -= 2;
            if (depth <= 0) {
                close = next;
                ++next;
                break;
            }
        }
        if (close == end) {
            next = end;
            if (validateClosure) {
                type.syntaxOk = false;
                type.syntaxError = "unclosed generic type for " + type.name;
                type.errorSpan = type.nameSpan;
            }
        }
        const std::vector<std::pair<size_t, size_t>> parts =
            splitTypeComponents(tokens, argumentsBegin, close);
        for (const auto& part : parts) {
            TypeSyntax argument;
            size_t ignored = part.first;
            if (parseTypeSyntax(tokens, part.first, part.second, argument, ignored, false)) {
                argument.spelling = tokensSpelling(tokens, part.first, part.second);
                argument.sourceSpan = tokensSpan(tokens, part.first, part.second);
                type.arguments.push_back(std::move(argument));
            }
        }

        const Type base = declaredTypeForName(type.name);
        if (type.syntaxOk && base != PrimitiveType::Unknown) {
            const int expected = type.name == "string" || isStructType(base)
                ? 0
                : primitiveArity(base.primitive);
            size_t supplied = 0;
            for (const auto& part : parts) supplied += part.first < part.second ? 1 : 0;
            if (expected == 0) {
                type.syntaxOk = false;
                type.syntaxError = type.name + " expects 0 subtypes";
                type.errorSpan = tokens[open].sourceSpan;
            } else if (supplied < static_cast<size_t>(expected)) {
                type.syntaxOk = false;
                type.syntaxError = type.name + " expects " + std::to_string(expected) +
                    " subtype" + (expected == 1 ? "" : "s") + " like " + type.name +
                    (expected == 1 ? "<int>" : "<int, int>");
                type.errorSpan = type.nameSpan;
            } else if (supplied > static_cast<size_t>(expected)) {
                type.syntaxOk = false;
                type.syntaxError = type.name + " expects " + std::to_string(expected) +
                    " subtype" + (expected == 1 ? "" : "s");
                const size_t extra = parts[static_cast<size_t>(expected)].first;
                type.errorSpan = extra > 0 && tokens[extra - 1].kind == TokenKind::Comma
                    ? tokens[extra - 1].sourceSpan
                    : (extra < end ? tokens[extra].sourceSpan : type.nameSpan);
            }
        }
    }

    if (next < end && tokens[next].kind == TokenKind::LeftParen) {
        const size_t close = findMatchingParen(tokens, next, end);
        if (close < end && close + 1 < end && tokens[close + 1].kind == TokenKind::Identifier) {
            type.functionType = true;
            for (const auto& part : splitTypeComponents(tokens, next + 1, close)) {
                if (part.first == part.second) continue;
                TypeSyntax parameter;
                size_t parameterStart = part.first;
                bool copyParameter = false;
                if (tokens[parameterStart].kind == TokenKind::Identifier &&
                    (tokens[parameterStart].text == "copy" || tokens[parameterStart].text == "deep")) {
                    copyParameter = true;
                    ++parameterStart;
                }
                size_t ignored = parameterStart;
                if (parseTypeSyntax(tokens, parameterStart, part.second, parameter, ignored)) {
                    parameter.spelling = tokensSpelling(tokens, parameterStart, part.second);
                    parameter.sourceSpan = tokensSpan(tokens, parameterStart, part.second);
                    type.functionParameters.push_back(std::move(parameter));
                    type.functionParameterCopy.push_back(copyParameter);
                }
            }
            next = close + 1;
        }
    }

    type.spelling = tokensSpelling(tokens, begin, next);
    type.sourceSpan = tokensSpan(tokens, begin, next);
    return true;
}

SourceSpan normalizeExpressionSpan(Expr& expression) {
    SourceSpan span = expression.sourceSpan;
    const auto include = [&](const std::unique_ptr<Expr>& child, SourceSpan current) {
        return child ? mergeSpans(current, normalizeExpressionSpan(*child)) : current;
    };
    if (auto* node = dynamic_cast<FieldExpr*>(&expression)) {
        span = include(node->base, span);
    } else if (auto* node = dynamic_cast<UnaryExpr*>(&expression)) {
        span = include(node->operand, span);
    } else if (auto* node = dynamic_cast<BinaryExpr*>(&expression)) {
        span = include(node->left, span);
        span = include(node->right, span);
    } else if (auto* node = dynamic_cast<CastExpr*>(&expression)) {
        span = include(node->operand, span);
    } else if (auto* node = dynamic_cast<CallExpr*>(&expression)) {
        span = include(node->receiver, span);
        for (const auto& argument : node->arguments) span = include(argument, span);
    } else if (auto* node = dynamic_cast<IndexExpr*>(&expression)) {
        span = include(node->base, span);
        span = include(node->index, span);
    } else if (auto* node = dynamic_cast<SliceExpr*>(&expression)) {
        span = include(node->base, span);
        span = include(node->start, span);
        span = include(node->end, span);
    } else if (auto* node = dynamic_cast<ListLiteralExpr*>(&expression)) {
        for (const auto& element : node->elements) span = include(element, span);
    } else if (auto* node = dynamic_cast<SetLiteralExpr*>(&expression)) {
        for (const auto& element : node->elements) span = include(element, span);
    } else if (auto* node = dynamic_cast<MapLiteralExpr*>(&expression)) {
        for (const MapLiteralEntry& entry : node->entries) {
            span = include(entry.key, span);
            span = include(entry.value, span);
        }
    } else if (auto* node = dynamic_cast<PairLiteralExpr*>(&expression)) {
        span = include(node->first, span);
        span = include(node->second, span);
    }
    expression.sourceSpan = span;
    return span;
}

std::unique_ptr<Expr> parseExpressionSlice(
    const std::vector<Token>& tokens,
    size_t begin,
    size_t end
) {
    std::vector<Token> slice = tokenSlice(tokens, begin, end);
    std::unique_ptr<Expr> expression = parseSyntaxExpressionAst(slice);
    if (expression) {
        normalizeExpressionSpan(*expression);
        const SourceSpan fullSpan = tokensSpan(tokens, begin, end);
        if (fullSpan.valid()) expression->sourceSpan = fullSpan;
    }
    return expression;
}

bool topLevelAssignment(
    const std::vector<Token>& tokens,
    size_t begin,
    size_t end,
    size_t& operation
) {
    int paren = 0;
    int bracket = 0;
    int brace = 0;
    for (size_t index = begin; index < end; ++index) {
        const Token& token = tokens[index];
        if (token.kind == TokenKind::LeftParen) ++paren;
        else if (token.kind == TokenKind::RightParen && paren > 0) --paren;
        else if (token.kind == TokenKind::LeftBracket) ++bracket;
        else if (token.kind == TokenKind::RightBracket && bracket > 0) --bracket;
        else if (token.kind == TokenKind::LeftBrace) ++brace;
        else if (token.kind == TokenKind::RightBrace && brace > 0) --brace;
        if (paren != 0 || bracket != 0 || brace != 0) continue;
        if (token.kind == TokenKind::Equals ||
            (token.kind == TokenKind::Operator &&
             (token.text == "+=" || token.text == "-=" || token.text == "*=" ||
              token.text == "/=" || token.text == "%=" || token.text == "<<=" ||
              token.text == ">>=" || token.text == "&=" || token.text == "|=" ||
              token.text == "^=" || token.text == "&&=" || token.text == "||="))) {
            operation = index;
            return true;
        }
    }
    return false;
}

void parseExpressionsByComma(
    const std::vector<Token>& tokens,
    size_t begin,
    size_t end,
    std::vector<std::unique_ptr<Expr>>& output
) {
    if (begin >= end) return;
    for (const auto& part : splitTopLevel(tokens, begin, end, TokenKind::Comma)) {
        if (part.first < part.second) output.push_back(parseExpressionSlice(tokens, part.first, part.second));
    }
}

void parseExpressionParts(
    const std::vector<Token>& tokens,
    size_t begin,
    size_t end,
    std::vector<std::unique_ptr<Expr>>& expressions,
    std::vector<std::vector<Token>>& tokenParts,
    std::vector<size_t>& offsets
) {
    if (begin >= end) return;
    for (const auto& part : splitTopLevel(tokens, begin, end, TokenKind::Comma)) {
        if (part.first >= part.second) continue;
        expressions.push_back(parseExpressionSlice(tokens, part.first, part.second));
        tokenParts.push_back(rebasedTokenSlice(tokens, part.first, part.second));
        offsets.push_back(static_cast<size_t>(std::max(0, tokens[part.first].span.startColumn - 1)));
    }
}

bool parseDeclarationShape(
    const std::vector<Token>& tokens,
    size_t begin,
    size_t end,
    TypeSyntax& type,
    size_t& nameIndex
) {
    size_t next = begin;
    if (!parseTypeSyntax(tokens, begin, end, type, next)) return false;
    if (next >= end || tokens[next].kind != TokenKind::Identifier) return false;
    nameIndex = next;
    return true;
}

void fillVariableDeclaration(
    VariableDeclarationAst& declaration,
    const std::vector<Token>& tokens,
    size_t begin,
    size_t end
) {
    size_t firstName = begin;
    parseTypeSyntax(tokens, begin, end, declaration.type, firstName);
    declaration.inferredType = declaration.type.name == "var";
    if (firstName >= end || tokens[firstName].kind != TokenKind::Identifier) {
        if (!declaration.type.syntaxOk) {
            declaration.continuationTokenIndex = end;
            return;
        }
        declaration.syntaxOk = false;
        declaration.syntaxError = "expected variable name after " + declaration.type.name;
        declaration.syntaxErrorOffset = firstName < end
            ? static_cast<size_t>(std::max(0, tokens[firstName].span.startColumn - 1))
            : static_cast<size_t>(std::max(0, tokens[begin].span.endColumn));
        declaration.continuationTokenIndex = end;
        return;
    }

    size_t operation = end;
    const bool hasAssignment = topLevelAssignment(tokens, firstName, end, operation);
    const size_t namesEnd = hasAssignment ? operation : end;
    declaration.names.push_back(tokens[firstName].text);
    declaration.nameSpans.push_back(tokens[firstName].sourceSpan);
    size_t cursor = firstName + 1;
    while (cursor < namesEnd && tokens[cursor].kind == TokenKind::Comma) {
        if (cursor + 1 >= namesEnd || tokens[cursor + 1].kind != TokenKind::Identifier) {
            declaration.syntaxOk = false;
            declaration.syntaxError = "expected variable name after ','";
            declaration.syntaxErrorOffset = static_cast<size_t>(std::max(0, tokens[cursor].span.endColumn));
            declaration.continuationTokenIndex = cursor;
            return;
        }
        declaration.names.push_back(tokens[cursor + 1].text);
        declaration.nameSpans.push_back(tokens[cursor + 1].sourceSpan);
        cursor += 2;
    }
    declaration.continuationTokenIndex = declaration.inferredType ? firstName + 1 : cursor;
    if (hasAssignment) {
        declaration.initializerKind = VariableDeclarationAst::InitializerKind::Assignment;
        parseExpressionsByComma(tokens, operation + 1, end, declaration.initializers);
    } else if (firstName + 1 < end && tokens[firstName + 1].kind == TokenKind::LeftParen) {
        declaration.initializerKind = VariableDeclarationAst::InitializerKind::Parenthesized;
        const size_t close = findMatchingParen(tokens, firstName + 1, end);
        if (close <= end) parseExpressionsByComma(tokens, firstName + 2, close, declaration.initializers);
    }
}

ForClauseAst parseForClause(const std::vector<Token>& tokens) {
    ForClauseAst clause;
    clause.tokens = tokens;
    if (tokens.empty()) return clause;
    clause.sourceSpan = tokensSpan(tokens, 0, tokens.size());
    size_t nameIndex = 0;
    if (parseDeclarationShape(tokens, 0, tokens.size(), clause.type, nameIndex)) {
        clause.kind = ForClauseKind::VariableDeclaration;
        clause.inferredType = clause.type.name == "var";
        size_t operation = tokens.size();
        const bool hasAssignment = topLevelAssignment(tokens, nameIndex, tokens.size(), operation);
        const size_t namesEnd = hasAssignment ? operation : tokens.size();
        for (size_t index = nameIndex; index < namesEnd; ++index) {
            if (tokens[index].kind == TokenKind::Identifier &&
                (index == nameIndex || tokens[index - 1].kind == TokenKind::Comma)) {
                clause.names.push_back(tokens[index].text);
                clause.nameSpans.push_back(tokens[index].sourceSpan);
            }
        }
        clause.continuationTokenIndex = hasAssignment ? operation : tokens.size();
        if (hasAssignment) {
            parseExpressionsByComma(tokens, operation + 1, tokens.size(), clause.expressions);
        }
        return clause;
    }
    size_t operation = tokens.size();
    if (topLevelAssignment(tokens, 0, tokens.size(), operation)) {
        clause.kind = ForClauseKind::Assignment;
        clause.operation = tokens[operation].text;
        clause.operationToken = tokens[operation];
        parseExpressionParts(tokens, 0, operation, clause.expressions,
            clause.targetTokens, clause.targetOffsets);
        parseExpressionParts(tokens, operation + 1, tokens.size(), clause.expressions,
            clause.valueTokens, clause.valueOffsets);
        return clause;
    }
    clause.kind = ForClauseKind::Expression;
    clause.expressions.push_back(parseExpressionSlice(tokens, 0, tokens.size()));
    return clause;
}

class Parser {
public:
    explicit Parser(const TokenStream& stream) : fragments(splitTokenStream(stream)) {}

    ProgramAst parse(SourceSpan sourceSpan) {
        ProgramAst program;
        program.sourceSpan = sourceSpan;
        program.body = parseBlock(false);
        program.body.sourceSpan = sourceSpan;
        return program;
    }

private:
    std::vector<SourceFragment> fragments;
    size_t current = 0;

    StatementParseResult parsedKind(const SourceFragment& fragment) const {
        return parseStatementAst(fragment.tokens, fragment.startColumn);
    }

    bool nextIs(StatementParseResult::Kind kind) const {
        return current < fragments.size() && parsedKind(fragments[current]).kind == kind;
    }

    BlockAst parseBlock(bool expectsClose) {
        BlockAst block;
        while (current < fragments.size()) {
            StatementParseResult parsed = parsedKind(fragments[current]);
            if (parsed.kind == StatementParseResult::Kind::CloseBrace) {
                if (expectsClose) {
                    block.closingSyntax = syntaxSite(fragments[current++]);
                    block.hasClosingSyntax = true;
                    block.sourceSpan = mergeSpans(block.sourceSpan, block.closingSyntax.sourceSpan);
                    break;
                }
                SourceFragment unmatched = fragments[current++];
                auto error = std::make_unique<ErrorStatementAst>(
                    statementSyntax(unmatched),
                    "unmatched closing brace"
                );
                block.sourceSpan = mergeSpans(block.sourceSpan, error->sourceSpan);
                block.statements.push_back(std::move(error));
                continue;
            }
            std::unique_ptr<ProgramStatement> statement = parseStatement();
            if (!statement) {
                SourceFragment recovery = fragments[current++];
                statement = std::make_unique<ErrorStatementAst>(statementSyntax(recovery), "parser made recovery progress");
            }
            block.sourceSpan = mergeSpans(block.sourceSpan, statement->sourceSpan);
            block.statements.push_back(std::move(statement));
        }
        return block;
    }

    void attachCompletion(std::unique_ptr<CompletionBranchAst>& branch) {
        if (!nextIs(StatementParseResult::Kind::Nobreak) && !nextIs(StatementParseResult::Kind::Else)) return;
        branch = std::make_unique<CompletionBranchAst>();
        branch->headerSyntax = syntaxSite(fragments[current++]);
        branch->body = parseBlock(true);
        branch->sourceSpan = mergeSpans(branch->headerSyntax.sourceSpan, branch->body.sourceSpan);
    }

    std::unique_ptr<ProgramStatement> parseStatement() {
        SourceFragment fragment = fragments[current++];
        const size_t count = codeTokenCount(fragment);
        if (count == 0) return std::make_unique<CommentStatementAst>(statementSyntax(fragment));
        const std::vector<Token>& tokens = fragment.tokens;
        StatementParseResult parsed = parseStatementAst(tokens, fragment.startColumn);

        if (count >= 3 && tokens[0].kind == TokenKind::Identifier &&
            (tokens[0].text == "struct" || tokens[0].text == "class") &&
            tokens[count - 1].kind == TokenKind::LeftBrace) {
            auto aggregate = std::make_unique<AggregateDeclarationAst>(statementSyntax(fragment));
            aggregate->isClass = tokens[0].text == "class";
            if (tokens[1].kind == TokenKind::Identifier) {
                aggregate->name = tokens[1].text;
                aggregate->nameSpan = tokens[1].sourceSpan;
            }
            aggregate->body = parseBlock(true);
            aggregate->sourceSpan = mergeSpans(aggregate->sourceSpan, aggregate->body.sourceSpan);
            return aggregate;
        }

        TypeSyntax returnType;
        size_t functionName = 0;
        if (parseDeclarationShape(tokens, 0, count, returnType, functionName) &&
            functionName + 1 < count && tokens[functionName + 1].kind == TokenKind::LeftParen) {
            const size_t close = findMatchingParen(tokens, functionName + 1, count);
            bool hasOpeningBrace = false;
            for (size_t index = functionName + 2; index < count; ++index) {
                if (tokens[index].kind == TokenKind::LeftBrace) {
                    hasOpeningBrace = true;
                    break;
                }
            }
            if (hasOpeningBrace &&
                (close == count || (close + 1 < count && tokens[close + 1].kind == TokenKind::LeftBrace))) {
                auto function = std::make_unique<FunctionDeclarationAst>(statementSyntax(fragment));
                function->returnType = std::move(returnType);
                function->name = tokens[functionName].text;
                function->nameSpan = tokens[functionName].sourceSpan;
                if (close == count) {
                    function->syntaxOk = false;
                    function->syntaxError = "unclosed parenthesis in function declaration";
                    function->syntaxErrorOffset = static_cast<size_t>(
                        std::max(0, tokens[functionName].span.startColumn - 1));
                } else if (close > functionName + 2 && tokens[close - 1].kind == TokenKind::Comma) {
                    function->syntaxOk = false;
                    function->syntaxError = "expected parameter after ','";
                    function->syntaxErrorOffset = static_cast<size_t>(
                        std::max(0, tokens[close - 1].span.startColumn - 1));
                }
                for (const auto& part : splitTypeComponents(tokens, functionName + 2, close)) {
                    if (part.first >= part.second) continue;
                    ParameterSyntax parameter;
                    size_t parameterStart = part.first;
                    if (tokens[parameterStart].kind == TokenKind::Identifier &&
                        (tokens[parameterStart].text == "copy" || tokens[parameterStart].text == "deep")) {
                        parameter.copyParameter = true;
                        parameter.modifier = tokens[parameterStart].text;
                        parameter.modifierSpan = tokens[parameterStart].sourceSpan;
                        ++parameterStart;
                    }
                    size_t parameterName = parameterStart;
                    if (parseDeclarationShape(tokens, parameterStart, part.second, parameter.type, parameterName)) {
                        parameter.name = tokens[parameterName].text;
                        parameter.sourceSpan = tokensSpan(tokens, part.first, part.second);
                    }
                    function->parameters.push_back(std::move(parameter));
                }
                if (close < count) function->body = parseBlock(true);
                function->sourceSpan = mergeSpans(function->sourceSpan, function->body.sourceSpan);
                return function;
            }
        }

        if (parsed.kind == StatementParseResult::Kind::If) {
            auto result = std::make_unique<IfStatementAst>(statementSyntax(fragment));
            const ConditionHeader& header = static_cast<const IfStmt&>(*parsed.statement).header;
            result->syntaxOk = parsed.ok;
            result->syntaxErrorOffset = parsed.errorOffset;
            result->syntaxError = parsed.message;
            result->conditionTokens = header.conditionTokens;
            result->conditionOffset = header.conditionOffset;
            result->condition = parseExpressionSlice(header.conditionTokens, 0, header.conditionTokens.size());
            result->thenBody = parseBlock(true);
            while (nextIs(StatementParseResult::Kind::ElseIf)) {
                ConditionalBranchAst branch;
                const SourceFragment branchFragment = fragments[current++];
                branch.headerSyntax = syntaxSite(branchFragment);
                StatementParseResult branchParsed = parsedKind(branchFragment);
                const ConditionHeader& branchHeader = static_cast<const ElseIfStmt&>(*branchParsed.statement).header;
                branch.syntaxOk = branchParsed.ok;
                branch.syntaxErrorOffset = branchParsed.errorOffset;
                branch.syntaxError = branchParsed.message;
                branch.conditionTokens = branchHeader.conditionTokens;
                branch.conditionOffset = branchHeader.conditionOffset;
                branch.condition = parseExpressionSlice(branchHeader.conditionTokens, 0, branchHeader.conditionTokens.size());
                branch.body = parseBlock(true);
                branch.sourceSpan = mergeSpans(branch.headerSyntax.sourceSpan, branch.body.sourceSpan);
                result->elseIfBranches.push_back(std::move(branch));
            }
            if (nextIs(StatementParseResult::Kind::Else)) {
                result->elseBranch = std::make_unique<CompletionBranchAst>();
                result->elseBranch->headerSyntax = syntaxSite(fragments[current++]);
                result->elseBranch->body = parseBlock(true);
                result->elseBranch->sourceSpan = mergeSpans(
                    result->elseBranch->headerSyntax.sourceSpan,
                    result->elseBranch->body.sourceSpan
                );
            }
            result->sourceSpan = mergeSpans(result->sourceSpan, result->thenBody.sourceSpan);
            for (const auto& branch : result->elseIfBranches) result->sourceSpan = mergeSpans(result->sourceSpan, branch.sourceSpan);
            if (result->elseBranch) result->sourceSpan = mergeSpans(result->sourceSpan, result->elseBranch->sourceSpan);
            return result;
        }

        if (parsed.kind == StatementParseResult::Kind::While) {
            auto result = std::make_unique<WhileStatementAst>(statementSyntax(fragment));
            const ConditionHeader& header = static_cast<const WhileStmt&>(*parsed.statement).header;
            result->syntaxOk = parsed.ok;
            result->syntaxErrorOffset = parsed.errorOffset;
            result->syntaxError = parsed.message;
            result->conditionTokens = header.conditionTokens;
            result->conditionOffset = header.conditionOffset;
            result->condition = parseExpressionSlice(header.conditionTokens, 0, header.conditionTokens.size());
            result->body = parseBlock(true);
            attachCompletion(result->nobreakBranch);
            result->sourceSpan = mergeSpans(result->sourceSpan, result->body.sourceSpan);
            if (result->nobreakBranch) result->sourceSpan = mergeSpans(result->sourceSpan, result->nobreakBranch->sourceSpan);
            return result;
        }

        if (parsed.kind == StatementParseResult::Kind::Rep) {
            auto result = std::make_unique<RepStatementAst>(statementSyntax(fragment));
            const ConditionHeader& header = static_cast<const RepStmt&>(*parsed.statement).header;
            result->syntaxOk = parsed.ok;
            result->syntaxErrorOffset = parsed.errorOffset;
            result->syntaxError = parsed.message;
            result->countTokens = header.conditionTokens;
            result->countOffset = header.conditionOffset;
            result->count = parseExpressionSlice(header.conditionTokens, 0, header.conditionTokens.size());
            result->body = parseBlock(true);
            attachCompletion(result->nobreakBranch);
            result->sourceSpan = mergeSpans(result->sourceSpan, result->body.sourceSpan);
            if (result->nobreakBranch) result->sourceSpan = mergeSpans(result->sourceSpan, result->nobreakBranch->sourceSpan);
            return result;
        }

        if (parsed.kind == StatementParseResult::Kind::ForEach) {
            auto result = std::make_unique<ForEachStatementAst>(statementSyntax(fragment));
            const ForEachHeader& header = static_cast<const ForEachStmt&>(*parsed.statement).header;
            result->syntaxOk = parsed.ok;
            result->syntaxErrorOffset = parsed.errorOffset;
            result->syntaxError = parsed.message;
            result->iterableTokens = header.iterableTokens;
            result->variableOffset = header.variableOffset;
            result->iterableOffset = header.iterableOffset;
            result->variableName = header.variableName;
            result->inferredVariable = header.usesVar;
            if (!header.declarationTokens.empty()) {
                size_t name = 0;
                parseDeclarationShape(header.declarationTokens, 0, header.declarationTokens.size(), result->variableType, name);
            }
            result->iterable = parseExpressionSlice(header.iterableTokens, 0, header.iterableTokens.size());
            result->body = parseBlock(true);
            attachCompletion(result->nobreakBranch);
            result->sourceSpan = mergeSpans(result->sourceSpan, result->body.sourceSpan);
            if (result->nobreakBranch) result->sourceSpan = mergeSpans(result->sourceSpan, result->nobreakBranch->sourceSpan);
            return result;
        }

        if (parsed.kind == StatementParseResult::Kind::For) {
            auto result = std::make_unique<ForStatementAst>(statementSyntax(fragment));
            const ForHeader& header = static_cast<const ForStmt&>(*parsed.statement).header;
            result->syntaxOk = parsed.ok;
            result->syntaxErrorOffset = parsed.errorOffset;
            result->syntaxError = parsed.message;
            result->initializer = parseForClause(header.initializerTokens);
            result->initializer.offset = header.initializerOffset;
            result->conditionTokens = header.conditionTokens;
            result->conditionOffset = header.conditionOffset;
            if (!header.conditionTokens.empty()) {
                result->condition = parseExpressionSlice(header.conditionTokens, 0, header.conditionTokens.size());
            }
            result->iteration = parseForClause(header.iterationTokens);
            result->iteration.offset = header.iterationOffset;
            result->body = parseBlock(true);
            attachCompletion(result->nobreakBranch);
            result->sourceSpan = mergeSpans(result->sourceSpan, result->body.sourceSpan);
            if (result->nobreakBranch) result->sourceSpan = mergeSpans(result->sourceSpan, result->nobreakBranch->sourceSpan);
            return result;
        }

        size_t end = count;
        if (end > 0 && tokens[end - 1].kind == TokenKind::Semicolon) --end;
        if (end > 0 && tokens[0].kind == TokenKind::Identifier && tokens[0].text == "return") {
            auto result = std::make_unique<ReturnStatementAst>(statementSyntax(fragment));
            if (end > 1) {
                result->value = parseExpressionSlice(tokens, 1, end);
                result->valueTokens = rebasedTokenSlice(tokens, 1, end);
                result->valueOffset = static_cast<size_t>(std::max(0, tokens[1].span.startColumn - 1));
            }
            return result;
        }
        if (end == 1 && tokens[0].kind == TokenKind::Identifier && tokens[0].text == "break") {
            return std::make_unique<SimpleControlStatementAst>(ProgramStatementKind::Break, statementSyntax(fragment));
        }
        if (end == 1 && tokens[0].kind == TokenKind::Identifier && tokens[0].text == "continue") {
            return std::make_unique<SimpleControlStatementAst>(ProgramStatementKind::Continue, statementSyntax(fragment));
        }

        TypeSyntax declarationType;
        size_t firstName = 0;
        const bool declarationShape = parseDeclarationShape(tokens, 0, end, declarationType, firstName);
        const bool knownDeclarationRoot = end > 0 && tokens[0].kind == TokenKind::Identifier &&
            (declaredTypeForName(tokens[0].text) != PrimitiveType::Unknown ||
             tokens[0].text == "var" || tokens[0].text == "bigint" || tokens[0].text == "Bigint" ||
             tokens[0].text == "bigfloat" || tokens[0].text == "BigFloat");
        if (declarationShape || knownDeclarationRoot) {
            auto result = std::make_unique<VariableDeclarationAst>(statementSyntax(fragment));
            fillVariableDeclaration(*result, tokens, 0, end);
            return result;
        }

        size_t operation = end;
        if (topLevelAssignment(tokens, 0, end, operation)) {
            auto result = std::make_unique<AssignmentStatementAst>(statementSyntax(fragment));
            result->operation = tokens[operation].text;
            result->operationSpan = tokens[operation].sourceSpan;
            result->operationToken = tokens[operation];
            parseExpressionParts(tokens, 0, operation, result->targets,
                result->targetTokens, result->targetOffsets);
            parseExpressionParts(tokens, operation + 1, end, result->values,
                result->valueTokens, result->valueOffsets);
            return result;
        }

        if (parsed.kind == StatementParseResult::Kind::Else ||
            parsed.kind == StatementParseResult::Kind::ElseIf ||
            parsed.kind == StatementParseResult::Kind::Nobreak ||
            parsed.kind == StatementParseResult::Kind::CloseBrace) {
            auto error = std::make_unique<ErrorStatementAst>(statementSyntax(fragment), "unattached block continuation");
            if (count > 0 && tokens[count - 1].kind == TokenKind::LeftBrace) {
                error->recoveredBody = std::make_unique<BlockAst>(parseBlock(true));
                error->sourceSpan = mergeSpans(error->sourceSpan, error->recoveredBody->sourceSpan);
            }
            return error;
        }

        auto result = std::make_unique<ExpressionStatementAst>(statementSyntax(fragment));
        if (end > 0) result->expression = parseExpressionSlice(tokens, 0, end);
        return result;
    }
};

bool spanWithin(SourceSpan child, SourceSpan parent) {
    return !child.valid() || !parent.valid() ||
        (child.source == parent.source && child.startOffset >= parent.startOffset && child.endOffset <= parent.endOffset);
}

bool validateBlock(const BlockAst& block, SourceSpan parent, std::string& message);

bool validateExpression(const Expr* expression, SourceSpan parent, std::string& message) {
    if (!expression) {
        message = "mandatory expression child is null";
        return false;
    }
    // Recovery expressions may represent wholly missing syntax, so there is no
    // token range to attach. Their containing statement still carries the
    // source span needed by downstream diagnostics.
    if (dynamic_cast<const ErrorExpr*>(expression)) return true;
    if (!expression->sourceSpan.valid() || !spanWithin(expression->sourceSpan, parent)) {
        message = "expression has an invalid or out-of-parent span [" +
            std::to_string(expression->sourceSpan.startOffset) + ".." +
            std::to_string(expression->sourceSpan.endOffset) + "] in [" +
            std::to_string(parent.startOffset) + ".." +
            std::to_string(parent.endOffset) + "]";
        return false;
    }
    const auto child = [&](const std::unique_ptr<Expr>& value) {
        return validateExpression(value.get(), expression->sourceSpan, message);
    };
    if (const auto* node = dynamic_cast<const FieldExpr*>(expression)) return child(node->base);
    if (const auto* node = dynamic_cast<const UnaryExpr*>(expression)) return child(node->operand);
    if (const auto* node = dynamic_cast<const BinaryExpr*>(expression)) return child(node->left) && child(node->right);
    if (const auto* node = dynamic_cast<const CastExpr*>(expression)) return child(node->operand);
    if (const auto* node = dynamic_cast<const CallExpr*>(expression)) {
        if (node->receiver && !child(node->receiver)) return false;
        for (const auto& argument : node->arguments) if (!child(argument)) return false;
    } else if (const auto* node = dynamic_cast<const IndexExpr*>(expression)) {
        return child(node->base) && child(node->index);
    } else if (const auto* node = dynamic_cast<const SliceExpr*>(expression)) {
        return child(node->base) &&
            (!node->start || child(node->start)) &&
            (!node->end || child(node->end));
    } else if (const auto* node = dynamic_cast<const ListLiteralExpr*>(expression)) {
        for (const auto& element : node->elements) if (!child(element)) return false;
    } else if (const auto* node = dynamic_cast<const SetLiteralExpr*>(expression)) {
        for (const auto& element : node->elements) if (!child(element)) return false;
    } else if (const auto* node = dynamic_cast<const MapLiteralExpr*>(expression)) {
        for (const MapLiteralEntry& entry : node->entries) {
            if (!child(entry.key) || !child(entry.value)) return false;
        }
    } else if (const auto* node = dynamic_cast<const PairLiteralExpr*>(expression)) {
        return child(node->first) && child(node->second);
    }
    return true;
}

bool validateCompletion(const CompletionBranchAst* branch, SourceSpan parent, std::string& message) {
    if (!branch) return true;
    if (!spanWithin(branch->sourceSpan, parent)) {
        message = "completion branch span lies outside its parent";
        return false;
    }
    return validateBlock(branch->body, branch->sourceSpan, message);
}

bool validateBlock(const BlockAst& block, SourceSpan parent, std::string& message) {
    if (!spanWithin(block.sourceSpan, parent)) {
        message = "block span lies outside its parent";
        return false;
    }
    if (block.hasClosingSyntax && !spanWithin(block.closingSyntax.sourceSpan, parent)) {
        message = "closing brace span lies outside its block parent";
        return false;
    }
    for (const auto& statement : block.statements) {
        if (!statement) {
            message = "block contains a null statement";
            return false;
        }
        if (!statement->sourceSpan.valid()) {
            message = "statement has no source span";
            return false;
        }
        if (!spanWithin(statement->sourceSpan, parent)) {
            message = "statement span lies outside its parent";
            return false;
        }
        if (const auto* node = dynamic_cast<const ErrorStatementAst*>(statement.get())) {
            if (node->recoveredBody && !validateBlock(*node->recoveredBody, statement->sourceSpan, message)) return false;
        } else if (const auto* node = dynamic_cast<const VariableDeclarationAst*>(statement.get())) {
            for (const auto& expression : node->initializers) {
                if (!validateExpression(expression.get(), statement->sourceSpan, message)) return false;
            }
        } else if (const auto* node = dynamic_cast<const AssignmentStatementAst*>(statement.get())) {
            for (const auto& expression : node->targets) if (!validateExpression(expression.get(), statement->sourceSpan, message)) return false;
            for (const auto& expression : node->values) if (!validateExpression(expression.get(), statement->sourceSpan, message)) return false;
        } else if (const auto* node = dynamic_cast<const ExpressionStatementAst*>(statement.get())) {
            if (!validateExpression(node->expression.get(), statement->sourceSpan, message)) return false;
        } else if (const auto* node = dynamic_cast<const ReturnStatementAst*>(statement.get())) {
            if (node->value && !validateExpression(node->value.get(), statement->sourceSpan, message)) return false;
        } else if (const auto* node = dynamic_cast<const FunctionDeclarationAst*>(statement.get())) {
            if (!validateBlock(node->body, statement->sourceSpan, message)) return false;
        } else if (const auto* node = dynamic_cast<const AggregateDeclarationAst*>(statement.get())) {
            if (!validateBlock(node->body, statement->sourceSpan, message)) return false;
        } else if (const auto* node = dynamic_cast<const WhileStatementAst*>(statement.get())) {
            if (!validateExpression(node->condition.get(), statement->sourceSpan, message) ||
                !validateBlock(node->body, statement->sourceSpan, message) ||
                !validateCompletion(node->nobreakBranch.get(), statement->sourceSpan, message)) return false;
        } else if (const auto* node = dynamic_cast<const ForStatementAst*>(statement.get())) {
            if ((node->condition && !validateExpression(node->condition.get(), statement->sourceSpan, message)) ||
                !validateBlock(node->body, statement->sourceSpan, message) ||
                !validateCompletion(node->nobreakBranch.get(), statement->sourceSpan, message)) return false;
            for (const auto& expression : node->initializer.expressions) if (!validateExpression(expression.get(), statement->sourceSpan, message)) return false;
            for (const auto& expression : node->iteration.expressions) if (!validateExpression(expression.get(), statement->sourceSpan, message)) return false;
        } else if (const auto* node = dynamic_cast<const ForEachStatementAst*>(statement.get())) {
            if (!validateExpression(node->iterable.get(), statement->sourceSpan, message) ||
                !validateBlock(node->body, statement->sourceSpan, message) ||
                !validateCompletion(node->nobreakBranch.get(), statement->sourceSpan, message)) return false;
        } else if (const auto* node = dynamic_cast<const RepStatementAst*>(statement.get())) {
            if (!validateExpression(node->count.get(), statement->sourceSpan, message) ||
                !validateBlock(node->body, statement->sourceSpan, message) ||
                !validateCompletion(node->nobreakBranch.get(), statement->sourceSpan, message)) return false;
        } else if (const auto* node = dynamic_cast<const IfStatementAst*>(statement.get())) {
            if (!validateExpression(node->condition.get(), statement->sourceSpan, message) ||
                !validateBlock(node->thenBody, statement->sourceSpan, message)) return false;
            for (const ConditionalBranchAst& branch : node->elseIfBranches) {
                if (!spanWithin(branch.sourceSpan, statement->sourceSpan) ||
                    !validateExpression(branch.condition.get(), branch.sourceSpan, message) ||
                    !validateBlock(branch.body, branch.sourceSpan, message)) return false;
            }
            if (!validateCompletion(node->elseBranch.get(), statement->sourceSpan, message)) return false;
        }
    }
    return true;
}
}

ProgramAst parseProgramAst(const TokenStream& tokenStream) {
    return Parser(tokenStream).parse(tokenStream.sourceSpan);
}

bool validateProgramAst(const ProgramAst& program, std::string& message) {
    if (!program.sourceSpan.valid()) {
        message = "program has no source span";
        return false;
    }
    return validateBlock(program.body, program.sourceSpan, message);
}
