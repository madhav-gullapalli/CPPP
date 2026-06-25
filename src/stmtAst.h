#pragma once

#include "controlFlow.h"

#include <memory>
#include <string>

struct Stmt {
    int sourceColumn = 0;
    virtual ~Stmt() = default;
};

struct EmptyStmt : Stmt {
};

struct RawStmt : Stmt {
    std::string text;

    explicit RawStmt(std::string text, int sourceColumn) : text(std::move(text)) {
        this->sourceColumn = sourceColumn;
    }
};

struct CloseBraceStmt : Stmt {
    std::string trailingText;

    CloseBraceStmt(std::string trailingText, int sourceColumn) : trailingText(std::move(trailingText)) {
        this->sourceColumn = sourceColumn;
    }
};

struct ElseStmt : Stmt {
    explicit ElseStmt(int sourceColumn) {
        this->sourceColumn = sourceColumn;
    }
};

struct ElseIfStmt : Stmt {
    ConditionHeader header;

    ElseIfStmt(ConditionHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};

struct IfStmt : Stmt {
    ConditionHeader header;

    IfStmt(ConditionHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};

struct WhileStmt : Stmt {
    ConditionHeader header;

    WhileStmt(ConditionHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};

struct RepStmt : Stmt {
    ConditionHeader header;

    RepStmt(ConditionHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};

struct ForStmt : Stmt {
    ForHeader header;

    ForStmt(ForHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};

struct ForEachStmt : Stmt {
    ForEachHeader header;

    ForEachStmt(ForEachHeader header, int sourceColumn) : header(std::move(header)) {
        this->sourceColumn = sourceColumn;
    }
};
