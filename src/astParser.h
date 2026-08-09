/* Syntax-only full-program parser. */

#pragma once

#include "programAst.h"
#include "tokenizer.h"

#include <string>
ProgramAst parseProgramAst(const TokenStream& tokenStream);

// Returns false only for broken internal AST invariants. User syntax errors are
// represented by ErrorStatementAst and remain valid trees.
bool validateProgramAst(const ProgramAst& program, std::string& message);
