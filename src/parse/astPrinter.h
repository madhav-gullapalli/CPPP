/* Deterministic tree printer for ProgramAst. */

#pragma once

#include "programAst.h"

#include <iosfwd>

void printProgramAst(std::ostream& output, const ProgramAst& program);

