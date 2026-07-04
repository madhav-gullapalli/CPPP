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

// Parsed payload of `if (...)`, `while (...)`, or `else if (...)`.
struct ConditionHeader {
    std::string condition;
    size_t conditionOffset;
};

// Parsed parts of `for (init; condition; iteration)`.
struct ForHeader {
    std::string initializer;
    size_t initializerOffset;
    std::string condition;
    size_t conditionOffset;
    std::string iteration;
    size_t iterationOffset;
};

// Parsed parts of CP++ `for (T x in iterable)`.
struct ForEachHeader {
    std::string declaration;
    size_t declarationOffset;
    std::string variableName;
    size_t variableOffset;
    std::string iterable;
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

// Fast boolean-style helpers.
bool parseConditionHeader(const std::string& statement, const std::string& keyword, ConditionHeader& header);
bool parseForHeader(const std::string& statement, ForHeader& header);

// Detailed helpers preserve user-facing diagnostics and exact source offsets.
ForEachParseResult parseForEachHeader(const std::string& statement);
ConditionParseResult parseConditionHeaderDetailed(const std::string& statement, const std::string& keyword, const std::string& syntaxName);
ConditionParseResult parseElseIfHeaderDetailed(const std::string& statement);
ForParseResult parseForHeaderDetailed(const std::string& statement);
bool parseElseHeader(const std::string& statement);
bool parseElseIfHeader(const std::string& statement, ConditionHeader& header);
