#pragma once

#include "errors.h"
#include "expressions.h"
#include "tokenizer.h"

#include <map>
#include <string>
#include <vector>

struct FunctionParameter {
    std::string name;
    Type type;
    int column = 0;
};

struct FunctionSignature {
    std::string name;
    Type returnType;
    bool returnsVoid = false;
    std::vector<FunctionParameter> parameters;
};

struct ParsedFunctionHeader {
    bool matched = false;
    bool ok = true;
    FunctionSignature signature;
    std::string generatedSignature;
    int nameColumn = 0;
};

ParsedFunctionHeader parseFunctionHeader(
    const std::string& inputFile,
    int lineNumber,
    const std::string& statementBody,
    int statementColumn,
    const std::map<int, std::string>& sourceLines
);

std::string functionParameterTypesDescription(const FunctionSignature& signature);
std::string functionArgumentTypesDescription(const std::vector<Type>& argumentTypes);
