/*
 * programAst.h
 *
 * Owns the syntax-only, recursive representation of one complete CP++ file.
 * Semantic types and symbol-table decisions intentionally do not live here.
 */

#pragma once

#include "compileContext.h"
#include "exprAst.h"

#include <memory>
#include <string>
#include <vector>

struct ProgramAstNode {
    SourceSpan sourceSpan;
    virtual ~ProgramAstNode() = default;
};

struct TypeSyntax {
    SourceSpan sourceSpan;
    std::string spelling;
    std::string name;
    std::vector<TypeSyntax> arguments;
    std::vector<TypeSyntax> functionParameters;
    bool functionType = false;
};

struct ParameterSyntax {
    SourceSpan sourceSpan;
    TypeSyntax type;
    std::string name;
    bool copyParameter = false;
};

enum class ProgramStatementKind {
    Comment,
    Error,
    VariableDeclaration,
    Assignment,
    Expression,
    Return,
    Break,
    Continue,
    If,
    While,
    For,
    ForEach,
    Rep,
    FunctionDeclaration,
    AggregateDeclaration
};

struct ProgramStatement : ProgramAstNode {
    ProgramStatementKind kind;
    // The compatibility lowering stage consumes this canonical logical source
    // fragment. Syntax classification and block ownership are represented by
    // the concrete node, so later stages never need the original TokenStream.
    SourceFragment fragment;

    explicit ProgramStatement(ProgramStatementKind kind, SourceFragment fragment) :
        kind(kind), fragment(std::move(fragment)) {
        sourceSpan = this->fragment.sourceSpan;
    }
};

struct BlockAst : ProgramAstNode {
    std::vector<std::unique_ptr<ProgramStatement>> statements;
    SourceFragment closingFragment;
    bool hasClosingFragment = false;
};

struct CommentStatementAst : ProgramStatement {
    explicit CommentStatementAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::Comment, std::move(fragment)) {}
};

struct ErrorStatementAst : ProgramStatement {
    std::string reason;
    std::unique_ptr<BlockAst> recoveredBody;

    ErrorStatementAst(SourceFragment fragment, std::string reason) :
        ProgramStatement(ProgramStatementKind::Error, std::move(fragment)),
        reason(std::move(reason)) {}
};

struct VariableDeclarationAst : ProgramStatement {
    TypeSyntax type;
    std::vector<std::string> names;
    std::vector<SourceSpan> nameSpans;
    std::vector<std::unique_ptr<Expr>> initializers;

    explicit VariableDeclarationAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::VariableDeclaration, std::move(fragment)) {}
};

struct AssignmentStatementAst : ProgramStatement {
    std::string operation;
    SourceSpan operationSpan;
    std::vector<std::unique_ptr<Expr>> targets;
    std::vector<std::unique_ptr<Expr>> values;

    explicit AssignmentStatementAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::Assignment, std::move(fragment)) {}
};

struct ExpressionStatementAst : ProgramStatement {
    std::unique_ptr<Expr> expression;

    explicit ExpressionStatementAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::Expression, std::move(fragment)) {}
};

struct ReturnStatementAst : ProgramStatement {
    std::unique_ptr<Expr> value;

    explicit ReturnStatementAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::Return, std::move(fragment)) {}
};

struct SimpleControlStatementAst : ProgramStatement {
    explicit SimpleControlStatementAst(ProgramStatementKind kind, SourceFragment fragment) :
        ProgramStatement(kind, std::move(fragment)) {}
};

struct ConditionalBranchAst : ProgramAstNode {
    SourceFragment headerFragment;
    std::unique_ptr<Expr> condition;
    BlockAst body;
};

struct CompletionBranchAst : ProgramAstNode {
    SourceFragment headerFragment;
    BlockAst body;
};

struct IfStatementAst : ProgramStatement {
    std::unique_ptr<Expr> condition;
    BlockAst thenBody;
    std::vector<ConditionalBranchAst> elseIfBranches;
    std::unique_ptr<CompletionBranchAst> elseBranch;

    explicit IfStatementAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::If, std::move(fragment)) {}
};

struct WhileStatementAst : ProgramStatement {
    std::unique_ptr<Expr> condition;
    BlockAst body;
    std::unique_ptr<CompletionBranchAst> nobreakBranch;

    explicit WhileStatementAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::While, std::move(fragment)) {}
};

enum class ForClauseKind {
    Empty,
    VariableDeclaration,
    Assignment,
    Expression,
    Error
};

struct ForClauseAst : ProgramAstNode {
    ForClauseKind kind = ForClauseKind::Empty;
    TypeSyntax type;
    std::vector<std::string> names;
    std::string operation;
    std::vector<std::unique_ptr<Expr>> expressions;
};

struct ForStatementAst : ProgramStatement {
    ForClauseAst initializer;
    std::unique_ptr<Expr> condition;
    ForClauseAst iteration;
    BlockAst body;
    std::unique_ptr<CompletionBranchAst> nobreakBranch;

    explicit ForStatementAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::For, std::move(fragment)) {}
};

struct ForEachStatementAst : ProgramStatement {
    TypeSyntax variableType;
    std::string variableName;
    SourceSpan variableSpan;
    bool inferredVariable = false;
    std::unique_ptr<Expr> iterable;
    BlockAst body;
    std::unique_ptr<CompletionBranchAst> nobreakBranch;

    explicit ForEachStatementAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::ForEach, std::move(fragment)) {}
};

struct RepStatementAst : ProgramStatement {
    std::unique_ptr<Expr> count;
    BlockAst body;
    std::unique_ptr<CompletionBranchAst> nobreakBranch;

    explicit RepStatementAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::Rep, std::move(fragment)) {}
};

struct FunctionDeclarationAst : ProgramStatement {
    TypeSyntax returnType;
    std::string name;
    SourceSpan nameSpan;
    std::vector<ParameterSyntax> parameters;
    BlockAst body;

    explicit FunctionDeclarationAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::FunctionDeclaration, std::move(fragment)) {}
};

struct AggregateDeclarationAst : ProgramStatement {
    std::string name;
    SourceSpan nameSpan;
    bool isClass = false;
    BlockAst body;

    explicit AggregateDeclarationAst(SourceFragment fragment) :
        ProgramStatement(ProgramStatementKind::AggregateDeclaration, std::move(fragment)) {}
};

struct ProgramAst : ProgramAstNode {
    BlockAst body;
    size_t attributedFragmentCount = 0;
};
