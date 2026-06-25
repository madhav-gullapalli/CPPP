#include "assignmentCppp.h"

#include "expressions.h"
#include "tokenizer.h"
#include "typesCppp.h"

namespace {
struct IndexedAssignmentTarget {
    std::string name;
    int nameColumn = 0;
    std::string indexText;
    int indexColumn = 0;
};

struct AssignmentTarget {
    bool indexed = false;
    std::string name;
    int column = 0;
    std::string indexText;
    int indexColumn = 0;
};

bool isCompoundAssignment(const std::vector<Token>& tokens) {
    return tokens.size() >= 3 &&
        tokens[1].kind == TokenKind::Operator &&
        (tokens[1].text == "+=" || tokens[1].text == "-=" || tokens[1].text == "*=" ||
         tokens[1].text == "/=" || tokens[1].text == "%=" || tokens[1].text == "<<=" ||
         tokens[1].text == ">>=" || tokens[1].text == "&=" || tokens[1].text == "|=" ||
         tokens[1].text == "^=" || tokens[1].text == "&&=" || tokens[1].text == "||=");
}

std::string compoundOperator(const std::string& assignmentOperator) {
    return assignmentOperator.substr(0, assignmentOperator.size() - 1);
}

bool parseAssignmentTargets(
    const std::vector<Token>& tokens,
    const std::string& statementBody,
    int statementColumn,
    std::vector<AssignmentTarget>& targets,
    size_t& equalsIndex
) {
    size_t tokenIndex = 0;
    while (tokenIndex < tokens.size() && tokens[tokenIndex].kind == TokenKind::Identifier) {
        AssignmentTarget target;
        target.name = tokens[tokenIndex].text;
        target.column = statementColumn + tokens[tokenIndex].span.startColumn - 1;
        ++tokenIndex;

        if (tokenIndex < tokens.size() && tokens[tokenIndex].kind == TokenKind::LeftBracket) {
            int bracketDepth = 0;
            const size_t leftBracketIndex = tokenIndex;
            size_t rightBracketIndex = 0;
            for (; tokenIndex < tokens.size(); ++tokenIndex) {
                if (tokens[tokenIndex].kind == TokenKind::LeftBracket) {
                    ++bracketDepth;
                } else if (tokens[tokenIndex].kind == TokenKind::RightBracket) {
                    --bracketDepth;
                    if (bracketDepth == 0) {
                        rightBracketIndex = tokenIndex;
                        break;
                    }
                }
            }

            if (rightBracketIndex == 0) {
                return false;
            }

            const size_t indexStart = static_cast<size_t>(tokens[leftBracketIndex].span.endColumn);
            const size_t indexLength = static_cast<size_t>(tokens[rightBracketIndex].span.startColumn - tokens[leftBracketIndex].span.endColumn - 1);
            target.indexed = true;
            target.indexText = statementBody.substr(indexStart, indexLength);
            target.indexColumn = statementColumn + tokens[leftBracketIndex + 1].span.startColumn - 1;
            tokenIndex = rightBracketIndex + 1;
        }

        targets.push_back(target);

        if (tokenIndex >= tokens.size()) {
            return false;
        }
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

bool parseIndexedAssignmentTarget(
    const std::vector<Token>& tokens,
    const std::string& statementBody,
    int statementColumn,
    IndexedAssignmentTarget& target,
    size_t& equalsIndex
) {
    if (tokens.size() < 5 ||
        tokens[0].kind != TokenKind::Identifier ||
        tokens[1].kind != TokenKind::LeftBracket) {
        return false;
    }

    int bracketDepth = 0;
    size_t tokenIndex = 1;
    size_t rightBracketIndex = 0;
    for (; tokenIndex < tokens.size(); ++tokenIndex) {
        if (tokens[tokenIndex].kind == TokenKind::LeftBracket) {
            ++bracketDepth;
        } else if (tokens[tokenIndex].kind == TokenKind::RightBracket) {
            --bracketDepth;
            if (bracketDepth == 0) {
                rightBracketIndex = tokenIndex;
                break;
            }
        }
    }

    if (rightBracketIndex == 0 || rightBracketIndex + 1 >= tokens.size() || tokens[rightBracketIndex + 1].kind != TokenKind::Equals) {
        return false;
    }

    const size_t indexStart = static_cast<size_t>(tokens[1].span.endColumn);
    const size_t indexLength = static_cast<size_t>(tokens[rightBracketIndex].span.startColumn - tokens[1].span.endColumn - 1);
    target.name = tokens[0].text;
    target.nameColumn = statementColumn + tokens[0].span.startColumn - 1;
    target.indexText = statementBody.substr(indexStart, indexLength);
    target.indexColumn = statementColumn + tokens[2].span.startColumn - 1;
    equalsIndex = rightBracketIndex + 1;
    return true;
}

}

AssignmentEmitResult emitAssignmentStatement(
    const std::string& inputFile,
    int lineNumber,
    int statementColumn,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks
) {
    const std::vector<Token> tokens = tokenize(statementBody);
    if (tokens.size() < 2 || tokens[0].kind != TokenKind::Identifier) {
        return {false, true, "", {}};
    }

    std::vector<AssignmentTarget> assignmentTargets;
    size_t equalsIndex = 0;
    if (parseAssignmentTargets(tokens, statementBody, statementColumn, assignmentTargets, equalsIndex) && assignmentTargets.size() > 1) {
        const size_t expressionTokenIndex = equalsIndex + 1;
        if (tokens[expressionTokenIndex].kind == TokenKind::EndOfFile) {
            recordSourceError(inputFile, lineNumber, statementColumn + tokens[equalsIndex].span.endColumn, "expected expression after assignment", sourceLines);
            return {true, false, "", {}};
        }

        const int expressionStartColumn = statementColumn + tokens[expressionTokenIndex].span.startColumn - 1;
        const int relativeExpressionStartColumn = tokens[expressionTokenIndex].span.startColumn;
        int expressionEndColumn = tokens[expressionTokenIndex].span.endColumn;
        size_t tokenIndex = expressionTokenIndex;
        while (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
            expressionEndColumn = tokens[tokenIndex].span.endColumn;
            ++tokenIndex;
        }

        const std::string expressionText = statementBody.substr(
            static_cast<size_t>(relativeExpressionStartColumn - 1),
            static_cast<size_t>(expressionEndColumn - relativeExpressionStartColumn + 1)
        );
        const std::vector<Token> expressionTokens = tokenize(expressionText);
        std::string generatedStatement = "    ";
        std::vector<SourceRange> ranges;
        for (const AssignmentTarget& target : assignmentTargets) {
            const auto variable = declaredVariables.find(target.name);
            if (variable == declaredVariables.end()) {
                recordSourceError(inputFile, lineNumber, target.column, "use of undeclared variable '" + target.name + "'", sourceLines);
                return {true, false, "", {}};
            }

            if (target.indexed) {
                if (variable->second.primitive != PrimitiveType::List || variable->second.subtypes.size() != 1) {
                    recordSourceError(inputFile, lineNumber, target.column, "index assignment requires a List value", sourceLines);
                    return {true, false, "", {}};
                }

                const ExpressionEmitResult index = emitExpression(
                    inputFile,
                    lineNumber,
                    target.indexText,
                    target.indexColumn,
                    sourceLines,
                    declaredVariables,
                    emitRuntimeChecks
                );
                if (!index.ok) {
                    return {true, false, "", {}};
                }

                if (!index.explicitCast && !isImplicitlyConvertible(index.type, PrimitiveType::Int)) {
                    recordSourceError(inputFile, lineNumber, target.indexColumn, "list index must be int", sourceLines);
                    return {true, false, "", {}};
                }
            }
        }

        if (isInputCall(expressionTokens)) {
            for (size_t i = 0; i < assignmentTargets.size(); ++i) {
                if (assignmentTargets[i].indexed) {
                    recordSourceError(inputFile, lineNumber, assignmentTargets[i].column, "input() multi-assignment only supports variable targets", sourceLines);
                    return {true, false, "", {}};
                }
                const auto variable = declaredVariables.find(assignmentTargets[i].name);
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

        std::vector<std::string> expressions;
        std::vector<int> expressionColumns;
        int depth = 0;
        size_t startIndex = 0;
        int startColumn = expressionStartColumn;
        for (const Token& token : expressionTokens) {
            if (token.kind == TokenKind::EndOfFile) {
                break;
            }
            if (token.kind == TokenKind::LeftParen) {
                ++depth;
            } else if (token.kind == TokenKind::RightParen && depth > 0) {
                --depth;
            } else if (token.kind == TokenKind::Comma && depth == 0) {
                const size_t endIndex = static_cast<size_t>(token.span.startColumn - 1);
                expressions.push_back(expressionText.substr(startIndex, endIndex - startIndex));
                expressionColumns.push_back(startColumn);
                startIndex = static_cast<size_t>(token.span.endColumn);
                startColumn = expressionStartColumn + token.span.endColumn;
            }
        }
        expressions.push_back(expressionText.substr(startIndex));
        expressionColumns.push_back(startColumn);

        if (expressions.size() != assignmentTargets.size()) {
            recordSourceError(inputFile, lineNumber, expressionStartColumn, "multi-assignment requires the same number of values as targets", sourceLines);
            return {true, false, "", {}};
        }

        std::vector<std::string> emittedExpressions;
        std::vector<std::string> emittedIndices;
        for (size_t i = 0; i < expressions.size(); ++i) {
            const auto variable = declaredVariables.find(assignmentTargets[i].name);
            const Type targetType = assignmentTargets[i].indexed ? variable->second.subtypes[0] : variable->second;
            const ExpressionEmitResult expression = emitExpression(
                inputFile,
                lineNumber,
                expressions[i],
                expressionColumns[i],
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
                    expressionColumns[i],
                    "cannot implicitly convert " + cpppTypeName(expression.type) + " to " + cpppTypeName(targetType),
                    sourceLines
                );
                return {true, false, "", {}};
            }

            std::string emitted = expression.generatedExpression;
            if (!isImplicitlyConvertible(expression.type, targetType) || expression.type != targetType) {
                emitted = castExpressionTo(emitted, expression.type, targetType);
            }
            emittedExpressions.push_back(emitted);

            if (assignmentTargets[i].indexed) {
                const ExpressionEmitResult index = emitExpression(
                    inputFile,
                    lineNumber,
                    assignmentTargets[i].indexText,
                    assignmentTargets[i].indexColumn,
                    sourceLines,
                    declaredVariables,
                    emitRuntimeChecks
                );
                if (!index.ok) {
                    return {true, false, "", {}};
                }

                std::string emittedIndex = index.generatedExpression;
                if (!isImplicitlyConvertible(index.type, PrimitiveType::Int) || index.type != PrimitiveType::Int) {
                    emittedIndex = castExpressionTo(emittedIndex, index.type, PrimitiveType::Int);
                }
                emittedIndices.push_back(emittedIndex);
            } else {
                emittedIndices.push_back("");
            }
        }

        for (size_t i = 0; i < emittedExpressions.size(); ++i) {
            generatedStatement += "auto __cppp_tmp_" + std::to_string(i) + " = " + emittedExpressions[i] + "; ";
        }
        for (size_t i = 0; i < assignmentTargets.size(); ++i) {
            const AssignmentTarget& target = assignmentTargets[i];
            if (target.indexed) {
                if (emitRuntimeChecks) {
                    requireRuntimeHelper("CPPPListSet");
                    generatedStatement += "CPPPListSet(" + target.name + ", " + emittedIndices[i] + ", __cppp_tmp_" + std::to_string(i) + ", " + std::to_string(lineNumber) + ", " + std::to_string(target.indexColumn) + ");";
                } else {
                    generatedStatement += target.name + "[" + emittedIndices[i] + "] = __cppp_tmp_" + std::to_string(i) + ";";
                }
            } else {
                generatedStatement += target.name + " = __cppp_tmp_" + std::to_string(i) + ";";
            }
        }
        for (size_t i = 0; i < assignmentTargets.size(); ++i) {
            ranges.push_back({
                lineNumber,
                assignmentTargets[i].indexed ? assignmentTargets[i].column : assignmentTargets[i].column,
                5,
                5 + static_cast<int>(assignmentTargets[i].name.size()) - 1
            });
        }
        return {true, true, generatedStatement, ranges};
    }

    IndexedAssignmentTarget indexedTarget;
    size_t indexedEqualsIndex = 0;
    if (parseIndexedAssignmentTarget(tokens, statementBody, statementColumn, indexedTarget, indexedEqualsIndex)) {
        const auto variable = declaredVariables.find(indexedTarget.name);
        if (variable == declaredVariables.end()) {
            recordSourceError(inputFile, lineNumber, indexedTarget.nameColumn, "use of undeclared variable '" + indexedTarget.name + "'", sourceLines);
            return {true, false, "", {}};
        }

        if (variable->second.primitive != PrimitiveType::List || variable->second.subtypes.size() != 1) {
            recordSourceError(inputFile, lineNumber, indexedTarget.nameColumn, "index assignment requires a List value", sourceLines);
            return {true, false, "", {}};
        }

        if (tokens[indexedEqualsIndex + 1].kind == TokenKind::EndOfFile) {
            recordSourceError(inputFile, lineNumber, statementColumn + tokens[indexedEqualsIndex].span.endColumn, "expected expression after assignment", sourceLines);
            return {true, false, "", {}};
        }

        const ExpressionEmitResult index = emitExpression(
            inputFile,
            lineNumber,
            indexedTarget.indexText,
            indexedTarget.indexColumn,
            sourceLines,
            declaredVariables,
            emitRuntimeChecks
        );
        if (!index.ok) {
            return {true, false, "", {}};
        }

        if (!index.explicitCast && !isImplicitlyConvertible(index.type, PrimitiveType::Int)) {
            recordSourceError(inputFile, lineNumber, indexedTarget.indexColumn, "list index must be int", sourceLines);
            return {true, false, "", {}};
        }

        std::string emittedIndex = index.generatedExpression;
        if (!isImplicitlyConvertible(index.type, PrimitiveType::Int) || index.type != PrimitiveType::Int) {
            emittedIndex = castExpressionTo(emittedIndex, index.type, PrimitiveType::Int);
        }

        const int expressionStartColumn = statementColumn + tokens[indexedEqualsIndex + 1].span.startColumn - 1;
        const int relativeExpressionStartColumn = tokens[indexedEqualsIndex + 1].span.startColumn;
        int expressionEndColumn = tokens[indexedEqualsIndex + 1].span.endColumn;
        size_t tokenIndex = indexedEqualsIndex + 1;
        while (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
            expressionEndColumn = tokens[tokenIndex].span.endColumn;
            ++tokenIndex;
        }

        const std::string expressionText = statementBody.substr(
            static_cast<size_t>(relativeExpressionStartColumn - 1),
            static_cast<size_t>(expressionEndColumn - relativeExpressionStartColumn + 1)
        );

        const Type elementType = variable->second.subtypes[0];
        const ExpressionEmitResult expression = emitExpression(
            inputFile,
            lineNumber,
            expressionText,
            expressionStartColumn,
            sourceLines,
            declaredVariables,
            emitRuntimeChecks
        );
        if (!expression.ok) {
            return {true, false, "", {}};
        }

        if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, elementType)) {
            recordSourceError(
                inputFile,
                lineNumber,
                expressionStartColumn,
                "cannot implicitly convert " + cpppTypeName(expression.type) + " to " + cpppTypeName(elementType),
                sourceLines
            );
            return {true, false, "", {}};
        }

        std::string emittedExpression = expression.generatedExpression;
        if (!isImplicitlyConvertible(expression.type, elementType) || expression.type != elementType) {
            emittedExpression = castExpressionTo(emittedExpression, expression.type, elementType);
        }

        if (emitRuntimeChecks) {
            requireRuntimeHelper("CPPPListSet");
        }
        const std::string generatedStatement = emitRuntimeChecks
            ? "    CPPPListSet(" + indexedTarget.name + ", " + emittedIndex + ", " + emittedExpression + ", " + std::to_string(lineNumber) + ", " + std::to_string(indexedTarget.indexColumn) + ");"
            : "    " + indexedTarget.name + "[" + emittedIndex + "] = " + emittedExpression + ";";

        return {
            true,
            true,
            generatedStatement,
            {{
                lineNumber,
                indexedTarget.nameColumn,
                5,
                5 + static_cast<int>(indexedTarget.name.size()) - 1
            }}
        };
    }

    const bool simpleAssignment = tokens[1].kind == TokenKind::Equals;
    const bool compoundAssignment = isCompoundAssignment(tokens);
    if (!simpleAssignment && !compoundAssignment) {
        return {false, true, "", {}};
    }

    const size_t expressionTokenIndex = 2;
    if (tokens[expressionTokenIndex].kind == TokenKind::EndOfFile) {
        recordSourceError(inputFile, lineNumber, statementColumn + tokens[expressionTokenIndex - 1].span.endColumn, "expected expression after assignment", sourceLines);
        return {true, false, "", {}};
    }

    const std::string variableName = tokens[0].text;
    const auto variable = declaredVariables.find(variableName);
    if (variable == declaredVariables.end()) {
        recordSourceError(inputFile, lineNumber, statementColumn + tokens[0].span.startColumn - 1, "use of undeclared variable '" + variableName + "'", sourceLines);
        return {true, false, "", {}};
    }
    const Type targetType = variable->second;

    const int expressionStartColumn = statementColumn + tokens[expressionTokenIndex].span.startColumn - 1;
    const int relativeExpressionStartColumn = tokens[expressionTokenIndex].span.startColumn;
    int expressionEndColumn = tokens[expressionTokenIndex].span.endColumn;
    size_t tokenIndex = expressionTokenIndex;
    while (tokens[tokenIndex].kind != TokenKind::EndOfFile) {
        expressionEndColumn = tokens[tokenIndex].span.endColumn;
        ++tokenIndex;
    }

    std::string expressionText = statementBody.substr(
        static_cast<size_t>(relativeExpressionStartColumn - 1),
        static_cast<size_t>(expressionEndColumn - relativeExpressionStartColumn + 1)
    );
    const std::vector<Token> expressionTokens = tokenize(expressionText);
    std::string inputExpression;
    std::vector<InputArgument> inputArguments;
    if (simpleAssignment && parseInputCall(expressionText, expressionStartColumn, inputArguments)) {
        if (!emitInputCallForType(
                inputFile,
                lineNumber,
                expressionText,
                expressionStartColumn,
                targetType,
                sourceLines,
                declaredVariables,
                inputExpression)) {
            return {true, false, "", {}};
        }
        const std::string generatedStatement = "    " + variableName + " = " + inputExpression + ";";
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

    const bool preserveCompoundListAppend =
        compoundAssignment &&
        tokens[1].text == "+=" &&
        targetType.primitive == PrimitiveType::List &&
        targetType.subtypes.size() == 1;

    const ExpressionEmitResult expression = emitExpression(
        inputFile,
        lineNumber,
        compoundAssignment && !preserveCompoundListAppend ? variableName + " " + compoundOperator(tokens[1].text) + " " + expressionText : expressionText,
        compoundAssignment && !preserveCompoundListAppend ? statementColumn + tokens[0].span.startColumn - 1 : expressionStartColumn,
        sourceLines,
        declaredVariables
    );
    if (!expression.ok) {
        return {true, false, "", {}};
    }

    if (preserveCompoundListAppend) {
        if (expression.type != targetType) {
            recordSourceError(
                inputFile,
                lineNumber,
                expressionStartColumn,
                "cannot append " + cpppTypeName(expression.type) + " to " + cpppTypeName(targetType),
                sourceLines
            );
            return {true, false, "", {}};
        }

        const std::string generatedStatement =
            "    ([&]() { auto __cppp_append = " + expression.generatedExpression + "; " +
            variableName + ".insert(" + variableName + ".end(), __cppp_append.begin(), __cppp_append.end()); }());";
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
        emittedExpression = castExpressionTo(emittedExpression, expression.type, targetType);
    }

    const std::string generatedStatement = "    " + variableName + " = " + emittedExpression + ";";
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
