#include "assignmentCppp.h"

#include "expressions.h"
#include "tokenizer.h"

namespace {
struct AssignmentTarget {
    std::string name;
    int column;
};

bool isCompoundAssignment(const std::vector<Token>& tokens) {
    return tokens.size() >= 4 &&
        tokens[1].kind == TokenKind::Operator &&
        (tokens[1].text == "+" || tokens[1].text == "-" || tokens[1].text == "*" || tokens[1].text == "/" || tokens[1].text == "%") &&
        tokens[2].kind == TokenKind::Equals;
}

bool parseAssignmentTargets(
    const std::vector<Token>& tokens,
    std::vector<AssignmentTarget>& targets,
    size_t& equalsIndex
) {
    size_t tokenIndex = 0;
    while (tokens[tokenIndex].kind == TokenKind::Identifier) {
        targets.push_back({tokens[tokenIndex].text, tokens[tokenIndex].span.startColumn});
        ++tokenIndex;

        if (tokens[tokenIndex].kind == TokenKind::Equals) {
            equalsIndex = tokenIndex;
            return true;
        }

        if (tokens[tokenIndex].kind != TokenKind::Comma) {
            return false;
        }

        ++tokenIndex;
    }

    return false;
}
}

AssignmentEmitResult emitAssignmentStatement(
    const std::string& inputFile,
    int lineNumber,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, CpppType>& declaredVariables
) {
    const std::vector<Token> tokens = tokenize(statementBody);
    if (tokens.size() < 2 || tokens[0].kind != TokenKind::Identifier) {
        return {false, true, "", {}};
    }

    std::vector<AssignmentTarget> assignmentTargets;
    size_t equalsIndex = 0;
    if (parseAssignmentTargets(tokens, assignmentTargets, equalsIndex) && assignmentTargets.size() > 1) {
        const size_t expressionTokenIndex = equalsIndex + 1;
        if (tokens[expressionTokenIndex].kind == TokenKind::EndOfFile) {
            recordSourceError(inputFile, lineNumber, tokens[equalsIndex].span.endColumn + 1, "expected expression after assignment", sourceLines);
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
        const std::vector<Token> expressionTokens = tokenize(expressionText);
        if (!isInputCall(expressionTokens)) {
            if (expressionTokens.size() > 1 &&
                expressionTokens[0].kind == TokenKind::Identifier &&
                expressionTokens[0].text == "input") {
                emitExpression(inputFile, lineNumber, expressionText, expressionStartColumn, sourceLines, declaredVariables);
                return {true, false, "", {}};
            }

            recordSourceError(inputFile, lineNumber, expressionStartColumn, "multi-assignment requires input()", sourceLines);
            return {true, false, "", {}};
        }

        std::string generatedStatement = "    ";
        std::vector<SourceRange> ranges;
        for (size_t i = 0; i < assignmentTargets.size(); ++i) {
            const auto variable = declaredVariables.find(assignmentTargets[i].name);
            if (variable == declaredVariables.end()) {
                recordSourceError(inputFile, lineNumber, assignmentTargets[i].column, "use of undeclared variable '" + assignmentTargets[i].name + "'", sourceLines);
                return {true, false, "", {}};
            }

            if (i > 0) {
                generatedStatement += " ";
            }

            const int generatedStartColumn = static_cast<int>(generatedStatement.size()) + 1;
            generatedStatement += assignmentTargets[i].name + " = " + inputFunctionForType(variable->second) + ";";
            ranges.push_back({
                lineNumber,
                assignmentTargets[i].column,
                generatedStartColumn,
                generatedStartColumn + static_cast<int>(assignmentTargets[i].name.size()) - 1
            });
        }

        return {true, true, generatedStatement, ranges};
    }

    const bool simpleAssignment = tokens[1].kind == TokenKind::Equals;
    const bool compoundAssignment = isCompoundAssignment(tokens);
    if (!simpleAssignment && !compoundAssignment) {
        return {false, true, "", {}};
    }

    const std::string variableName = tokens[0].text;
    const auto variable = declaredVariables.find(variableName);
    if (variable == declaredVariables.end()) {
        recordSourceError(inputFile, lineNumber, tokens[0].span.startColumn, "use of undeclared variable '" + variableName + "'", sourceLines);
        return {true, false, "", {}};
    }
    const CpppType targetType = variable->second;

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
    const std::vector<Token> expressionTokens = tokenize(expressionText);
    if (isInputCall(expressionTokens)) {
        const std::string generatedStatement = "    " + variableName + " = " + inputFunctionForType(targetType) + ";";
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

    if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, targetType)) {
        recordSourceError(
            inputFile,
            lineNumber,
            expressionStartColumn,
            "cannot implicitly convert " + cpppTypeName(expression.type) + " to " + cpppTypeName(targetType),
            sourceLines
        );
        return {true, false, "", {}};
    }

    std::string emittedExpression = expression.generatedExpression;
    if (!isImplicitlyConvertible(expression.type, targetType) || expression.type != targetType) {
        emittedExpression = castExpressionTo(emittedExpression, targetType);
    }

    const std::string assignmentOperator = simpleAssignment ? "=" : tokens[1].text + "=";
    const std::string generatedStatement = "    " + variableName + " " + assignmentOperator + " " + emittedExpression + ";";
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
