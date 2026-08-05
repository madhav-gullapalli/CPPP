/*
 * statementParser.cpp
 *
 * Parses statement-level syntax and constructs the intermediate statement AST.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "statementParser.h"

#include "controlFlow.h"

namespace {
// trim removes surrounding whitespace from a string.
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::string tokenSpelling(const std::vector<Token>& tokens) {
    std::string result;
    const Token* previous = nullptr;
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::EndOfFile || token.kind == TokenKind::LineComment) {
            break;
        }
        if (previous != nullptr) {
            const size_t previousEnd = previous->span.endOffset;
            const size_t currentStart = token.span.startOffset;
            result.append(currentStart > previousEnd ? currentStart - previousEnd : 0, ' ');
        } else if (token.span.startOffset > 0) {
            result.append(token.span.startOffset, ' ');
        }
        result += token.text;
        previous = &token;
    }
    return trim(result);
}
}

// parseStatementAst parses statementsast for the compiler pipeline.
StatementParseResult parseStatementAst(const std::vector<Token>& tokens, int sourceColumn) {
    StatementParseResult result;
    const bool empty = tokens.empty() || tokens[0].kind == TokenKind::EndOfFile;
    if (empty) {
        result.kind = StatementParseResult::Kind::Empty;
        result.statement = std::make_unique<EmptyStmt>();
        result.statement->sourceColumn = sourceColumn;
        return result;
    }

    if (tokens[0].kind == TokenKind::RightBrace) {
        const std::string trimmed = tokenSpelling(tokens);
        result.kind = StatementParseResult::Kind::CloseBrace;
        result.statement = std::make_unique<CloseBraceStmt>(trim(trimmed.substr(1)), sourceColumn);
        return result;
    }

    const ForEachParseResult forEachResult = parseForEachHeader(tokens);
    if (forEachResult.matched) {
        result.kind = StatementParseResult::Kind::ForEach;
        result.ok = forEachResult.ok;
        result.errorOffset = forEachResult.errorOffset;
        result.message = forEachResult.message;
        result.statement = std::make_unique<ForEachStmt>(forEachResult.header, sourceColumn);
        return result;
    }

    const ForParseResult forResult = parseForHeaderDetailed(tokens);
    if (forResult.matched) {
        result.kind = StatementParseResult::Kind::For;
        result.ok = forResult.ok;
        result.errorOffset = forResult.errorOffset;
        result.message = forResult.message;
        result.statement = std::make_unique<ForStmt>(forResult.header, sourceColumn);
        return result;
    }

    const ConditionParseResult repResult = parseConditionHeaderDetailed(tokens, "rep", "rep");
    if (repResult.matched) {
        result.kind = StatementParseResult::Kind::Rep;
        result.ok = repResult.ok;
        result.errorOffset = repResult.errorOffset;
        result.message = repResult.message;
        result.statement = std::make_unique<RepStmt>(repResult.header, sourceColumn);
        return result;
    }

    const ConditionParseResult ifResult = parseConditionHeaderDetailed(tokens, "if", "if");
    if (ifResult.matched) {
        result.kind = StatementParseResult::Kind::If;
        result.ok = ifResult.ok;
        result.errorOffset = ifResult.errorOffset;
        result.message = ifResult.message;
        result.statement = std::make_unique<IfStmt>(ifResult.header, sourceColumn);
        return result;
    }

    const ConditionParseResult whileResult = parseConditionHeaderDetailed(tokens, "while", "while");
    if (whileResult.matched) {
        result.kind = StatementParseResult::Kind::While;
        result.ok = whileResult.ok;
        result.errorOffset = whileResult.errorOffset;
        result.message = whileResult.message;
        result.statement = std::make_unique<WhileStmt>(whileResult.header, sourceColumn);
        return result;
    }

    const ConditionParseResult elseIfResult = parseElseIfHeaderDetailed(tokens);
    if (elseIfResult.matched) {
        result.kind = StatementParseResult::Kind::ElseIf;
        result.ok = elseIfResult.ok;
        result.errorOffset = elseIfResult.errorOffset;
        result.message = elseIfResult.message;
        result.statement = std::make_unique<ElseIfStmt>(elseIfResult.header, sourceColumn);
        return result;
    }

    if (parseElseHeader(tokens)) {
        result.kind = StatementParseResult::Kind::Else;
        result.statement = std::make_unique<ElseStmt>(sourceColumn);
        return result;
    }

    if (parseNobreakHeader(tokens)) {
        result.kind = StatementParseResult::Kind::Nobreak;
        result.statement = std::make_unique<NobreakStmt>(sourceColumn);
        return result;
    }

    const std::string trimmed = tokenSpelling(tokens);
    result.kind = StatementParseResult::Kind::Raw;
    result.statement = std::make_unique<RawStmt>(trimmed, sourceColumn);
    return result;
}
