/* Deterministic printer for the analyzed CP++ AST. */

#pragma once

#include "semanticAst.h"

#include <iosfwd>

void printAnalyzedProgramAst(std::ostream& output, const AnalyzedProgramAst& program);
