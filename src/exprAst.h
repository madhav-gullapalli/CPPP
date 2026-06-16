#pragma once

#include "expressions.h"

#include <memory>
#include <string>
#include <vector>

struct Expr {
    int sourceColumn = 0;
    Type inferredType = PrimitiveType::Unknown;
    bool mutableValue = false;
    bool explicitCast = false;

    virtual ~Expr() = default;
};

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

struct VariableExpr : Expr {
    std::string name;

    VariableExpr(std::string name, int sourceColumn) : name(std::move(name)) {
        this->sourceColumn = sourceColumn;
    }
};

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

struct IndexExpr : Expr {
    std::unique_ptr<Expr> base;
    std::unique_ptr<Expr> index;

    IndexExpr(std::unique_ptr<Expr> base, std::unique_ptr<Expr> index, int sourceColumn) :
        base(std::move(base)),
        index(std::move(index)) {
        this->sourceColumn = sourceColumn;
    }
};

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

struct ListLiteralExpr : Expr {
    std::vector<std::unique_ptr<Expr>> elements;

    ListLiteralExpr(std::vector<std::unique_ptr<Expr>> elements, int sourceColumn) :
        elements(std::move(elements)) {
        this->sourceColumn = sourceColumn;
    }
};
