/*
 * statementParser.h
 *
 * Declares the statement parser interface and result metadata.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "stmtAst.h"

#include <memory>
#include <string>

// StatementParseResult implements the StatementParseResult behavior for the statementParser.h module.
struct StatementParseResult {
    enum class Kind {
        Empty,
        Raw,
        CloseBrace,
        Else,
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

// parseStatementAst parses statementsast for the compiler pipeline.
StatementParseResult parseStatementAst(const std::string& statement, int sourceColumn = 1);
