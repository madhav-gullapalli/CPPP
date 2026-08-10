/* C++ generation for semantically analyzed expression AST nodes. */

#pragma once

#include "exprAst.h"
#include "functions.h"

#include <map>
#include <string>

std::string generateAnalyzedExpression(
    const Expr& expression,
    int lineNumber,
    bool emitRuntimeChecks,
    const std::map<std::string, FunctionSignature>& declaredFunctions
);
