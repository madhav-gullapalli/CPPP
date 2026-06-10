#include "controlFlow.h"

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
