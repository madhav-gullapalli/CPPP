/* Lightweight ownership boundary between syntax parsing and code generation. */

#pragma once

#include "functions.h"
#include "programAst.h"

#include <map>
#include <set>
#include <string>
#include <vector>

struct AnalyzedProgramAst {
    ProgramAst* program = nullptr;
    bool valid = false;
    std::map<std::string, FunctionSignature> functions;
    std::map<std::string, std::map<std::string, Type>> aggregateFields;
    std::map<std::string, std::vector<std::string>> aggregateFieldOrder;
    std::map<std::string, std::map<std::string, FunctionSignature>> aggregateMethods;
    std::map<std::string, FunctionSignature> aggregateConstructors;
    std::set<std::string> classNames;
    std::vector<std::string> aggregateEmissionOrder;
};
