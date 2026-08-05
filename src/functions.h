/*
 * functions.h
 *
 * Declares function signatures, parameters, and related metadata structures.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "errors.h"
#include "expressions.h"
#include "tokenizer.h"

#include <map>
#include <string>
#include <vector>

// FunctionParameter implements the FunctionParameter behavior for the functions.h module.
struct FunctionParameter {
    std::string name;
    Type type;
    bool copyParameter = false;
    int column = 0;
};

// FunctionSignature implements the FunctionSignature behavior for the functions.h module.
struct FunctionSignature {
    std::string name;
    Type returnType;
    bool returnsVoid = false;
    std::vector<FunctionParameter> parameters;
};

// ParsedFunctionHeader parses dfunctionheaders for the compiler pipeline.
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
    int statementColumn,
    const std::map<int, std::string>& sourceLines,
    const std::vector<Token>& sourceTokens
);

// functionParameterTypesDescription implements the functionParameterTypesDescription behavior for the functions.h module.
std::string functionParameterTypesDescription(const FunctionSignature& signature);
// functionArgumentTypesDescription implements the functionArgumentTypesDescription behavior for the functions.h module.
std::string functionArgumentTypesDescription(const std::vector<Type>& argumentTypes);
