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

bool parseConditionHeader(const std::string& statement, const std::string& keyword, ConditionHeader& header);
bool parseForHeader(const std::string& statement, ForHeader& header);
bool parseElseHeader(const std::string& statement);
bool parseElseIfHeader(const std::string& statement, ConditionHeader& header);
