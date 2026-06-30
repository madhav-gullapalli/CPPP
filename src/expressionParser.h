#pragma once

#include "exprAst.h"
#include "expressions.h"
#include "functions.h"

class ExpressionParser {
public:
    ExpressionParser(
        const std::string& inputFile,
        int lineNumber,
        const std::string& expressionText,
        int expressionColumn,
        const std::map<int, std::string>& sourceLines,
        const std::map<std::string, Type>& declaredVariables,
        const std::map<std::string, FunctionSignature>& declaredFunctions,
        bool emitRuntimeChecks
    );

    ExpressionEmitResult parse();
    std::unique_ptr<Expr> parseAst(bool& ok);

private:
    const std::string& inputFile;
    int lineNumber;
    const std::string& expressionText;
    int expressionColumn;
    const std::map<int, std::string>& sourceLines;
    const std::map<std::string, Type>& declaredVariables;
    const std::map<std::string, FunctionSignature>& declaredFunctions;
    bool emitRuntimeChecks;
    std::vector<Token> tokens;
    size_t current = 0;

    bool atEnd() const;
    const Token& peek() const;
    const Token& previous() const;
    bool match(TokenKind kind, const std::string& text = "");
    bool check(TokenKind kind, const std::string& text = "") const;
    bool isOperator(const std::string& text) const;
    bool isUnterminatedQuotedToken(const Token& token) const;
    int absoluteColumn(const Token& token) const;
    void report(const Token& token, const std::string& message) const;
    bool reportInputUsageError(const Token& inputToken) const;

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
    std::unique_ptr<Expr> parseListMethodCall(std::unique_ptr<Expr> expression, bool& ok);
};
