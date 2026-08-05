/*
 * stmtAst.h
 *
 * Defines the statement AST node types used by the parser and compiler.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "controlFlow.h"
#include "errors.h"

#include <memory>
#include <string>

// Stmt implements the Stmt behavior for the stmtAst.h module.
struct Stmt {
    int sourceColumn = 0;
    SourceSpan sourceSpan;
    virtual ~Stmt() = default;
};

// EmptyStmt implements the EmptyStmt behavior for the stmtAst.h module.
struct EmptyStmt : Stmt {
};

// RawStmt implements the RawStmt behavior for the stmtAst.h module.
struct RawStmt : Stmt {
    std::string text;

// RawStmt implements the RawStmt behavior for the stmtAst.h module.
    explicit RawStmt(std::string text, int sourceColumn) : text(std::move(text)) {
        this->sourceColumn = sourceColumn;
    }
};

// CloseBraceStmt implements the CloseBraceStmt behavior for the stmtAst.h module.
struct CloseBraceStmt : Stmt {
    std::vector<Token> trailingTokens;

    CloseBraceStmt(std::vector<Token> trailingTokens, int sourceColumn) : trailingTokens(std::move(trailingTokens)) {
        this->sourceColumn = sourceColumn;
    }
};

// ElseStmt implements the ElseStmt behavior for the stmtAst.h module.
struct ElseStmt : Stmt {
// ElseStmt implements the ElseStmt behavior for the stmtAst.h module.
    explicit ElseStmt(int sourceColumn) {
        this->sourceColumn = sourceColumn;
    }
};

// NobreakStmt marks the completion block of a loop that did not break.
struct NobreakStmt : Stmt {
    explicit NobreakStmt(int sourceColumn) {
        this->sourceColumn = sourceColumn;
    }
};

// ElseIfStmt implements the ElseIfStmt behavior for the stmtAst.h module.
struct ElseIfStmt : Stmt {
    ConditionHeader header;

    ElseIfStmt(ConditionHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};

// IfStmt implements the IfStmt behavior for the stmtAst.h module.
struct IfStmt : Stmt {
    ConditionHeader header;

    IfStmt(ConditionHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};

// WhileStmt implements the WhileStmt behavior for the stmtAst.h module.
struct WhileStmt : Stmt {
    ConditionHeader header;

    WhileStmt(ConditionHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};

// RepStmt implements the RepStmt behavior for the stmtAst.h module.
struct RepStmt : Stmt {
    ConditionHeader header;

    RepStmt(ConditionHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};

// ForStmt implements the ForStmt behavior for the stmtAst.h module.
struct ForStmt : Stmt {
    ForHeader header;

    ForStmt(ForHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};

// ForEachStmt implements the ForEachStmt behavior for the stmtAst.h module.
struct ForEachStmt : Stmt {
    ForEachHeader header;

    ForEachStmt(ForEachHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};
