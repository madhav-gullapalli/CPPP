/*
 * controlFlow.cpp
 *
 * Parses control-flow headers from canonical source tokens.
 */

#include "controlFlow.h"

#include <algorithm>

namespace {
bool isTypeRootName(const std::string& text) {
    return text == "bool" || text == "char" || text == "int" || text == "float" ||
        text == "List" || text == "Set" || text == "Map" || text == "Pair" ||
        text == "string" || text == "range";
}

struct TypeTokenParseResult {
    bool matched = false;
    bool ok = false;
    size_t nextTokenIndex = 0;
};

TypeTokenParseResult parseTypeTokenSequence(const std::vector<Token>& tokens, size_t startIndex) {
    TypeTokenParseResult result;
    if (startIndex >= tokens.size() || tokens[startIndex].kind != TokenKind::Identifier ||
        !isTypeRootName(tokens[startIndex].text)) {
        return result;
    }

    result.matched = true;
    result.ok = true;
    result.nextTokenIndex = startIndex + 1;
    const std::string& typeName = tokens[startIndex].text;
    if (typeName != "List" && typeName != "Set" && typeName != "Map" && typeName != "Pair") {
        return result;
    }

    if (result.nextTokenIndex >= tokens.size() || tokens[result.nextTokenIndex].text != "<") {
        result.ok = false;
        return result;
    }
    TypeTokenParseResult subtype = parseTypeTokenSequence(tokens, ++result.nextTokenIndex);
    if (!subtype.matched || !subtype.ok) {
        result.ok = false;
        return result;
    }
    result.nextTokenIndex = subtype.nextTokenIndex;

    if (typeName == "Map" || typeName == "Pair") {
        if (result.nextTokenIndex >= tokens.size() || tokens[result.nextTokenIndex].kind != TokenKind::Comma) {
            result.ok = false;
            return result;
        }
        subtype = parseTypeTokenSequence(tokens, ++result.nextTokenIndex);
        if (!subtype.matched || !subtype.ok) {
            result.ok = false;
            return result;
        }
        result.nextTokenIndex = subtype.nextTokenIndex;
    }

    if (result.nextTokenIndex >= tokens.size() || tokens[result.nextTokenIndex].text != ">") {
        result.ok = false;
        return result;
    }
    ++result.nextTokenIndex;
    return result;
}

std::vector<Token> codeTokens(const std::vector<Token>& tokens) {
    std::vector<Token> result;
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::EndOfFile || token.kind == TokenKind::LineComment) break;
        result.push_back(token);
    }
    return result;
}

std::vector<Token> tokenRange(const std::vector<Token>& tokens, size_t begin, size_t end) {
    std::vector<Token> result;
    if (begin >= end || end > tokens.size()) return result;
    const int startColumn = tokens[begin].span.startColumn;
    const size_t startOffset = tokens[begin].span.startOffset;
    for (size_t index = begin; index < end; ++index) {
        Token token = tokens[index];
        token.span.startColumn -= startColumn - 1;
        token.span.endColumn -= startColumn - 1;
        token.span.startOffset -= startOffset;
        token.span.endOffset -= startOffset;
        result.push_back(std::move(token));
    }
    Token eof = result.back();
    eof.kind = TokenKind::EndOfFile;
    eof.text.clear();
    eof.span.startColumn = eof.span.endColumn + 1;
    eof.span.endColumn = eof.span.startColumn;
    eof.span.startOffset = eof.span.endOffset;
    result.push_back(std::move(eof));
    return result;
}

size_t offsetOf(const std::vector<Token>& tokens, size_t index, size_t fallback = 0) {
    return index < tokens.size() ? tokens[index].span.startOffset : fallback;
}

size_t findMatchingRightParen(const std::vector<Token>& tokens, size_t leftParen) {
    int depth = 0;
    for (size_t index = leftParen; index < tokens.size(); ++index) {
        if (tokens[index].kind == TokenKind::LeftParen) ++depth;
        if (tokens[index].kind == TokenKind::RightParen && --depth == 0) return index;
    }
    return tokens.size();
}

ConditionParseResult conditionError(size_t offset, const std::string& message) {
    ConditionParseResult result;
    result.matched = true;
    result.errorOffset = offset;
    result.message = message;
    return result;
}
}

ConditionParseResult parseConditionHeaderDetailed(
    const std::vector<Token>& sourceTokens,
    const std::string& keyword,
    const std::string& syntaxName
) {
    const std::vector<Token> tokens = codeTokens(sourceTokens);
    ConditionParseResult result;
    if (tokens.empty() || tokens[0].kind != TokenKind::Identifier || tokens[0].text != keyword) return result;
    result.matched = true;
    if (tokens.back().kind != TokenKind::LeftBrace) return conditionError(tokens[0].span.endOffset, "missing '{' after " + syntaxName);
    if (tokens.size() < 2 || tokens[1].kind != TokenKind::LeftParen) return conditionError(tokens[0].span.endOffset, "expected '(' after " + syntaxName);
    const size_t rightParen = findMatchingRightParen(tokens, 1);
    if (rightParen == tokens.size()) return conditionError(tokens[1].span.endOffset, "unclosed parenthesis in " + syntaxName);
    if (rightParen + 1 != tokens.size() - 1) return conditionError(tokens[rightParen].span.endOffset, "expected '{' after " + syntaxName + " condition");
    result.ok = true;
    result.header.conditionOffset = rightParen == 2 ? tokens[1].span.endOffset : offsetOf(tokens, 2);
    result.header.conditionTokens = tokenRange(tokens, 2, rightParen);
    return result;
}

ConditionParseResult parseElseIfHeaderDetailed(const std::vector<Token>& sourceTokens) {
    const std::vector<Token> tokens = codeTokens(sourceTokens);
    ConditionParseResult result;
    if (tokens.size() < 2 || tokens[0].text != "else" || tokens[1].text != "if") return result;
    std::vector<Token> nested(tokens.begin() + 1, tokens.end());
    result = parseConditionHeaderDetailed(nested, "if", "else if");
    result.matched = true;
    return result;
}

ForParseResult parseForHeaderDetailed(const std::vector<Token>& sourceTokens) {
    const std::vector<Token> tokens = codeTokens(sourceTokens);
    ForParseResult result;
    if (tokens.empty() || tokens[0].text != "for") return result;
    result.matched = true;
    if (tokens.back().kind != TokenKind::LeftBrace) { result.errorOffset = tokens[0].span.endOffset; result.message = "missing '{' after for loop"; return result; }
    if (tokens.size() < 2 || tokens[1].kind != TokenKind::LeftParen) { result.errorOffset = tokens[0].span.endOffset; result.message = "expected '(' after for"; return result; }
    const size_t rightParen = findMatchingRightParen(tokens, 1);
    if (rightParen == tokens.size()) { result.errorOffset = tokens[1].span.endOffset; result.message = "unclosed parenthesis in for"; return result; }
    if (rightParen + 1 != tokens.size() - 1) { result.errorOffset = tokens[rightParen].span.endOffset; result.message = "expected '{' after for header"; return result; }

    std::vector<size_t> semicolons;
    int depth = 0;
    for (size_t index = 2; index < rightParen; ++index) {
        if (tokens[index].kind == TokenKind::LeftParen || tokens[index].kind == TokenKind::LeftBracket) ++depth;
        else if (tokens[index].kind == TokenKind::RightParen || tokens[index].kind == TokenKind::RightBracket) --depth;
        else if (tokens[index].kind == TokenKind::Semicolon && depth == 0) semicolons.push_back(index);
    }
    if (semicolons.size() != 2) { result.errorOffset = tokens[1].span.endOffset; result.message = "for loop must use syntax for (init; condition; step)"; return result; }

    result.ok = true;
    const size_t first = semicolons[0], second = semicolons[1];
    result.header.initializerOffset = first == 2 ? tokens[1].span.endOffset : offsetOf(tokens, 2);
    result.header.initializerTokens = tokenRange(tokens, 2, first);
    result.header.conditionOffset = second == first + 1 ? tokens[first].span.endOffset : offsetOf(tokens, first + 1);
    result.header.conditionTokens = tokenRange(tokens, first + 1, second);
    result.header.iterationOffset = rightParen == second + 1 ? tokens[second].span.endOffset : offsetOf(tokens, second + 1);
    result.header.iterationTokens = tokenRange(tokens, second + 1, rightParen);
    return result;
}

ForEachParseResult parseForEachHeader(const std::vector<Token>& sourceTokens) {
    const std::vector<Token> tokens = codeTokens(sourceTokens);
    ForEachParseResult result;
    if (tokens.empty() || tokens[0].text != "for") return result;
    for (size_t index = 2; index < tokens.size(); ++index) {
        if (tokens[index].kind == TokenKind::Semicolon) return result;
    }
    result.matched = true;
    if (tokens.size() < 4 || tokens[1].kind != TokenKind::LeftParen || tokens.back().kind != TokenKind::LeftBrace) {
        result.errorOffset = tokens[0].span.endOffset;
        result.message = "for-in loop must use syntax for (T x in list) or for (var x in list)";
        return result;
    }
    const size_t rightParen = findMatchingRightParen(tokens, 1);
    if (rightParen == tokens.size() || rightParen + 1 != tokens.size() - 1) {
        result.errorOffset = tokens[1].span.endOffset;
        result.message = "for-in loop must use syntax for (T x in list) or for (var x in list)";
        return result;
    }
    size_t variableIndex = 0;
    if (tokens[2].text == "var") {
        result.header.usesVar = true;
        variableIndex = 3;
    } else {
        const TypeTokenParseResult type = parseTypeTokenSequence(tokens, 2);
        if (!type.matched) { result.errorOffset = offsetOf(tokens, 2); result.message = "for-in loop must start with a type or var and variable like for (int x in list)"; return result; }
        if (!type.ok) { result.ok = true; result.header.declarationTokens = tokenRange(tokens, 2, rightParen); result.header.declarationOffset = offsetOf(tokens, 2); return result; }
        variableIndex = type.nextTokenIndex;
    }
    if (variableIndex >= rightParen || tokens[variableIndex].kind != TokenKind::Identifier || tokens[variableIndex].text == "in") { result.errorOffset = offsetOf(tokens, variableIndex, tokens[1].span.endOffset); result.message = "expected loop variable before 'in'"; return result; }
    if (variableIndex + 1 >= rightParen || tokens[variableIndex + 1].text != "in") { result.errorOffset = offsetOf(tokens, variableIndex + 1, tokens[variableIndex].span.endOffset); result.message = "expected 'in' after loop variable"; return result; }
    if (variableIndex + 2 >= rightParen) { result.errorOffset = tokens[variableIndex + 1].span.endOffset; result.message = "expected List expression after 'in'"; return result; }

    result.ok = true;
    result.header.declarationOffset = offsetOf(tokens, 2);
    result.header.declarationTokens = tokenRange(tokens, 2, variableIndex + 1);
    result.header.variableName = tokens[variableIndex].text;
    result.header.variableOffset = tokens[variableIndex].span.startOffset;
    result.header.iterableOffset = tokens[variableIndex + 2].span.startOffset;
    result.header.iterableTokens = tokenRange(tokens, variableIndex + 2, rightParen);
    return result;
}

bool parseElseHeader(const std::vector<Token>& sourceTokens) {
    const std::vector<Token> tokens = codeTokens(sourceTokens);
    return tokens.size() == 2 && tokens[0].text == "else" && tokens[1].kind == TokenKind::LeftBrace;
}

bool parseNobreakHeader(const std::vector<Token>& sourceTokens) {
    const std::vector<Token> tokens = codeTokens(sourceTokens);
    return tokens.size() == 2 && tokens[0].text == "nobreak" && tokens[1].kind == TokenKind::LeftBrace;
}
