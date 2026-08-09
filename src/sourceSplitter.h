/*
 * sourceSplitter.h
 *
 * Parser-internal token-to-statement grouping stage.
 *
 * This module does not rescan source text. It groups the canonical file token
 * stream into logical SourceFragment views consumed only by astParser.cpp.
 */

#pragma once

#include "errors.h"
#include "tokenizer.h"

#include <string>
#include <vector>

// Parser-internal logical view of the canonical token stream. SourceFragment
// does not cross the ProgramAst boundary.
struct SourceFragment {
    int lineNumber = 0;
    int startColumn = 1;
    std::string commentText;
    int endLineNumber = 0;
    int endColumn = 1;
    SourceSpan sourceSpan;
    std::vector<Token> tokens;
};

// Groups a canonical whole-file token stream into logical statement views.
std::vector<SourceFragment> splitTokenStream(const TokenStream& tokenStream);
