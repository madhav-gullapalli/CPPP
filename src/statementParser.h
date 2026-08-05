/*
 * statementParser.h
 *
 * Declares the statement parser interface and result metadata.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "stmtAst.h"
#include "tokenizer.h"

#include <memory>
#include <string>
#include <vector>

// StatementParseResult implements the StatementParseResult behavior for the statementParser.h module.
struct StatementParseResult {
    enum class Kind {
        Empty,
        Raw,
        CloseBrace,
        Else,
        Nobreak,
        ElseIf,
        If,
        While,
        Rep,
        For,
        ForEach
    };

    Kind kind = Kind::Raw;
    bool ok = true;
    size_t errorOffset = 0;
    std::string message;
    std::unique_ptr<Stmt> statement;
};

// Parses a logical statement from the canonical file token stream. The token
// vector must end with EndOfFile, as SourceFragment::tokens does.
StatementParseResult parseStatementAst(const std::vector<Token>& tokens, int sourceColumn = 1);
