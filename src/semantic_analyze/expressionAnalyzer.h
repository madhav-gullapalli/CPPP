/* Semantic analysis for existing expression AST nodes. */

#pragma once

#include "exprAst.h"
#include "functions.h"

bool analyzeExpressionAst(
    Expr& expression,
    const std::string& inputFile,
    int lineNumber,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    const std::map<std::string, int>* futureVariableLines = nullptr
);
