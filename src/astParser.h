/* Syntax-only full-program parser. */

#pragma once

#include "programAst.h"
#include "tokenizer.h"

#include <string>
#include <vector>

ProgramAst parseProgramAst(const TokenStream& tokenStream);

// Transitional lowering used by the existing semantic/code-generation pass.
// The returned fragments are produced by walking the AST in source order.
std::vector<SourceFragment> lowerProgramAstToFragments(const ProgramAst& program);

// Returns false only for broken internal AST invariants. User syntax errors are
// represented by ErrorStatementAst and remain valid trees.
bool validateProgramAst(const ProgramAst& program, std::string& message);

