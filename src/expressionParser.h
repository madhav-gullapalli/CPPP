/*
 * expressionParser.h
 *
 * Declares the expression parser interface used by the compiler pipeline.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "exprAst.h"
#include "expressions.h"
#include "functions.h"

// ExpressionParser holds state or behavior used by the expressionParser.h implementation.
class ExpressionParser {
public:
    ExpressionParser(
        const std::string& inputFile,
        int lineNumber,
        const std::vector<Token>& expressionTokens,
        int expressionColumn,
        const std::map<int, std::string>& sourceLines,
        const std::map<std::string, Type>& declaredVariables,
        const std::map<std::string, FunctionSignature>& declaredFunctions,
        bool emitRuntimeChecks
    );

// parse parses  for the compiler pipeline.
    ExpressionEmitResult parse();
    std::unique_ptr<Expr> parseAst(bool& ok);

private:
    const std::string& inputFile;
    int lineNumber;
    int expressionColumn;
    const std::map<int, std::string>& sourceLines;
    const std::map<std::string, Type>& declaredVariables;
    const std::map<std::string, FunctionSignature>& declaredFunctions;
    bool emitRuntimeChecks;
    std::vector<Token> tokens;
    size_t current = 0;

// atEnd implements the atEnd behavior for the expressionParser.h module.
    bool atEnd() const;
    const Token& peek() const;
    const Token& previous() const;
// match implements the match behavior for the expressionParser.h module.
    bool match(TokenKind kind, const std::string& text = "");
// check implements the check behavior for the expressionParser.h module.
    bool check(TokenKind kind, const std::string& text = "") const;
// isOperator returns whether the supplied input satisfies the relevant condition.
    bool isOperator(const std::string& text) const;
// isUnterminatedQuotedToken returns whether the supplied input satisfies the relevant condition.
    bool isUnterminatedQuotedToken(const Token& token) const;
// absoluteColumn implements the absoluteColumn behavior for the expressionParser.h module.
    int absoluteColumn(const Token& token) const;
    int absoluteEndColumn(const Token& token) const;
    void report(const Token& token, const std::string& message) const;
    void reportUnexpectedTrailingToken(const Token& token) const;
// reportInputUsageError implements the reportInputUsageError behavior for the expressionParser.h module.
    bool reportInputUsageError(const Token& inputToken) const;

// isTypeName returns whether the supplied input satisfies the relevant condition.
    bool isTypeName(const std::string& name) const;
    std::unique_ptr<Expr> parseExpression(bool& ok);
    std::unique_ptr<Expr> parseLogicalOr(bool& ok);
    std::unique_ptr<Expr> parseLogicalAnd(bool& ok);
    std::unique_ptr<Expr> parseBitwiseOr(bool& ok);
    std::unique_ptr<Expr> parseBitwiseXor(bool& ok);
    std::unique_ptr<Expr> parseBitwiseAnd(bool& ok);
    std::unique_ptr<Expr> parseEquality(bool& ok);
    std::unique_ptr<Expr> parseComparison(bool& ok);
    std::unique_ptr<Expr> parseShift(bool& ok);
    std::unique_ptr<Expr> parseAdditive(bool& ok);
    std::unique_ptr<Expr> parseMultiplicative(bool& ok);
    std::unique_ptr<Expr> parseUnary(bool& ok);
    std::unique_ptr<Expr> parsePostfix(bool& ok);
    std::unique_ptr<Expr> parsePrimary(bool& ok);
    std::unique_ptr<Expr> parseMethodCall(std::unique_ptr<Expr> expression, bool& ok);
    std::unique_ptr<Expr> parseBraceLiteral(bool& ok);
};
