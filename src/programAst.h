/*
 * programAst.h
 *
 * Owns the syntax-only, recursive representation of one complete CP++ file.
 * Semantic types and parser-internal SourceFragments intentionally do not live
 * in this tree.
 */

#pragma once

#include "exprAst.h"
#include "tokenizer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

struct ProgramAstNode {
    SourceSpan sourceSpan;
    virtual ~ProgramAstNode() = default;
};

struct SyntaxSite {
    SourceSpan sourceSpan;
    int lineNumber = 0;
    int startColumn = 1;
    int endLineNumber = 0;
    int endColumn = 1;
    std::string commentText;
};

// StatementSyntax retains tokens only for precise diagnostics and the
// expression-level compatibility emitters. Statement identity and block
// structure are represented exclusively by concrete AST nodes.
struct StatementSyntax : SyntaxSite {
    std::vector<Token> tokens;
    bool terminated = false;
    bool opensBlock = false;
    size_t codeLength = 0;
};

struct TypeSyntax {
    SourceSpan sourceSpan;
    SourceSpan nameSpan;
    SourceSpan errorSpan;
    std::string spelling;
    std::string name;
    std::string syntaxError;
    std::vector<TypeSyntax> arguments;
    std::vector<TypeSyntax> functionParameters;
    std::vector<bool> functionParameterCopy;
    bool functionType = false;
    bool syntaxOk = true;
};

struct ParameterSyntax {
    SourceSpan sourceSpan;
    TypeSyntax type;
    std::string name;
    bool copyParameter = false;
    std::string modifier;
    SourceSpan modifierSpan;
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
    StatementSyntax syntax;
    bool syntaxOk = true;
    size_t syntaxErrorOffset = 0;
    std::string syntaxError;

    explicit ProgramStatement(ProgramStatementKind kind, StatementSyntax syntax) :
        kind(kind), syntax(std::move(syntax)) {
        sourceSpan = this->syntax.sourceSpan;
    }
};

struct BlockAst : ProgramAstNode {
    std::vector<std::unique_ptr<ProgramStatement>> statements;
    SyntaxSite closingSyntax;
    bool hasClosingSyntax = false;
};

struct CommentStatementAst : ProgramStatement {
    explicit CommentStatementAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::Comment, std::move(syntax)) {}
};

struct ErrorStatementAst : ProgramStatement {
    std::string reason;
    std::unique_ptr<BlockAst> recoveredBody;

    ErrorStatementAst(StatementSyntax syntax, std::string reason) :
        ProgramStatement(ProgramStatementKind::Error, std::move(syntax)),
        reason(std::move(reason)) {}
};

struct VariableDeclarationAst : ProgramStatement {
    TypeSyntax type;
    bool inferredType = false;
    std::vector<std::string> names;
    std::vector<SourceSpan> nameSpans;
    std::vector<std::unique_ptr<Expr>> initializers;
    size_t continuationTokenIndex = 0;

    explicit VariableDeclarationAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::VariableDeclaration, std::move(syntax)) {}
};

struct AssignmentStatementAst : ProgramStatement {
    std::string operation;
    SourceSpan operationSpan;
    Token operationToken;
    std::vector<std::unique_ptr<Expr>> targets;
    std::vector<std::unique_ptr<Expr>> values;
    std::vector<std::vector<Token>> targetTokens;
    std::vector<std::vector<Token>> valueTokens;
    std::vector<size_t> targetOffsets;
    std::vector<size_t> valueOffsets;

    explicit AssignmentStatementAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::Assignment, std::move(syntax)) {}
};

struct ExpressionStatementAst : ProgramStatement {
    std::unique_ptr<Expr> expression;

    explicit ExpressionStatementAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::Expression, std::move(syntax)) {}
};

struct ReturnStatementAst : ProgramStatement {
    std::unique_ptr<Expr> value;
    std::vector<Token> valueTokens;
    size_t valueOffset = 0;

    explicit ReturnStatementAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::Return, std::move(syntax)) {}
};

struct SimpleControlStatementAst : ProgramStatement {
    explicit SimpleControlStatementAst(ProgramStatementKind kind, StatementSyntax syntax) :
        ProgramStatement(kind, std::move(syntax)) {}
};

struct ConditionalBranchAst : ProgramAstNode {
    SyntaxSite headerSyntax;
    std::unique_ptr<Expr> condition;
    std::vector<Token> conditionTokens;
    size_t conditionOffset = 0;
    bool syntaxOk = true;
    size_t syntaxErrorOffset = 0;
    std::string syntaxError;
    BlockAst body;
};

struct CompletionBranchAst : ProgramAstNode {
    SyntaxSite headerSyntax;
    BlockAst body;
};

struct IfStatementAst : ProgramStatement {
    std::unique_ptr<Expr> condition;
    std::vector<Token> conditionTokens;
    size_t conditionOffset = 0;
    BlockAst thenBody;
    std::vector<ConditionalBranchAst> elseIfBranches;
    std::unique_ptr<CompletionBranchAst> elseBranch;

    explicit IfStatementAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::If, std::move(syntax)) {}
};

struct WhileStatementAst : ProgramStatement {
    std::unique_ptr<Expr> condition;
    std::vector<Token> conditionTokens;
    size_t conditionOffset = 0;
    BlockAst body;
    std::unique_ptr<CompletionBranchAst> nobreakBranch;

    explicit WhileStatementAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::While, std::move(syntax)) {}
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
    bool inferredType = false;
    std::vector<std::string> names;
    std::vector<SourceSpan> nameSpans;
    size_t continuationTokenIndex = 0;
    std::string operation;
    Token operationToken;
    std::vector<std::unique_ptr<Expr>> expressions;
    std::vector<std::vector<Token>> targetTokens;
    std::vector<std::vector<Token>> valueTokens;
    std::vector<size_t> targetOffsets;
    std::vector<size_t> valueOffsets;
    std::vector<Token> tokens;
    size_t offset = 0;
};

struct ForStatementAst : ProgramStatement {
    ForClauseAst initializer;
    std::unique_ptr<Expr> condition;
    std::vector<Token> conditionTokens;
    size_t conditionOffset = 0;
    ForClauseAst iteration;
    BlockAst body;
    std::unique_ptr<CompletionBranchAst> nobreakBranch;

    explicit ForStatementAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::For, std::move(syntax)) {}
};

struct ForEachStatementAst : ProgramStatement {
    TypeSyntax variableType;
    std::string variableName;
    bool inferredVariable = false;
    std::unique_ptr<Expr> iterable;
    std::vector<Token> iterableTokens;
    size_t variableOffset = 0;
    size_t iterableOffset = 0;
    BlockAst body;
    std::unique_ptr<CompletionBranchAst> nobreakBranch;

    explicit ForEachStatementAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::ForEach, std::move(syntax)) {}
};

struct RepStatementAst : ProgramStatement {
    std::unique_ptr<Expr> count;
    std::vector<Token> countTokens;
    size_t countOffset = 0;
    BlockAst body;
    std::unique_ptr<CompletionBranchAst> nobreakBranch;

    explicit RepStatementAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::Rep, std::move(syntax)) {}
};

struct FunctionDeclarationAst : ProgramStatement {
    TypeSyntax returnType;
    std::string name;
    SourceSpan nameSpan;
    std::vector<ParameterSyntax> parameters;
    BlockAst body;

    explicit FunctionDeclarationAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::FunctionDeclaration, std::move(syntax)) {}
};

struct AggregateDeclarationAst : ProgramStatement {
    std::string name;
    SourceSpan nameSpan;
    bool isClass = false;
    BlockAst body;

    explicit AggregateDeclarationAst(StatementSyntax syntax) :
        ProgramStatement(ProgramStatementKind::AggregateDeclaration, std::move(syntax)) {}
};

struct ProgramAst : ProgramAstNode {
    BlockAst body;
};
