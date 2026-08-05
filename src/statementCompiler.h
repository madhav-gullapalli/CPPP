/*
 * statementCompiler.h
 *
 * Main statement-lowering stage.
 *
 * This stage consumes the canonical whole-file TokenStream, derives temporary
 * statement views, updates scope state, and queues generated C++ lines.
 */

#pragma once

#include "compileContext.h"

#include <vector>

// Lowers a canonical source token stream into generated C++ while updating the
// shared CompileContext.
void compileTokenStream(CompileContext& context, const TokenStream& tokenStream);
