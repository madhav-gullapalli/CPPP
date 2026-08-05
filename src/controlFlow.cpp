/*
 * controlFlow.cpp
 *
 * Parses and lowers control-flow constructs such as loops, conditionals, and break/continue helpers.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "controlFlow.h"

#include "tokenizer.h"

#include <cctype>
#include <vector>

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

// startsWithWord returns whether the text starts with the given prefix.
bool startsWithWord(const std::string& text, const std::string& word) {
    if (text.rfind(word, 0) != 0) {
        return false;
    }

    return text.size() == word.size() ||
        !(std::isalnum(static_cast<unsigned char>(text[word.size()])) || text[word.size()] == '_');
}

// topLevelSemicolons implements the topLevelSemicolons behavior for the controlFlow.cpp module.
std::vector<size_t> topLevelSemicolons(const std::string& text) {
    std::vector<size_t> semicolons;
    int parenDepth = 0;
    bool inString = false;
    bool inChar = false;
    bool escaped = false;

    for (size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if ((inString || inChar) && ch == '\\') {
            escaped = true;
            continue;
        }

        if (!inChar && ch == '"') {
            inString = !inString;
            continue;
        }

        if (!inString && ch == '\'') {
            inChar = !inChar;
            continue;
        }

        if (inString || inChar) {
            continue;
        }

        if (ch == '(') {
            ++parenDepth;
        } else if (ch == ')' && parenDepth > 0) {
            --parenDepth;
        } else if (ch == ';' && parenDepth == 0) {
            semicolons.push_back(i);
        }
    }

    return semicolons;
}

// trimHeaderPart removes surrounding whitespace from a string.
std::string trimHeaderPart(const std::string& raw, size_t baseOffset, size_t& offset) {
    const size_t trimStart = raw.find_first_not_of(" \t\r\n");
    offset = baseOffset + (trimStart == std::string::npos ? 0 : trimStart);
    return trim(raw);
}

// isTypeRootName returns whether the supplied input satisfies the relevant condition.
bool isTypeRootName(const std::string& text) {
    return text == "bool" || text == "char" || text == "int" || text == "float" ||
        text == "List" || text == "Set" || text == "Map" || text == "Pair" || text == "string" || text == "range";
}

// TypeTokenParseResult implements the TypeTokenParseResult behavior for the controlFlow.cpp module.
struct TypeTokenParseResult {
    bool matched = false;
    bool ok = false;
    size_t nextTokenIndex = 0;
};

TypeTokenParseResult parseTypeTokenSequence(const std::vector<Token>& tokens, size_t startIndex) {
    TypeTokenParseResult result;
    if (startIndex >= tokens.size() || tokens[startIndex].kind != TokenKind::Identifier || !isTypeRootName(tokens[startIndex].text)) {
        return result;
    }

    result.matched = true;
    result.ok = true;
    result.nextTokenIndex = startIndex + 1;

    if (tokens[startIndex].text != "List" && tokens[startIndex].text != "Set" && tokens[startIndex].text != "Map" && tokens[startIndex].text != "Pair") {
        return result;
    }

    if (result.nextTokenIndex >= tokens.size() ||
        tokens[result.nextTokenIndex].kind != TokenKind::Operator ||
        tokens[result.nextTokenIndex].text != "<") {
        result.ok = false;
        return result;
    }

    ++result.nextTokenIndex;
    TypeTokenParseResult subtype = parseTypeTokenSequence(tokens, result.nextTokenIndex);
    if (!subtype.matched || !subtype.ok) {
        result.ok = false;
        return result;
    }
    result.nextTokenIndex = subtype.nextTokenIndex;

    if (tokens[startIndex].text == "Map" || tokens[startIndex].text == "Pair") {
        if (result.nextTokenIndex >= tokens.size() || tokens[result.nextTokenIndex].kind != TokenKind::Comma) {
            result.ok = false;
            return result;
        }

        ++result.nextTokenIndex;
        subtype = parseTypeTokenSequence(tokens, result.nextTokenIndex);
        if (!subtype.matched || !subtype.ok) {
            result.ok = false;
            return result;
        }
        result.nextTokenIndex = subtype.nextTokenIndex;
    }

    if (result.nextTokenIndex >= tokens.size() ||
        tokens[result.nextTokenIndex].kind != TokenKind::Operator ||
        tokens[result.nextTokenIndex].text != ">") {
        result.ok = false;
        return result;
    }

    ++result.nextTokenIndex;
    return result;
}

ConditionParseResult makeConditionError(size_t errorOffset, const std::string& message) {
    ConditionParseResult result;
    result.matched = true;
    result.ok = false;
    result.errorOffset = errorOffset;
    result.message = message;
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

std::string tokenRangeText(const std::vector<Token>& tokens, size_t begin, size_t end) {
    if (begin >= end || end > tokens.size()) return "";
    std::string result;
    size_t previousEnd = tokens[begin].span.startOffset;
    for (size_t index = begin; index < end; ++index) {
        const Token& token = tokens[index];
        if (token.span.startOffset > previousEnd) {
            result.append(token.span.startOffset - previousEnd, ' ');
        }
        result += token.text;
        previousEnd = token.span.endOffset;
    }
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
    if (tokens.back().kind != TokenKind::LeftBrace) return makeConditionError(tokens[0].span.endOffset, "missing '{' after " + syntaxName);
    if (tokens.size() < 2 || tokens[1].kind != TokenKind::LeftParen) return makeConditionError(tokens[0].span.endOffset, "expected '(' after " + syntaxName);
    const size_t rightParen = findMatchingRightParen(tokens, 1);
    if (rightParen == tokens.size()) return makeConditionError(tokens[1].span.endOffset, "unclosed parenthesis in " + syntaxName);
    if (rightParen + 1 != tokens.size() - 1) return makeConditionError(tokens[rightParen].span.endOffset, "expected '{' after " + syntaxName + " condition");
    result.ok = true;
    result.header.conditionOffset = rightParen == 2 ? tokens[1].span.endOffset : offsetOf(tokens, 2);
    result.header.condition = tokenRangeText(tokens, 2, rightParen);
    return result;
}

ConditionParseResult parseElseIfHeaderDetailed(const std::vector<Token>& sourceTokens) {
    const std::vector<Token> tokens = codeTokens(sourceTokens);
    ConditionParseResult result;
    if (tokens.empty() || tokens[0].kind != TokenKind::Identifier || tokens[0].text != "else") return result;
    if (tokens.size() < 2 || tokens[1].kind != TokenKind::Identifier || tokens[1].text != "if") return result;
    std::vector<Token> nested(tokens.begin() + 1, tokens.end());
    result = parseConditionHeaderDetailed(nested, "if", "else if");
    result.matched = true;
    return result;
}

ForParseResult parseForHeaderDetailed(const std::vector<Token>& sourceTokens) {
    const std::vector<Token> tokens = codeTokens(sourceTokens);
    ForParseResult result;
    if (tokens.empty() || tokens[0].kind != TokenKind::Identifier || tokens[0].text != "for") return result;
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
    result.header.initializer = tokenRangeText(tokens, 2, first);
    result.header.conditionOffset = second == first + 1 ? tokens[first].span.endOffset : offsetOf(tokens, first + 1);
    result.header.condition = tokenRangeText(tokens, first + 1, second);
    result.header.iterationOffset = rightParen == second + 1 ? tokens[second].span.endOffset : offsetOf(tokens, second + 1);
    result.header.iteration = tokenRangeText(tokens, second + 1, rightParen);
    return result;
}

ForEachParseResult parseForEachHeader(const std::vector<Token>& sourceTokens) {
    const std::vector<Token> tokens = codeTokens(sourceTokens);
    ForEachParseResult result;
    if (tokens.empty() || tokens[0].kind != TokenKind::Identifier || tokens[0].text != "for") return result;
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
    for (size_t index = 2; index < rightParen; ++index) if (tokens[index].kind == TokenKind::Semicolon) { result.matched = false; return result; }
    size_t variableIndex = 0;
    if (tokens[2].kind == TokenKind::Identifier && tokens[2].text == "var") {
        result.header.usesVar = true; variableIndex = 3;
    } else {
        const TypeTokenParseResult type = parseTypeTokenSequence(tokens, 2);
        if (!type.matched) { result.errorOffset = offsetOf(tokens, 2); result.message = "for-in loop must start with a type or var and variable like for (int x in list)"; return result; }
        if (!type.ok) { result.ok = true; result.header.declaration = tokenRangeText(tokens, 2, rightParen); result.header.declarationOffset = offsetOf(tokens, 2); return result; }
        variableIndex = type.nextTokenIndex;
    }
    if (variableIndex >= rightParen || tokens[variableIndex].kind != TokenKind::Identifier || tokens[variableIndex].text == "in") { result.errorOffset = offsetOf(tokens, variableIndex, tokens[1].span.endOffset); result.message = "expected loop variable before 'in'"; return result; }
    if (variableIndex + 1 >= rightParen || tokens[variableIndex + 1].kind != TokenKind::Identifier || tokens[variableIndex + 1].text != "in") { result.errorOffset = offsetOf(tokens, variableIndex + 1, tokens[variableIndex].span.endOffset); result.message = "expected 'in' after loop variable"; return result; }
    if (variableIndex + 2 >= rightParen) { result.errorOffset = tokens[variableIndex + 1].span.endOffset; result.message = "expected List expression after 'in'"; return result; }
    result.ok = true;
    result.header.declarationOffset = offsetOf(tokens, 2);
    result.header.declaration = tokenRangeText(tokens, 2, variableIndex + 1);
    result.header.variableName = tokens[variableIndex].text;
    result.header.variableOffset = tokens[variableIndex].span.startOffset;
    result.header.iterableOffset = tokens[variableIndex + 2].span.startOffset;
    result.header.iterable = tokenRangeText(tokens, variableIndex + 2, rightParen);
    return result;
}

bool parseElseHeader(const std::vector<Token>& sourceTokens) {
    const std::vector<Token> tokens = codeTokens(sourceTokens);
    return tokens.size() == 2 && tokens[0].kind == TokenKind::Identifier && tokens[0].text == "else" && tokens[1].kind == TokenKind::LeftBrace;
}

bool parseNobreakHeader(const std::vector<Token>& sourceTokens) {
    const std::vector<Token> tokens = codeTokens(sourceTokens);
    return tokens.size() == 2 && tokens[0].kind == TokenKind::Identifier && tokens[0].text == "nobreak" && tokens[1].kind == TokenKind::LeftBrace;
}

// parseConditionHeader parses conditionheaders for the compiler pipeline.
bool parseConditionHeader(const std::string& statement, const std::string& keyword, ConditionHeader& header) {
    if (statement.empty() || statement.back() != '{') {
        return false;
    }

    const std::string withoutBrace = trim(statement.substr(0, statement.size() - 1));
    if (!startsWithWord(withoutBrace, keyword)) {
        return false;
    }

    const size_t leftParen = withoutBrace.find('(', keyword.size());
    const size_t rightParen = withoutBrace.find_last_of(')');
    if (leftParen == std::string::npos || rightParen == std::string::npos || rightParen <= leftParen) {
        return false;
    }

    if (!trim(withoutBrace.substr(keyword.size(), leftParen - keyword.size())).empty() ||
        !trim(withoutBrace.substr(rightParen + 1)).empty()) {
        return false;
    }

    const std::string rawCondition = withoutBrace.substr(leftParen + 1, rightParen - leftParen - 1);
    const size_t trimStart = rawCondition.find_first_not_of(" \t\r\n");
    header.condition = trim(rawCondition);
    header.conditionOffset = leftParen + 1 + (trimStart == std::string::npos ? 0 : trimStart);
    return true;
}

// parseForHeader parses forheaders for the compiler pipeline.
bool parseForHeader(const std::string& statement, ForHeader& header) {
    if (statement.empty() || statement.back() != '{') {
        return false;
    }

    const std::string withoutBrace = trim(statement.substr(0, statement.size() - 1));
    if (!startsWithWord(withoutBrace, "for")) {
        return false;
    }

    const size_t leftParen = withoutBrace.find('(', 3);
    const size_t rightParen = withoutBrace.find_last_of(')');
    if (leftParen == std::string::npos || rightParen == std::string::npos || rightParen <= leftParen) {
        return false;
    }

    if (!trim(withoutBrace.substr(3, leftParen - 3)).empty() ||
        !trim(withoutBrace.substr(rightParen + 1)).empty()) {
        return false;
    }

    const std::string rawHeader = withoutBrace.substr(leftParen + 1, rightParen - leftParen - 1);
    const std::vector<size_t> semicolons = topLevelSemicolons(rawHeader);
    if (semicolons.size() != 2) {
        return false;
    }

    const std::string rawInitializer = rawHeader.substr(0, semicolons[0]);
    const std::string rawCondition = rawHeader.substr(semicolons[0] + 1, semicolons[1] - semicolons[0] - 1);
    const std::string rawIteration = rawHeader.substr(semicolons[1] + 1);

    const size_t headerOffset = leftParen + 1;
    header.initializer = trimHeaderPart(rawInitializer, headerOffset, header.initializerOffset);
    header.condition = trimHeaderPart(rawCondition, headerOffset + semicolons[0] + 1, header.conditionOffset);
    header.iteration = trimHeaderPart(rawIteration, headerOffset + semicolons[1] + 1, header.iterationOffset);
    return true;
}

ForEachParseResult parseForEachHeader(const std::string& statement) {
    ForEachParseResult result;
    if (statement.empty() || statement.back() != '{') {
        return result;
    }

    const std::string withoutBrace = trim(statement.substr(0, statement.size() - 1));
    if (!startsWithWord(withoutBrace, "for")) {
        return result;
    }
    result.matched = true;

    const size_t leftParen = withoutBrace.find('(', 3);
    const size_t rightParen = withoutBrace.find_last_of(')');
    if (leftParen == std::string::npos || rightParen == std::string::npos || rightParen <= leftParen) {
        result.errorOffset = 3;
        result.message = "for-in loop must use syntax for (T x in list) or for (var x in list)";
        return result;
    }

    if (!trim(withoutBrace.substr(3, leftParen - 3)).empty() ||
        !trim(withoutBrace.substr(rightParen + 1)).empty()) {
        result.errorOffset = 3;
        result.message = "for-in loop must use syntax for (T x in list) or for (var x in list)";
        return result;
    }

    const std::string rawHeader = withoutBrace.substr(leftParen + 1, rightParen - leftParen - 1);
    if (!topLevelSemicolons(rawHeader).empty()) {
        result.matched = false;
        return result;
    }

    const std::vector<Token> tokens = tokenize(rawHeader);
    size_t variableIndex = 0;
    if (tokens[0].kind == TokenKind::Identifier && tokens[0].text == "var") {
        result.header.usesVar = true;
        if (tokens.size() <= 2 || tokens[1].kind != TokenKind::Identifier || tokens[1].text == "in") {
            const size_t errorIndex = tokens.size() > 1 ? 1 : 0;
            result.errorOffset = leftParen + 1 + static_cast<size_t>(tokens[errorIndex].span.startColumn - 1);
            result.message = "expected loop variable before 'in'";
            return result;
        }
        variableIndex = 1;
    } else {
        TypeTokenParseResult typeResult = parseTypeTokenSequence(tokens, 0);
        if (!typeResult.matched) {
            result.errorOffset = leftParen + 1;
            result.message = "for-in loop must start with a type or var and variable like for (int x in list)";
            return result;
        }

        if (!typeResult.ok) {
            result.ok = true;
            result.header.declaration = trim(rawHeader);
            result.header.declarationOffset = leftParen + 1;
            result.header.variableName.clear();
            result.header.variableOffset = leftParen + 1;
            result.header.iterable.clear();
            result.header.iterableOffset = leftParen + 1;
            return result;
        }

        if (typeResult.nextTokenIndex >= tokens.size() || tokens[typeResult.nextTokenIndex].kind != TokenKind::Identifier) {
            const size_t errorIndex = std::min(typeResult.nextTokenIndex, tokens.size() - 1);
            result.errorOffset = leftParen + 1 + static_cast<size_t>(tokens[errorIndex].span.startColumn - 1);
            result.message = "expected loop variable before 'in'";
            return result;
        }

        variableIndex = typeResult.nextTokenIndex;
        if (tokens[variableIndex].text == "in") {
            result.errorOffset = leftParen + 1 + static_cast<size_t>(tokens[variableIndex].span.startColumn - 1);
            result.message = "expected loop variable before 'in'";
            return result;
        }
    }

    if (variableIndex + 1 >= tokens.size() ||
        tokens[variableIndex + 1].kind != TokenKind::Identifier ||
        tokens[variableIndex + 1].text != "in") {
        const size_t errorIndex = std::min(variableIndex + 1, tokens.size() - 1);
        result.errorOffset = leftParen + 1 + static_cast<size_t>(tokens[errorIndex].span.startColumn - 1);
        result.message = "expected 'in' after loop variable";
        return result;
    }

    const size_t iterableIndex = variableIndex + 2;
    if (iterableIndex >= tokens.size() || tokens[iterableIndex].kind == TokenKind::EndOfFile) {
        result.errorOffset = leftParen + 1 + static_cast<size_t>(tokens[variableIndex + 1].span.endColumn);
        result.message = "expected List expression after 'in'";
        return result;
    }

    const size_t declarationStart = static_cast<size_t>(tokens[0].span.startColumn - 1);
    const size_t declarationEnd = static_cast<size_t>(tokens[variableIndex].span.endColumn);
    const size_t iterableStart = static_cast<size_t>(tokens[iterableIndex].span.startColumn - 1);
    const size_t iterableEnd = static_cast<size_t>(tokens[tokens.size() - 2].span.endColumn);

    result.ok = true;
    result.header.declaration = trim(rawHeader.substr(declarationStart, declarationEnd - declarationStart));
    result.header.declarationOffset = leftParen + 1 + declarationStart;
    result.header.variableName = tokens[variableIndex].text;
    result.header.variableOffset = leftParen + 1 + static_cast<size_t>(tokens[variableIndex].span.startColumn - 1);
    result.header.iterable = trim(rawHeader.substr(iterableStart, iterableEnd - iterableStart));
    result.header.iterableOffset = leftParen + 1 + iterableStart;
    return result;
}

ConditionParseResult parseConditionHeaderDetailed(const std::string& statement, const std::string& keyword, const std::string& syntaxName) {
    ConditionParseResult result;
    if (!startsWithWord(trim(statement), keyword)) {
        return result;
    }
    result.matched = true;

    if (statement.empty() || statement.back() != '{') {
        return makeConditionError(keyword.size(), "missing '{' after " + syntaxName);
    }

    const std::string withoutBrace = trim(statement.substr(0, statement.size() - 1));
    const size_t leftParen = withoutBrace.find('(', keyword.size());
    if (leftParen == std::string::npos) {
        return makeConditionError(keyword.size(), "expected '(' after " + syntaxName);
    }

    if (!trim(withoutBrace.substr(keyword.size(), leftParen - keyword.size())).empty()) {
        return makeConditionError(keyword.size(), "expected '(' after " + syntaxName);
    }

    const size_t rightParen = withoutBrace.find_last_of(')');
    if (rightParen == std::string::npos || rightParen < leftParen) {
        return makeConditionError(leftParen + 1, "unclosed parenthesis in " + syntaxName);
    }

    if (!trim(withoutBrace.substr(rightParen + 1)).empty()) {
        return makeConditionError(rightParen + 1, "expected '{' after " + syntaxName + " condition");
    }

    const std::string rawCondition = withoutBrace.substr(leftParen + 1, rightParen - leftParen - 1);
    const size_t trimStart = rawCondition.find_first_not_of(" \t\r\n");
    result.ok = true;
    result.header.condition = trim(rawCondition);
    result.header.conditionOffset = leftParen + 1 + (trimStart == std::string::npos ? 0 : trimStart);
    return result;
}

ConditionParseResult parseElseIfHeaderDetailed(const std::string& statement) {
    ConditionParseResult result;
    if (!startsWithWord(trim(statement), "else")) {
        return result;
    }

    result.matched = true;
    if (statement.empty() || statement.back() != '{') {
        return makeConditionError(4, "missing '{' after else if");
    }

    const std::string withoutBrace = trim(statement.substr(0, statement.size() - 1));
    const std::string afterElse = trim(withoutBrace.substr(4));
    if (!startsWithWord(afterElse, "if")) {
        result.matched = false;
        return result;
    }

    ConditionParseResult nested = parseConditionHeaderDetailed(afterElse + "{", "if", "else if");
    nested.matched = true;
    if (nested.ok) {
        const size_t ifPosition = withoutBrace.find("if", 4);
        nested.header.conditionOffset = ifPosition + nested.header.conditionOffset;
    } else {
        const size_t ifPosition = withoutBrace.find("if", 4);
        nested.errorOffset = ifPosition + nested.errorOffset;
    }
    return nested;
}

ForParseResult parseForHeaderDetailed(const std::string& statement) {
    ForParseResult result;
    if (!startsWithWord(trim(statement), "for")) {
        return result;
    }
    result.matched = true;

    if (statement.empty() || statement.back() != '{') {
        result.ok = false;
        result.errorOffset = 3;
        result.message = "missing '{' after for loop";
        return result;
    }

    const std::string withoutBrace = trim(statement.substr(0, statement.size() - 1));
    const size_t leftParen = withoutBrace.find('(', 3);
    if (leftParen == std::string::npos) {
        result.ok = false;
        result.errorOffset = 3;
        result.message = "expected '(' after for";
        return result;
    }

    if (!trim(withoutBrace.substr(3, leftParen - 3)).empty()) {
        result.ok = false;
        result.errorOffset = 3;
        result.message = "expected '(' after for";
        return result;
    }

    const size_t rightParen = withoutBrace.find_last_of(')');
    if (rightParen == std::string::npos || rightParen < leftParen) {
        result.ok = false;
        result.errorOffset = leftParen + 1;
        result.message = "unclosed parenthesis in for";
        return result;
    }

    if (!trim(withoutBrace.substr(rightParen + 1)).empty()) {
        result.ok = false;
        result.errorOffset = rightParen + 1;
        result.message = "expected '{' after for header";
        return result;
    }

    const std::string rawHeader = withoutBrace.substr(leftParen + 1, rightParen - leftParen - 1);
    const std::vector<size_t> semicolons = topLevelSemicolons(rawHeader);
    if (semicolons.size() != 2) {
        result.ok = false;
        result.errorOffset = leftParen + 1;
        result.message = "for loop must use syntax for (init; condition; step)";
        return result;
    }

    result.ok = true;
    const std::string rawInitializer = rawHeader.substr(0, semicolons[0]);
    const std::string rawCondition = rawHeader.substr(semicolons[0] + 1, semicolons[1] - semicolons[0] - 1);
    const std::string rawIteration = rawHeader.substr(semicolons[1] + 1);
    const size_t headerOffset = leftParen + 1;
    result.header.initializer = trimHeaderPart(rawInitializer, headerOffset, result.header.initializerOffset);
    result.header.condition = trimHeaderPart(rawCondition, headerOffset + semicolons[0] + 1, result.header.conditionOffset);
    result.header.iteration = trimHeaderPart(rawIteration, headerOffset + semicolons[1] + 1, result.header.iterationOffset);
    return result;
}

// parseElseHeader parses elseheaders for the compiler pipeline.
bool parseElseHeader(const std::string& statement) {
    if (statement.empty() || statement.back() != '{') {
        return false;
    }

    return trim(statement.substr(0, statement.size() - 1)) == "else";
}

// parseNobreakHeader parses the loop completion block header.
bool parseNobreakHeader(const std::string& statement) {
    if (statement.empty() || statement.back() != '{') {
        return false;
    }

    return trim(statement.substr(0, statement.size() - 1)) == "nobreak";
}

// parseElseIfHeader parses elseifheaders for the compiler pipeline.
bool parseElseIfHeader(const std::string& statement, ConditionHeader& header) {
    if (statement.empty() || statement.back() != '{') {
        return false;
    }

    const std::string withoutBrace = trim(statement.substr(0, statement.size() - 1));
    if (!startsWithWord(withoutBrace, "else")) {
        return false;
    }

    const std::string afterElse = trim(withoutBrace.substr(4));
    ConditionHeader afterElseHeader;
    if (!parseConditionHeader(afterElse + "{", "if", afterElseHeader)) {
        return false;
    }

    const size_t ifPosition = withoutBrace.find("if", 4);
    header.condition = afterElseHeader.condition;
    header.conditionOffset = ifPosition + afterElseHeader.conditionOffset;
    return true;
}
