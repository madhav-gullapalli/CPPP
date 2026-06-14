#pragma once

#include <cstddef>
#include <string>

struct ConditionHeader {
    std::string condition;
    size_t conditionOffset;
};

struct ForHeader {
    std::string initializer;
    size_t initializerOffset;
    std::string condition;
    size_t conditionOffset;
    std::string iteration;
    size_t iterationOffset;
};

struct ForEachHeader {
    std::string declaration;
    size_t declarationOffset;
    std::string variableName;
    size_t variableOffset;
    std::string iterable;
    size_t iterableOffset;
};

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

bool parseConditionHeader(const std::string& statement, const std::string& keyword, ConditionHeader& header);
bool parseForHeader(const std::string& statement, ForHeader& header);
ForEachParseResult parseForEachHeader(const std::string& statement);
ConditionParseResult parseConditionHeaderDetailed(const std::string& statement, const std::string& keyword, const std::string& syntaxName);
ConditionParseResult parseElseIfHeaderDetailed(const std::string& statement);
ForParseResult parseForHeaderDetailed(const std::string& statement);
bool parseElseHeader(const std::string& statement);
bool parseElseIfHeader(const std::string& statement, ConditionHeader& header);
