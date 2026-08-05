/*
 * sourceSplitter.h
 *
 * Token-to-statement compatibility stage.
 *
 * This module does not rescan source text. It groups the canonical file token
 * stream into logical SourceFragment views for the existing statement parser.
 */

#pragma once

#include "compileContext.h"

#include <string>
#include <vector>

// Groups a canonical whole-file token stream into logical statement views.
std::vector<SourceFragment> splitTokenStream(const TokenStream& tokenStream);
