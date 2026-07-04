/*
 * statementCompiler.h
 *
 * Main statement-lowering stage.
 *
 * This stage consumes SourceFragment records, recognizes statement/control-flow
 * forms, updates scope state, and queues generated C++ lines into
 * CompileContext.
 */

#pragma once

#include "compileContext.h"

#include <vector>

// Lowers the logical source fragments produced by sourceSplitter into generated
// C++ while updating the shared CompileContext.
void compileSourceFragments(CompileContext& context, const std::vector<SourceFragment>& sourceFragments);
