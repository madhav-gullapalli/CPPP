/*
 * sourceSplitter.h
 *
 * First compiler stage after file I/O.
 *
 * This module does not fully parse CP++; it normalizes raw source text into
 * SourceFragment records that later stages can lower more easily while still
 * preserving the original source lines for diagnostics.
 */

#pragma once

#include "compileContext.h"

#include <istream>
#include <map>
#include <string>
#include <vector>

// Finds the first `//` that is not inside a string or character literal.
size_t findLineCommentStart(const std::string& text);

// Splits the raw source file into logical statement fragments and records the
// original line text for later source-mapped diagnostics.
std::vector<SourceFragment> splitSourceFragments(
    std::istream& input,
    std::map<int, std::string>& sourceLines,
    const std::string& sourceFile
);
