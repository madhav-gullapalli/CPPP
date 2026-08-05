/*
 * controlFlow.h
 *
 * Lightweight parsing helpers for control-flow headers.
 *
 * These helpers parse the "header" portion of `if`, `while`, classic `for`,
 * and CP++ `for-in` statements. Full block lowering still happens in
 * statementCompiler.cpp.
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "tokenizer.h"

// Parsed payload of `if (...)`, `while (...)`, or `else if (...)`.
struct ConditionHeader {
    std::vector<Token> conditionTokens;
    size_t conditionOffset;
};

// Parsed parts of `for (init; condition; iteration)`.
struct ForHeader {
    std::vector<Token> initializerTokens;
    size_t initializerOffset;
    std::vector<Token> conditionTokens;
    size_t conditionOffset;
    std::vector<Token> iterationTokens;
    size_t iterationOffset;
};

// Parsed parts of CP++ `for (T x in iterable)`.
struct ForEachHeader {
    std::vector<Token> declarationTokens;
    size_t declarationOffset;
    bool usesVar = false;
    std::string variableName;
    size_t variableOffset;
    std::vector<Token> iterableTokens;
    size_t iterableOffset;
};

// Rich result used when callers need exact source offsets and error messages.
struct ForEachParseResult {
    bool matched = false;
    bool ok = false;
    ForEachHeader header;
    size_t errorOffset = 0;
    std::string message;
};

struct ConditionParseResult {
    bool matched = false;
    bool ok = false;
    ConditionHeader header;
    size_t errorOffset = 0;
    std::string message;
};

struct ForParseResult {
    bool matched = false;
    bool ok = false;
    ForHeader header;
    size_t errorOffset = 0;
    std::string message;
};

// Detailed helpers preserve user-facing diagnostics and exact source offsets.
ForEachParseResult parseForEachHeader(const std::vector<Token>& tokens);
ConditionParseResult parseConditionHeaderDetailed(const std::vector<Token>& tokens, const std::string& keyword, const std::string& syntaxName);
ConditionParseResult parseElseIfHeaderDetailed(const std::vector<Token>& tokens);
ForParseResult parseForHeaderDetailed(const std::vector<Token>& tokens);
bool parseElseHeader(const std::vector<Token>& tokens);
bool parseNobreakHeader(const std::vector<Token>& tokens);
