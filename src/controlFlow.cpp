#include "controlFlow.h"

#include "tokenizer.h"

#include <cctype>
#include <vector>

namespace {
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

bool startsWithWord(const std::string& text, const std::string& word) {
    if (text.rfind(word, 0) != 0) {
        return false;
    }

    return text.size() == word.size() ||
        !(std::isalnum(static_cast<unsigned char>(text[word.size()])) || text[word.size()] == '_');
}

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

std::string trimHeaderPart(const std::string& raw, size_t baseOffset, size_t& offset) {
    const size_t trimStart = raw.find_first_not_of(" \t\r\n");
    offset = baseOffset + (trimStart == std::string::npos ? 0 : trimStart);
    return trim(raw);
}

bool isTypeRootName(const std::string& text) {
    return text == "bool" || text == "char" || text == "int" || text == "float" || text == "List" || text == "string";
}

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

    if (tokens[startIndex].text != "List") {
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
}

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
        result.message = "for-in loop must use syntax for (T x in list)";
        return result;
    }

    if (!trim(withoutBrace.substr(3, leftParen - 3)).empty() ||
        !trim(withoutBrace.substr(rightParen + 1)).empty()) {
        result.errorOffset = 3;
        result.message = "for-in loop must use syntax for (T x in list)";
        return result;
    }

    const std::string rawHeader = withoutBrace.substr(leftParen + 1, rightParen - leftParen - 1);
    if (!topLevelSemicolons(rawHeader).empty()) {
        result.matched = false;
        return result;
    }

    const std::vector<Token> tokens = tokenize(rawHeader);
    TypeTokenParseResult typeResult = parseTypeTokenSequence(tokens, 0);
    if (!typeResult.matched) {
        result.errorOffset = leftParen + 1;
        result.message = "for-in loop must start with a type and variable like for (int x in list)";
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

    const size_t variableIndex = typeResult.nextTokenIndex;
    if (tokens[variableIndex].text == "in") {
        result.errorOffset = leftParen + 1 + static_cast<size_t>(tokens[variableIndex].span.startColumn - 1);
        result.message = "expected loop variable before 'in'";
        return result;
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

bool parseElseHeader(const std::string& statement) {
    if (statement.empty() || statement.back() != '{') {
        return false;
    }

    return trim(statement.substr(0, statement.size() - 1)) == "else";
}

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
