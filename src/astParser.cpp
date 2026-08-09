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

std::vector<Token> tokenSlice(const std::vector<Token>& tokens, size_t begin, size_t end) {
    std::vector<Token> result;
    end = std::min(end, tokens.size());
    for (size_t index = begin; index < end; ++index) {
        if (tokens[index].kind == TokenKind::EndOfFile) break;
        result.push_back(tokens[index]);
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

bool isOpenDelimiter(const Token& token) {
    return token.kind == TokenKind::LeftParen || token.kind == TokenKind::LeftBracket ||
        token.kind == TokenKind::LeftBrace;
}

bool isCloseDelimiter(const Token& token) {
    return token.kind == TokenKind::RightParen || token.kind == TokenKind::RightBracket ||
        token.kind == TokenKind::RightBrace;
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
    size_t& next
) {
    if (begin >= end || tokens[begin].kind != TokenKind::Identifier) return false;
    type.name = tokens[begin].text;
    next = begin + 1;

    if (next < end && tokenIs(tokens[next], TokenKind::Operator, "<")) {
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
        if (close == end) next = end;
        for (const auto& part : splitTopLevel(tokens, argumentsBegin, close, TokenKind::Comma)) {
            TypeSyntax argument;
            size_t ignored = part.first;
            if (parseTypeSyntax(tokens, part.first, part.second, argument, ignored)) {
                argument.spelling = tokensSpelling(tokens, part.first, part.second);
                argument.sourceSpan = tokensSpan(tokens, part.first, part.second);
                type.arguments.push_back(std::move(argument));
            }
        }
    }

    if (next < end && tokens[next].kind == TokenKind::LeftParen) {
        const size_t close = findMatchingParen(tokens, next, end);
        if (close < end && close + 1 < end && tokens[close + 1].kind == TokenKind::Identifier) {
            type.functionType = true;
            for (const auto& part : splitTopLevel(tokens, next + 1, close, TokenKind::Comma)) {
                if (part.first == part.second) continue;
                TypeSyntax parameter;
                size_t ignored = part.first;
                if (parseTypeSyntax(tokens, part.first, part.second, parameter, ignored)) {
                    parameter.spelling = tokensSpelling(tokens, part.first, part.second);
                    parameter.sourceSpan = tokensSpan(tokens, part.first, part.second);
                    type.functionParameters.push_back(std::move(parameter));
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
    parseDeclarationShape(tokens, begin, end, declaration.type, firstName);
    size_t operation = end;
    const bool hasAssignment = topLevelAssignment(tokens, firstName, end, operation);
    const size_t namesEnd = hasAssignment ? operation : end;
    int depth = 0;
    for (size_t index = firstName; index < namesEnd; ++index) {
        const Token& token = tokens[index];
        if (isOpenDelimiter(token)) ++depth;
        else if (isCloseDelimiter(token) && depth > 0) --depth;
        if (depth == 0 && token.kind == TokenKind::Identifier &&
            (index == firstName || tokens[index - 1].kind == TokenKind::Comma)) {
            declaration.names.push_back(token.text);
            declaration.nameSpans.push_back(token.sourceSpan);
        }
    }
    if (hasAssignment) {
        parseExpressionsByComma(tokens, operation + 1, end, declaration.initializers);
    } else if (firstName + 1 < end && tokens[firstName + 1].kind == TokenKind::LeftParen) {
        const size_t close = findMatchingParen(tokens, firstName + 1, end);
        if (close <= end) parseExpressionsByComma(tokens, firstName + 2, close, declaration.initializers);
    }
}

ForClauseAst parseForClause(const std::vector<Token>& tokens) {
    ForClauseAst clause;
    if (tokens.empty()) return clause;
    clause.sourceSpan = tokensSpan(tokens, 0, tokens.size());
    size_t nameIndex = 0;
    if (parseDeclarationShape(tokens, 0, tokens.size(), clause.type, nameIndex)) {
        clause.kind = ForClauseKind::VariableDeclaration;
        clause.names.push_back(tokens[nameIndex].text);
        size_t operation = tokens.size();
        if (topLevelAssignment(tokens, nameIndex, tokens.size(), operation)) {
            parseExpressionsByComma(tokens, operation + 1, tokens.size(), clause.expressions);
        }
        return clause;
    }
    size_t operation = tokens.size();
    if (topLevelAssignment(tokens, 0, tokens.size(), operation)) {
        clause.kind = ForClauseKind::Assignment;
        clause.operation = tokens[operation].text;
        parseExpressionsByComma(tokens, 0, operation, clause.expressions);
        parseExpressionsByComma(tokens, operation + 1, tokens.size(), clause.expressions);
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
        program.attributedFragmentCount = fragments.size();
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
                    block.closingFragment = fragments[current++];
                    block.hasClosingFragment = true;
                    block.sourceSpan = mergeSpans(block.sourceSpan, block.closingFragment.sourceSpan);
                    break;
                }
                SourceFragment unmatched = fragments[current++];
                auto error = std::make_unique<ErrorStatementAst>(
                    unmatched,
                    "unmatched closing brace"
                );
                block.sourceSpan = mergeSpans(block.sourceSpan, error->sourceSpan);
                block.statements.push_back(std::move(error));
                continue;
            }
            std::unique_ptr<ProgramStatement> statement = parseStatement();
            if (!statement) {
                SourceFragment recovery = fragments[current++];
                statement = std::make_unique<ErrorStatementAst>(std::move(recovery), "parser made recovery progress");
            }
            block.sourceSpan = mergeSpans(block.sourceSpan, statement->sourceSpan);
            block.statements.push_back(std::move(statement));
        }
        return block;
    }

    void attachCompletion(std::unique_ptr<CompletionBranchAst>& branch) {
        if (!nextIs(StatementParseResult::Kind::Nobreak) && !nextIs(StatementParseResult::Kind::Else)) return;
        branch = std::make_unique<CompletionBranchAst>();
        branch->headerFragment = fragments[current++];
        branch->body = parseBlock(true);
        branch->sourceSpan = mergeSpans(branch->headerFragment.sourceSpan, branch->body.sourceSpan);
    }

    std::unique_ptr<ProgramStatement> parseStatement() {
        SourceFragment fragment = fragments[current++];
        const size_t count = codeTokenCount(fragment);
        if (count == 0) return std::make_unique<CommentStatementAst>(fragment);
        const std::vector<Token>& tokens = fragment.tokens;
        StatementParseResult parsed = parseStatementAst(tokens, fragment.startColumn);

        if (count >= 3 && tokens[0].kind == TokenKind::Identifier &&
            (tokens[0].text == "struct" || tokens[0].text == "class") &&
            tokens[count - 1].kind == TokenKind::LeftBrace) {
            auto aggregate = std::make_unique<AggregateDeclarationAst>(fragment);
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
        if (tokens[count - 1].kind == TokenKind::LeftBrace &&
            parseDeclarationShape(tokens, 0, count - 1, returnType, functionName) &&
            functionName + 1 < count && tokens[functionName + 1].kind == TokenKind::LeftParen) {
            const size_t close = findMatchingParen(tokens, functionName + 1, count);
            if (close < count && close + 1 < count && tokens[close + 1].kind == TokenKind::LeftBrace) {
                auto function = std::make_unique<FunctionDeclarationAst>(fragment);
                function->returnType = std::move(returnType);
                function->name = tokens[functionName].text;
                function->nameSpan = tokens[functionName].sourceSpan;
                for (const auto& part : splitTopLevel(tokens, functionName + 2, close, TokenKind::Comma)) {
                    if (part.first >= part.second) continue;
                    ParameterSyntax parameter;
                    size_t parameterStart = part.first;
                    if (tokens[parameterStart].kind == TokenKind::Identifier && tokens[parameterStart].text == "copy") {
                        parameter.copyParameter = true;
                        ++parameterStart;
                    }
                    size_t parameterName = parameterStart;
                    if (parseDeclarationShape(tokens, parameterStart, part.second, parameter.type, parameterName)) {
                        parameter.name = tokens[parameterName].text;
                        parameter.sourceSpan = tokensSpan(tokens, part.first, part.second);
                    }
                    function->parameters.push_back(std::move(parameter));
                }
                function->body = parseBlock(true);
                function->sourceSpan = mergeSpans(function->sourceSpan, function->body.sourceSpan);
                return function;
            }
        }

        if (parsed.kind == StatementParseResult::Kind::If) {
            auto result = std::make_unique<IfStatementAst>(fragment);
            const ConditionHeader& header = static_cast<const IfStmt&>(*parsed.statement).header;
            result->condition = parseExpressionSlice(header.conditionTokens, 0, header.conditionTokens.size());
            result->thenBody = parseBlock(true);
            while (nextIs(StatementParseResult::Kind::ElseIf)) {
                ConditionalBranchAst branch;
                branch.headerFragment = fragments[current++];
                StatementParseResult branchParsed = parsedKind(branch.headerFragment);
                const ConditionHeader& branchHeader = static_cast<const ElseIfStmt&>(*branchParsed.statement).header;
                branch.condition = parseExpressionSlice(branchHeader.conditionTokens, 0, branchHeader.conditionTokens.size());
                branch.body = parseBlock(true);
                branch.sourceSpan = mergeSpans(branch.headerFragment.sourceSpan, branch.body.sourceSpan);
                result->elseIfBranches.push_back(std::move(branch));
            }
            if (nextIs(StatementParseResult::Kind::Else)) {
                result->elseBranch = std::make_unique<CompletionBranchAst>();
                result->elseBranch->headerFragment = fragments[current++];
                result->elseBranch->body = parseBlock(true);
                result->elseBranch->sourceSpan = mergeSpans(
                    result->elseBranch->headerFragment.sourceSpan,
                    result->elseBranch->body.sourceSpan
                );
            }
            result->sourceSpan = mergeSpans(result->sourceSpan, result->thenBody.sourceSpan);
            for (const auto& branch : result->elseIfBranches) result->sourceSpan = mergeSpans(result->sourceSpan, branch.sourceSpan);
            if (result->elseBranch) result->sourceSpan = mergeSpans(result->sourceSpan, result->elseBranch->sourceSpan);
            return result;
        }

        if (parsed.kind == StatementParseResult::Kind::While) {
            auto result = std::make_unique<WhileStatementAst>(fragment);
            const ConditionHeader& header = static_cast<const WhileStmt&>(*parsed.statement).header;
            result->condition = parseExpressionSlice(header.conditionTokens, 0, header.conditionTokens.size());
            result->body = parseBlock(true);
            attachCompletion(result->nobreakBranch);
            result->sourceSpan = mergeSpans(result->sourceSpan, result->body.sourceSpan);
            if (result->nobreakBranch) result->sourceSpan = mergeSpans(result->sourceSpan, result->nobreakBranch->sourceSpan);
            return result;
        }

        if (parsed.kind == StatementParseResult::Kind::Rep) {
            auto result = std::make_unique<RepStatementAst>(fragment);
            const ConditionHeader& header = static_cast<const RepStmt&>(*parsed.statement).header;
            result->count = parseExpressionSlice(header.conditionTokens, 0, header.conditionTokens.size());
            result->body = parseBlock(true);
            attachCompletion(result->nobreakBranch);
            result->sourceSpan = mergeSpans(result->sourceSpan, result->body.sourceSpan);
            if (result->nobreakBranch) result->sourceSpan = mergeSpans(result->sourceSpan, result->nobreakBranch->sourceSpan);
            return result;
        }

        if (parsed.kind == StatementParseResult::Kind::ForEach) {
            auto result = std::make_unique<ForEachStatementAst>(fragment);
            const ForEachHeader& header = static_cast<const ForEachStmt&>(*parsed.statement).header;
            result->variableName = header.variableName;
            result->inferredVariable = header.usesVar;
            if (!header.declarationTokens.empty()) {
                size_t name = 0;
                parseDeclarationShape(header.declarationTokens, 0, header.declarationTokens.size(), result->variableType, name);
                if (name < header.declarationTokens.size()) result->variableSpan = header.declarationTokens[name].sourceSpan;
            }
            result->iterable = parseExpressionSlice(header.iterableTokens, 0, header.iterableTokens.size());
            result->body = parseBlock(true);
            attachCompletion(result->nobreakBranch);
            result->sourceSpan = mergeSpans(result->sourceSpan, result->body.sourceSpan);
            if (result->nobreakBranch) result->sourceSpan = mergeSpans(result->sourceSpan, result->nobreakBranch->sourceSpan);
            return result;
        }

        if (parsed.kind == StatementParseResult::Kind::For) {
            auto result = std::make_unique<ForStatementAst>(fragment);
            const ForHeader& header = static_cast<const ForStmt&>(*parsed.statement).header;
            result->initializer = parseForClause(header.initializerTokens);
            if (!header.conditionTokens.empty()) {
                result->condition = parseExpressionSlice(header.conditionTokens, 0, header.conditionTokens.size());
            }
            result->iteration = parseForClause(header.iterationTokens);
            result->body = parseBlock(true);
            attachCompletion(result->nobreakBranch);
            result->sourceSpan = mergeSpans(result->sourceSpan, result->body.sourceSpan);
            if (result->nobreakBranch) result->sourceSpan = mergeSpans(result->sourceSpan, result->nobreakBranch->sourceSpan);
            return result;
        }

        size_t end = count;
        if (end > 0 && tokens[end - 1].kind == TokenKind::Semicolon) --end;
        if (end > 0 && tokens[0].kind == TokenKind::Identifier && tokens[0].text == "return") {
            auto result = std::make_unique<ReturnStatementAst>(fragment);
            if (end > 1) result->value = parseExpressionSlice(tokens, 1, end);
            return result;
        }
        if (end == 1 && tokens[0].kind == TokenKind::Identifier && tokens[0].text == "break") {
            return std::make_unique<SimpleControlStatementAst>(ProgramStatementKind::Break, fragment);
        }
        if (end == 1 && tokens[0].kind == TokenKind::Identifier && tokens[0].text == "continue") {
            return std::make_unique<SimpleControlStatementAst>(ProgramStatementKind::Continue, fragment);
        }

        TypeSyntax declarationType;
        size_t firstName = 0;
        if (parseDeclarationShape(tokens, 0, end, declarationType, firstName)) {
            auto result = std::make_unique<VariableDeclarationAst>(fragment);
            fillVariableDeclaration(*result, tokens, 0, end);
            return result;
        }

        size_t operation = end;
        if (topLevelAssignment(tokens, 0, end, operation)) {
            auto result = std::make_unique<AssignmentStatementAst>(fragment);
            result->operation = tokens[operation].text;
            result->operationSpan = tokens[operation].sourceSpan;
            parseExpressionsByComma(tokens, 0, operation, result->targets);
            parseExpressionsByComma(tokens, operation + 1, end, result->values);
            return result;
        }

        if (parsed.kind == StatementParseResult::Kind::Else ||
            parsed.kind == StatementParseResult::Kind::ElseIf ||
            parsed.kind == StatementParseResult::Kind::Nobreak ||
            parsed.kind == StatementParseResult::Kind::CloseBrace) {
            auto error = std::make_unique<ErrorStatementAst>(fragment, "unattached block continuation");
            if (count > 0 && tokens[count - 1].kind == TokenKind::LeftBrace) {
                error->recoveredBody = std::make_unique<BlockAst>(parseBlock(true));
                error->sourceSpan = mergeSpans(error->sourceSpan, error->recoveredBody->sourceSpan);
            }
            return error;
        }

        auto result = std::make_unique<ExpressionStatementAst>(fragment);
        if (end > 0) result->expression = parseExpressionSlice(tokens, 0, end);
        return result;
    }
};

void flattenBlock(const BlockAst& block, std::vector<SourceFragment>& output);

void flattenStatement(const ProgramStatement& statement, std::vector<SourceFragment>& output) {
    output.push_back(statement.fragment);
    if (const auto* node = dynamic_cast<const ErrorStatementAst*>(&statement)) {
        if (node->recoveredBody) flattenBlock(*node->recoveredBody, output);
    } else if (const auto* node = dynamic_cast<const IfStatementAst*>(&statement)) {
        flattenBlock(node->thenBody, output);
        for (const ConditionalBranchAst& branch : node->elseIfBranches) {
            output.push_back(branch.headerFragment);
            flattenBlock(branch.body, output);
        }
        if (node->elseBranch) {
            output.push_back(node->elseBranch->headerFragment);
            flattenBlock(node->elseBranch->body, output);
        }
    } else if (const auto* node = dynamic_cast<const WhileStatementAst*>(&statement)) {
        flattenBlock(node->body, output);
        if (node->nobreakBranch) {
            output.push_back(node->nobreakBranch->headerFragment);
            flattenBlock(node->nobreakBranch->body, output);
        }
    } else if (const auto* node = dynamic_cast<const ForStatementAst*>(&statement)) {
        flattenBlock(node->body, output);
        if (node->nobreakBranch) {
            output.push_back(node->nobreakBranch->headerFragment);
            flattenBlock(node->nobreakBranch->body, output);
        }
    } else if (const auto* node = dynamic_cast<const ForEachStatementAst*>(&statement)) {
        flattenBlock(node->body, output);
        if (node->nobreakBranch) {
            output.push_back(node->nobreakBranch->headerFragment);
            flattenBlock(node->nobreakBranch->body, output);
        }
    } else if (const auto* node = dynamic_cast<const RepStatementAst*>(&statement)) {
        flattenBlock(node->body, output);
        if (node->nobreakBranch) {
            output.push_back(node->nobreakBranch->headerFragment);
            flattenBlock(node->nobreakBranch->body, output);
        }
    } else if (const auto* node = dynamic_cast<const FunctionDeclarationAst*>(&statement)) {
        flattenBlock(node->body, output);
    } else if (const auto* node = dynamic_cast<const AggregateDeclarationAst*>(&statement)) {
        flattenBlock(node->body, output);
    }
}

void flattenBlock(const BlockAst& block, std::vector<SourceFragment>& output) {
    for (const auto& statement : block.statements) flattenStatement(*statement, output);
    if (block.hasClosingFragment) output.push_back(block.closingFragment);
}

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
        return child(node->base) && child(node->start) && child(node->end);
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

std::vector<SourceFragment> lowerProgramAstToFragments(const ProgramAst& program) {
    std::vector<SourceFragment> fragments;
    flattenBlock(program.body, fragments);
    return fragments;
}

bool validateProgramAst(const ProgramAst& program, std::string& message) {
    if (!program.sourceSpan.valid()) {
        message = "program has no source span";
        return false;
    }
    if (!validateBlock(program.body, program.sourceSpan, message)) return false;
    if (lowerProgramAstToFragments(program).size() != program.attributedFragmentCount) {
        message = "AST compatibility traversal does not attribute every source fragment";
        return false;
    }
    return true;
}
