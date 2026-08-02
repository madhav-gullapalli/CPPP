/*
 * exprAst.h
 *
 * Declares the expression AST node hierarchy for parsed CP++ expressions.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "expressions.h"

#include <memory>
#include <string>
#include <vector>

// Expr implements the Expr behavior for the exprAst.h module.
struct Expr {
    int sourceColumn = 0;
    SourceSpan sourceSpan;
    Type inferredType = PrimitiveType::Unknown;
    bool mutableValue = false;
    bool explicitCast = false;

    virtual ~Expr() = default;
};

// LiteralExpr implements the LiteralExpr behavior for the exprAst.h module.
struct LiteralExpr : Expr {
    enum class Kind {
        Bool,
        Null,
        Int,
        Float,
        String,
        Char
    };

    Kind kind;
    std::string text;

    LiteralExpr(Kind kind, std::string text, int sourceColumn, SourceSpan sourceSpan = {}) :
        kind(kind),
        text(std::move(text)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};

// VariableExpr implements the VariableExpr behavior for the exprAst.h module.
struct VariableExpr : Expr {
    std::string name;

    VariableExpr(std::string name, int sourceColumn, SourceSpan sourceSpan = {}) : name(std::move(name)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};

// FieldExpr accesses a named public field on a CP++ struct value.
struct FieldExpr : Expr {
    std::unique_ptr<Expr> base;
    std::string field;

    FieldExpr(std::unique_ptr<Expr> base, std::string field, int sourceColumn, SourceSpan sourceSpan = {}) :
        base(std::move(base)), field(std::move(field)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};

// UnaryExpr implements the UnaryExpr behavior for the exprAst.h module.
struct UnaryExpr : Expr {
    std::string op;
    std::unique_ptr<Expr> operand;
    bool postfix = false;

    UnaryExpr(
        std::string op,
        std::unique_ptr<Expr> operand,
        int sourceColumn,
        bool postfix = false,
        SourceSpan sourceSpan = {}
    ) :
        op(std::move(op)),
        operand(std::move(operand)),
        postfix(postfix) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};

// BinaryExpr implements the BinaryExpr behavior for the exprAst.h module.
struct BinaryExpr : Expr {
    std::string op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    BinaryExpr(
        std::string op,
        std::unique_ptr<Expr> left,
        std::unique_ptr<Expr> right,
        int sourceColumn,
        SourceSpan sourceSpan = {}
    ) :
        op(std::move(op)),
        left(std::move(left)),
        right(std::move(right)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};

// CastExpr implements the CastExpr behavior for the exprAst.h module.
struct CastExpr : Expr {
    Type targetType;
    std::unique_ptr<Expr> operand;

    CastExpr(Type targetType, std::unique_ptr<Expr> operand, int sourceColumn, SourceSpan sourceSpan = {}) :
        targetType(std::move(targetType)),
        operand(std::move(operand)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
        explicitCast = true;
    }
};

// CallExpr implements the CallExpr behavior for the exprAst.h module.
struct CallExpr : Expr {
    std::string callee;
    std::unique_ptr<Expr> receiver;
    std::vector<std::unique_ptr<Expr>> arguments;
    bool partialApplication = false;

    CallExpr(
        std::string callee,
        std::unique_ptr<Expr> receiver,
        std::vector<std::unique_ptr<Expr>> arguments,
        int sourceColumn,
        SourceSpan sourceSpan = {}
    ) :
        callee(std::move(callee)),
        receiver(std::move(receiver)),
        arguments(std::move(arguments)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};

// IndexExpr implements the IndexExpr behavior for the exprAst.h module.
struct IndexExpr : Expr {
    std::unique_ptr<Expr> base;
    std::unique_ptr<Expr> index;

    IndexExpr(
        std::unique_ptr<Expr> base,
        std::unique_ptr<Expr> index,
        int sourceColumn,
        SourceSpan sourceSpan = {}
    ) :
        base(std::move(base)),
        index(std::move(index)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};

// SliceExpr implements the SliceExpr behavior for the exprAst.h module.
struct SliceExpr : Expr {
    std::unique_ptr<Expr> base;
    std::unique_ptr<Expr> start;
    std::unique_ptr<Expr> end;

    SliceExpr(
        std::unique_ptr<Expr> base,
        std::unique_ptr<Expr> start,
        std::unique_ptr<Expr> end,
        int sourceColumn,
        SourceSpan sourceSpan = {}
    ) :
        base(std::move(base)),
        start(std::move(start)),
        end(std::move(end)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};

// ListLiteralExpr handles list-specific behavior for the compiler or runtime.
struct ListLiteralExpr : Expr {
    std::vector<std::unique_ptr<Expr>> elements;

    ListLiteralExpr(
        std::vector<std::unique_ptr<Expr>> elements,
        int sourceColumn,
        SourceSpan sourceSpan = {}
    ) :
        elements(std::move(elements)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};

// SetLiteralExpr handles set-specific behavior for the compiler or runtime.
struct SetLiteralExpr : Expr {
    std::vector<std::unique_ptr<Expr>> elements;

    SetLiteralExpr(
        std::vector<std::unique_ptr<Expr>> elements,
        int sourceColumn,
        SourceSpan sourceSpan = {}
    ) :
        elements(std::move(elements)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};

// MapLiteralEntry holds one parsed key/value pair from a Map literal.
struct MapLiteralEntry {
    std::unique_ptr<Expr> key;
    std::unique_ptr<Expr> value;
};

// MapLiteralExpr handles map-specific behavior for the compiler or runtime.
struct MapLiteralExpr : Expr {
    std::vector<MapLiteralEntry> entries;

    MapLiteralExpr(
        std::vector<MapLiteralEntry> entries,
        int sourceColumn,
        SourceSpan sourceSpan = {}
    ) :
        entries(std::move(entries)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};

// PairLiteralExpr handles pair-specific behavior for the compiler or runtime.
struct PairLiteralExpr : Expr {
    std::unique_ptr<Expr> first;
    std::unique_ptr<Expr> second;

    PairLiteralExpr(
        std::unique_ptr<Expr> first,
        std::unique_ptr<Expr> second,
        int sourceColumn,
        SourceSpan sourceSpan = {}
    ) :
        first(std::move(first)),
        second(std::move(second)) {
        this->sourceColumn = sourceColumn;
        this->sourceSpan = sourceSpan;
    }
};
