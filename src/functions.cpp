/*
 * functions.cpp
 *
 * Implements function parameter typing and runtime helper selection for function lowering.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "functions.h"

// functionParameterTypesDescription implements the functionParameterTypesDescription behavior for the functions.cpp module.
std::string functionParameterTypesDescription(const FunctionSignature& signature) {
    if (signature.parameters.empty()) {
        return "no";
    }

    std::string description;
    for (size_t i = 0; i < signature.parameters.size(); ++i) {
        if (i > 0) {
            description += " and ";
        }
        description += cpppTypeName(signature.parameters[i].type);
    }
    return description;
}

// functionArgumentTypesDescription implements the functionArgumentTypesDescription behavior for the functions.cpp module.
std::string functionArgumentTypesDescription(const std::vector<Type>& argumentTypes) {
    if (argumentTypes.empty()) {
        return "no";
    }

    std::string description;
    for (size_t i = 0; i < argumentTypes.size(); ++i) {
        if (i > 0) {
            description += " and ";
        }
        description += cpppTypeName(argumentTypes[i]);
    }
    return description;
}
