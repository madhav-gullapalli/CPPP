#include "assignmentCppp.h"

#include "expressions.h"
#include "tokenizer.h"

namespace {
bool isCompoundAssignment(const std::vector<Token>& tokens) {
    return tokens.size() >= 4 &&
        tokens[1].kind == TokenKind::Operator &&
        (tokens[1].text == "+" || tokens[1].text == "-" || tokens[1].text == "*" || tokens[1].text == "/" || tokens[1].text == "%") &&
        tokens[2].kind == TokenKind::Equals;
}
}

AssignmentEmitResult emitAssignmentStatement(
    const std::string& inputFile,
    int lineNumber,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::set<std::string>& declaredVariables
) {
    const std::vector<Token> tokens = tokenize(statementBody);
    if (tokens.size() < 2 || tokens[0].kind != TokenKind::Identifier) {
        return {false, true, "", {}};
    }

    const bool simpleAssignment = tokens[1].kind == TokenKind::Equals;
    const bool compoundAssignment = isCompoundAssignment(tokens);
    if (!simpleAssignment && !compoundAssignment) {
        return {false, true, "", {}};
    }

    const std::string variableName = tokens[0].text;
    if (declaredVariables.count(variableName) == 0) {
        recordSourceError(inputFile, lineNumber, tokens[0].span.startColumn, "use of undeclared variable '" + variableName + "'", sourceLines);
        return {true, false, "", {}};
    }

    const size_t expressionTokenIndex = simpleAssignment ? 2 : 3;
    if (tokens[expressionTokenIndex].kind == TokenKind::EndOfFile) {
        recordSourceError(inputFile, lineNumber, tokens[expressionTokenIndex - 1].span.endColumn + 1, "expected expression after assignment", sourceLines);
        return {true, false, "", {}};
    }

    const int expressionStartColumn = tokens[expressionTokenIndex].span.startColumn;
    int expressionEndColumn = tokens[expressionTokenIndex].span.endColumn;
    size_t tokenIndex = expressionTokenIndex;
    while (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
        expressionEndColumn = tokens[tokenIndex].span.endColumn;
        ++tokenIndex;
    }

    const std::string expressionText = statementBody.substr(
        static_cast<size_t>(expressionStartColumn - 1),
        static_cast<size_t>(expressionEndColumn - expressionStartColumn + 1)
    );
    const ExpressionEmitResult expression = emitExpression(
        inputFile,
        lineNumber,
        expressionText,
        expressionStartColumn,
        sourceLines,
        declaredVariables
    );
    if (!expression.ok) {
        return {true, false, "", {}};
    }

    const std::string assignmentOperator = simpleAssignment ? "=" : tokens[1].text + "=";
    const std::string generatedStatement = "    " + variableName + " " + assignmentOperator + " " + expression.generatedExpression + ";";
    return {
        true,
        true,
        generatedStatement,
        {{
            lineNumber,
            tokens[0].span.startColumn,
            5,
            5 + static_cast<int>(variableName.size()) - 1
        }}
    };
}
