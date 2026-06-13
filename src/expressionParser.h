#pragma once

#include "expressions.h"

class ExpressionParser {
public:
    ExpressionParser(
        const std::string& inputFile,
        int lineNumber,
        const std::string& expressionText,
        int expressionColumn,
        const std::map<int, std::string>& sourceLines,
        const std::map<std::string, Type>& declaredVariables,
        bool emitRuntimeChecks
    );

    ExpressionEmitResult parse();

private:
    struct ParsedExpression {
        bool ok;
        std::string text;
        Type type;
        bool explicitCast;
        int sourceColumn;
        bool mutableValue;
    };

    const std::string& inputFile;
    int lineNumber;
    const std::string& expressionText;
    int expressionColumn;
    const std::map<int, std::string>& sourceLines;
    const std::map<std::string, Type>& declaredVariables;
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

    ParsedExpression parseExpression();
    ParsedExpression parseLogicalOr();
    ParsedExpression parseLogicalAnd();
    ParsedExpression parseBitwiseOr();
    ParsedExpression parseBitwiseXor();
    ParsedExpression parseBitwiseAnd();
    ParsedExpression parseEquality();
    ParsedExpression parseComparison();
    ParsedExpression parseShift();
    ParsedExpression parseAdditive();
    ParsedExpression parseMultiplicative();
    ParsedExpression parseUnary();
    ParsedExpression parsePostfix();
    ParsedExpression parsePrimary();
    ParsedExpression parseListMethodCall(ParsedExpression expression);

    bool isTypeName(const std::string& name) const;
    Type binaryResultType(Type left, Type right, const std::string& op) const;
    bool isValueType(Type type) const;
    bool isNumericType(Type type) const;
    bool isBitwiseType(Type type) const;
    bool isIncrementableType(Type type) const;
    bool isComparable(Type left, Type right) const;
    bool isFloatType(Type type) const;
    std::string runtimeErrorThrowExpression(int column, const std::string& message) const;
    std::string checkedIntegerExpression(
        const std::string& left,
        const std::string& right,
        const std::string& op,
        int column
    ) const;
};
