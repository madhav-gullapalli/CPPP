#pragma once

#include "stmtAst.h"

#include <memory>
#include <string>

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

StatementParseResult parseStatementAst(const std::string& statement, int sourceColumn = 1);
