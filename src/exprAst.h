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
    Type inferredType = PrimitiveType::Unknown;
    bool mutableValue = false;
    bool explicitCast = false;

    virtual ~Expr() = default;
};

// LiteralExpr implements the LiteralExpr behavior for the exprAst.h module.
struct LiteralExpr : Expr {
    enum class Kind {
        Bool,
        Int,
        Float,
        String,
        Char
    };

    Kind kind;
    std::string text;

    LiteralExpr(Kind kind, std::string text, int sourceColumn) :
        kind(kind),
        text(std::move(text)) {
        this->sourceColumn = sourceColumn;
    }
};

// VariableExpr implements the VariableExpr behavior for the exprAst.h module.
struct VariableExpr : Expr {
    std::string name;

    VariableExpr(std::string name, int sourceColumn) : name(std::move(name)) {
        this->sourceColumn = sourceColumn;
    }
};

// UnaryExpr implements the UnaryExpr behavior for the exprAst.h module.
struct UnaryExpr : Expr {
    std::string op;
    std::unique_ptr<Expr> operand;
    bool postfix = false;

    UnaryExpr(std::string op, std::unique_ptr<Expr> operand, int sourceColumn, bool postfix = false) :
        op(std::move(op)),
        operand(std::move(operand)),
        postfix(postfix) {
        this->sourceColumn = sourceColumn;
    }
};

// BinaryExpr implements the BinaryExpr behavior for the exprAst.h module.
struct BinaryExpr : Expr {
    std::string op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    BinaryExpr(std::string op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right, int sourceColumn) :
        op(std::move(op)),
        left(std::move(left)),
        right(std::move(right)) {
        this->sourceColumn = sourceColumn;
    }
};

// CastExpr implements the CastExpr behavior for the exprAst.h module.
struct CastExpr : Expr {
    Type targetType;
    std::unique_ptr<Expr> operand;

    CastExpr(Type targetType, std::unique_ptr<Expr> operand, int sourceColumn) :
        targetType(std::move(targetType)),
        operand(std::move(operand)) {
        this->sourceColumn = sourceColumn;
        explicitCast = true;
    }
};

// CallExpr implements the CallExpr behavior for the exprAst.h module.
struct CallExpr : Expr {
    std::string callee;
    std::unique_ptr<Expr> receiver;
    std::vector<std::unique_ptr<Expr>> arguments;

    CallExpr(
        std::string callee,
        std::unique_ptr<Expr> receiver,
        std::vector<std::unique_ptr<Expr>> arguments,
        int sourceColumn
    ) :
        callee(std::move(callee)),
        receiver(std::move(receiver)),
        arguments(std::move(arguments)) {
        this->sourceColumn = sourceColumn;
    }
};

// IndexExpr implements the IndexExpr behavior for the exprAst.h module.
struct IndexExpr : Expr {
    std::unique_ptr<Expr> base;
    std::unique_ptr<Expr> index;

    IndexExpr(std::unique_ptr<Expr> base, std::unique_ptr<Expr> index, int sourceColumn) :
        base(std::move(base)),
        index(std::move(index)) {
        this->sourceColumn = sourceColumn;
    }
};

// SliceExpr implements the SliceExpr behavior for the exprAst.h module.
struct SliceExpr : Expr {
    std::unique_ptr<Expr> base;
    std::unique_ptr<Expr> start;
    std::unique_ptr<Expr> end;

    SliceExpr(std::unique_ptr<Expr> base, std::unique_ptr<Expr> start, std::unique_ptr<Expr> end, int sourceColumn) :
        base(std::move(base)),
        start(std::move(start)),
        end(std::move(end)) {
        this->sourceColumn = sourceColumn;
    }
};

// ListLiteralExpr handles list-specific behavior for the compiler or runtime.
struct ListLiteralExpr : Expr {
    std::vector<std::unique_ptr<Expr>> elements;

    ListLiteralExpr(std::vector<std::unique_ptr<Expr>> elements, int sourceColumn) :
        elements(std::move(elements)) {
        this->sourceColumn = sourceColumn;
    }
};
