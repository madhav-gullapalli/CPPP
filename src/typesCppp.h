/*
 * typesCppp.h
 *
 * Declares the runtime helper and type-emission structures used across compiler passes.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "errors.h"
#include "expressions.h"

#include <map>
#include <set>
#include <string>
#include <vector>

// TypeEmitResult implements the TypeEmitResult behavior for the typesCppp.h module.
struct TypeEmitResult {
    bool matched;
    bool ok;
    std::string generatedStatement;
    std::vector<SourceRange> sourceRanges;
};

// RuntimeHelper provides runtime support for generated code.
struct RuntimeHelper {
    std::string name;
    std::vector<std::string> code;
    std::vector<std::string> deps;
    std::vector<std::string> triggers;
};

struct ResolvedDeclaredName {
    std::string name;
    int column = 1;
};

// Structural declaration prefix supplied by ProgramAst. Initializer tokens
// remain an expression-level compatibility input.
struct ResolvedDeclarationSyntax {
    bool inferred = false;
    Type type;
    std::vector<ResolvedDeclaredName> names;
    size_t continuationTokenIndex = 0;
};

// runtimeHelpers provides runtime support for generated code.
std::vector<RuntimeHelper> runtimeHelpers();
// typeSupportPreamble implements the typeSupportPreamble behavior for the typesCppp.h module.
std::vector<std::string> typeSupportPreamble();
// typeSupportPreambleForSubmit implements the typeSupportPreambleForSubmit behavior for the typesCppp.h module.
std::vector<std::string> typeSupportPreambleForSubmit(
    const std::set<std::string>& requiredHelpers,
    const std::set<std::string>& requiredContainerTypes = {},
    const std::set<std::string>& requiredContainerMembers = {}
);
void clearRequiredRuntimeHelpers();
void requireRuntimeHelper(const std::string& helperName);
const std::set<std::string>& requiredRuntimeHelpers();
void requireCopyHelpersForType(const Type& type);
void requirePrintHelpersForType(const Type& type);
void setRuntimeRequirementOwner(const std::string& ownerKey);
std::set<std::string> requiredRuntimeHelpersForOwners(const std::set<std::string>& ownerKeys);
void requireContainerMember(const Type& type, const std::string& memberName);
std::set<std::string> requiredContainerMembersForOwners(const std::set<std::string>& ownerKeys);
void requireStructMethod(const std::string& structName, const std::string& methodName);
const std::set<std::string>& requiredStructMethods();
// cppTypeForType implements the cppTypeForType behavior for the typesCppp.h module.
std::string cppTypeForType(const Type& type);
TypeEmitResult emitResolvedTypeDeclaration(
    const std::string& inputFile,
    int lineNumber,
    int statementStartColumn,
    const std::map<int, std::string>& sourceLines,
    std::map<std::string, Type>& declaredVariables,
    const ResolvedDeclarationSyntax& declaration,
    const std::vector<Token>& sourceTokens
);
