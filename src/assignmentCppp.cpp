/*
 * assignmentCppp.cpp
 *
 * Parses assignment statements and emits the corresponding C++ assignment logic, including compound operators and slice-aware rewrites.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "assignmentCppp.h"

#include "expressions.h"
#include "functions.h"
#include "tokenizer.h"
#include "typesCppp.h"

namespace {
// AssignmentTarget implements the AssignmentTarget behavior for the assignmentCppp.cpp module.
struct AssignmentTarget {
    std::vector<Token> tokens;
    int column = 0;
};

std::vector<Token> tokenRange(const std::vector<Token>& tokens, size_t begin, size_t end) {
    std::vector<Token> result;
    if (begin >= end || begin >= tokens.size()) return result;
    const int startColumn = tokens[begin].span.startColumn;
    const size_t startOffset = tokens[begin].span.startOffset;
    for (size_t index = begin; index < end && tokens[index].kind != TokenKind::EndOfFile; ++index) {
        Token token = tokens[index];
        token.span.startColumn -= startColumn - 1;
        token.span.endColumn -= startColumn - 1;
        token.span.startOffset -= startOffset;
        token.span.endOffset -= startOffset;
        result.push_back(std::move(token));
    }
    return result;
}

// isCompoundAssignmentOperator returns whether the supplied input satisfies the relevant condition.
bool isCompoundAssignmentOperator(const Token& token) {
    return token.kind == TokenKind::Operator &&
        (token.text == "+=" || token.text == "-=" || token.text == "*=" ||
         token.text == "/=" || token.text == "%=" || token.text == "<<=" ||
         token.text == ">>=" || token.text == "&=" || token.text == "|=" ||
         token.text == "^=" || token.text == "&&=" || token.text == "||=");
}

// isAssignmentOperatorToken returns whether the supplied input satisfies the relevant condition.
bool isAssignmentOperatorToken(const Token& token) {
    return token.kind == TokenKind::Equals || isCompoundAssignmentOperator(token);
}

// compoundOperator implements the compoundOperator behavior for the assignmentCppp.cpp module.
std::string compoundOperator(const std::string& assignmentOperator) {
    return assignmentOperator.substr(0, assignmentOperator.size() - 1);
}

bool parseAssignmentStructure(
    const std::vector<Token>& tokens,
    int statementColumn,
    std::vector<AssignmentTarget>& targets,
    size_t& operatorIndex
) {
    int bracketDepth = 0;
    int parenDepth = 0;
    int braceDepth = 0;
    size_t segmentStart = 0;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens[i];
        if (token.kind == TokenKind::EndOfFile) {
            break;
        }
        if (token.kind == TokenKind::LeftBracket) {
            ++bracketDepth;
            continue;
        }
        if (token.kind == TokenKind::RightBracket) {
            --bracketDepth;
            continue;
        }
        if (token.kind == TokenKind::LeftParen) {
            ++parenDepth;
            continue;
        }
        if (token.kind == TokenKind::RightParen) {
            --parenDepth;
            continue;
        }
        if (token.kind == TokenKind::LeftBrace) {
            ++braceDepth;
            continue;
        }
        if (token.kind == TokenKind::RightBrace) {
            --braceDepth;
            continue;
        }

        if (bracketDepth == 0 && parenDepth == 0 && braceDepth == 0 && isAssignmentOperatorToken(token)) {
            operatorIndex = i;
            break;
        }

        if (bracketDepth == 0 && parenDepth == 0 && braceDepth == 0 && token.kind == TokenKind::Comma) {
            if (segmentStart >= i) {
                return false;
            }
            const int startColumn = tokens[segmentStart].span.startColumn;
            targets.push_back({
                tokenRange(tokens, segmentStart, i),
                statementColumn + startColumn - 1
            });
            segmentStart = i + 1;
        }
    }

    if (operatorIndex == 0 || !isAssignmentOperatorToken(tokens[operatorIndex])) {
        return false;
    }
    if (segmentStart >= operatorIndex) {
        return false;
    }

    const int startColumn = tokens[segmentStart].span.startColumn;
    targets.push_back({
        tokenRange(tokens, segmentStart, operatorIndex),
        statementColumn + startColumn - 1
    });
    return !targets.empty();
}
}

AssignmentEmitResult emitAssignmentStatement(
    const std::string& inputFile,
    int lineNumber,
    int statementColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    bool emitRuntimeChecks,
    const std::vector<Token>& sourceTokens
) {
    const std::vector<Token>& tokens = sourceTokens;
    if (tokens.size() < 3) {
        return {false, true, "", {}};
    }

    std::vector<AssignmentTarget> targets;
    size_t operatorIndex = 0;
    if (!parseAssignmentStructure(tokens, statementColumn, targets, operatorIndex)) {
        return {false, true, "", {}};
    }

    const bool simpleAssignment = tokens[operatorIndex].kind == TokenKind::Equals;
    const bool compoundAssignment = isCompoundAssignmentOperator(tokens[operatorIndex]);
    if (targets.size() > 1 && !simpleAssignment) {
        return {false, true, "", {}};
    }

    if (tokens[operatorIndex + 1].kind == TokenKind::EndOfFile) {
        recordSourceError(inputFile, lineNumber, statementColumn + tokens[operatorIndex].span.endColumn, "expected expression after assignment", sourceLines);
        return {true, false, "", {}};
    }

    const int expressionStartRelativeColumn = tokens[operatorIndex + 1].span.startColumn;
    const int expressionColumn = statementColumn + expressionStartRelativeColumn - 1;
    const std::vector<Token> expressionTokens = tokenRange(tokens, operatorIndex + 1, tokens.size());

    std::vector<LvalueEmitResult> emittedTargets;
    emittedTargets.reserve(targets.size());
    for (const AssignmentTarget& target : targets) {
        const LvalueEmitResult emittedTarget = emitLvalueExpression(
            inputFile,
            lineNumber,
            target.tokens,
            target.column,
            sourceLines,
            declaredVariables,
            declaredFunctions,
            emitRuntimeChecks
        );
        if (!emittedTarget.ok) {
            return {true, false, "", {}};
        }
        emittedTargets.push_back(emittedTarget);
    }

    if (targets.size() > 1) {
        std::string inputExpression;
        std::vector<InputArgument> inputArguments;
        if (isInputCall(expressionTokens)) {
            std::string generatedStatement = "    ";
            std::vector<SourceRange> ranges;
            for (size_t i = 0; i < emittedTargets.size(); ++i) {
                if (i > 0) {
                    generatedStatement += " ";
                }

                if (!emitInputCallForType(
                        inputFile,
                        lineNumber,
                        expressionTokens,
                        expressionColumn,
                        emittedTargets[i].type,
                        sourceLines,
                        declaredVariables,
                        inputExpression)) {
                    return {true, false, "", {}};
                }

                const int generatedStartColumn = static_cast<int>(generatedStatement.size()) + 1;
                generatedStatement += emittedTargets[i].generatedExpression + " = " + inputExpression + ";";
                ranges.push_back({
                    lineNumber,
                    targets[i].column,
                    generatedStartColumn,
                    generatedStartColumn + static_cast<int>(emittedTargets[i].generatedExpression.size()) - 1
                });
            }
            return {true, true, generatedStatement, ranges};
        }

        std::vector<std::vector<Token>> expressions;
        std::vector<int> expressionColumns;
        int parenDepth = 0;
        int bracketDepth = 0;
        int braceDepth = 0;
        size_t segmentStart = 0;
        int segmentColumn = expressionColumn;
        for (const Token& token : expressionTokens) {
            if (token.kind == TokenKind::EndOfFile) {
                break;
            }
            if (token.kind == TokenKind::LeftParen) {
                ++parenDepth;
            } else if (token.kind == TokenKind::RightParen) {
                --parenDepth;
            } else if (token.kind == TokenKind::LeftBracket) {
                ++bracketDepth;
            } else if (token.kind == TokenKind::RightBracket) {
                --bracketDepth;
            } else if (token.kind == TokenKind::LeftBrace) {
                ++braceDepth;
            } else if (token.kind == TokenKind::RightBrace) {
                --braceDepth;
            } else if (token.kind == TokenKind::Comma && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
                expressions.push_back(tokenRange(expressionTokens, segmentStart, static_cast<size_t>(&token - expressionTokens.data())));
                expressionColumns.push_back(segmentColumn);
                segmentStart = static_cast<size_t>(&token - expressionTokens.data()) + 1;
                segmentColumn = expressionColumn + token.span.endColumn;
            }
        }
        expressions.push_back(tokenRange(expressionTokens, segmentStart, expressionTokens.size()));
        expressionColumns.push_back(segmentColumn);

        if (expressions.size() != emittedTargets.size()) {
            recordSourceError(inputFile, lineNumber, expressionColumn, "multi-assignment requires the same number of values as targets", sourceLines);
            return {true, false, "", {}};
        }

        std::vector<std::string> emittedExpressions;
        for (size_t i = 0; i < expressions.size(); ++i) {
            const ExpressionEmitResult expression = emitExpression(
                inputFile,
                lineNumber,
                expressions[i],
                expressionColumns[i],
                sourceLines,
                declaredVariables,
                declaredFunctions
            );
            if (!expression.ok) {
                return {true, false, "", {}};
            }
            if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, emittedTargets[i].type)) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    expressionColumns[i],
                    "cannot implicitly convert " + cpppTypeName(expression.type) + " to " + cpppTypeName(emittedTargets[i].type),
                    sourceLines
                );
                return {true, false, "", {}};
            }

            std::string emitted = expression.generatedExpression;
            if (expression.type != emittedTargets[i].type) {
                emitted = castExpressionTo(emitted, expression.type, emittedTargets[i].type);
            }
            emittedExpressions.push_back(emitted);
        }

        std::string generatedStatement = "    ";
        for (size_t i = 0; i < emittedExpressions.size(); ++i) {
            generatedStatement += "auto __cppp_tmp_" + std::to_string(i) + " = " + emittedExpressions[i] + "; ";
        }

        std::vector<SourceRange> ranges;
        for (size_t i = 0; i < emittedTargets.size(); ++i) {
            const int generatedStartColumn = static_cast<int>(generatedStatement.size()) + 1;
            generatedStatement += emittedTargets[i].generatedExpression + " = __cppp_tmp_" + std::to_string(i) + ";";
            ranges.push_back({
                lineNumber,
                targets[i].column,
                generatedStartColumn,
                generatedStartColumn + static_cast<int>(emittedTargets[i].generatedExpression.size()) - 1
            });
        }
        return {true, true, generatedStatement, ranges};
    }

    const LvalueEmitResult& target = emittedTargets[0];
    std::string inputExpression;
    std::vector<InputArgument> inputArguments;
    if (simpleAssignment && parseInputCall(expressionTokens, expressionColumn, inputArguments)) {
        if (!emitInputCallForType(
                inputFile,
                lineNumber,
                expressionTokens,
                expressionColumn,
                target.type,
                sourceLines,
                declaredVariables,
                inputExpression)) {
            return {true, false, "", {}};
        }
        return {
            true,
            true,
            "    " + target.generatedExpression + " = " + inputExpression + ";",
            {{
                lineNumber,
                targets[0].column,
                5,
                5 + static_cast<int>(target.generatedExpression.size()) - 1
            }}
        };
    }

    const bool preserveCompoundListAppend =
        compoundAssignment &&
        tokens[operatorIndex].text == "+=" &&
        target.type.primitive == PrimitiveType::List &&
        target.type.subtypes.size() == 1 &&
        targets[0].tokens.size() == 1 &&
        targets[0].tokens[0].kind == TokenKind::Identifier &&
        target.generatedExpression == targets[0].tokens[0].text;

    std::vector<Token> combinedExpressionTokens = expressionTokens;
    if (compoundAssignment && !preserveCompoundListAppend) {
        combinedExpressionTokens = targets[0].tokens;
        Token operation = tokens[operatorIndex];
        operation.kind = TokenKind::Operator;
        operation.text = compoundOperator(operation.text);
        combinedExpressionTokens.push_back(operation);
        Token leftParen = operation;
        leftParen.kind = TokenKind::LeftParen;
        leftParen.text = "(";
        combinedExpressionTokens.push_back(leftParen);
        combinedExpressionTokens.insert(combinedExpressionTokens.end(), expressionTokens.begin(), expressionTokens.end());
        Token rightParen = expressionTokens.back();
        rightParen.kind = TokenKind::RightParen;
        rightParen.text = ")";
        combinedExpressionTokens.push_back(rightParen);
    }
    const int combinedExpressionColumn =
        compoundAssignment && !preserveCompoundListAppend ? targets[0].column : expressionColumn;

    const ExpressionEmitResult expression = emitExpression(
        inputFile,
        lineNumber,
        combinedExpressionTokens,
        combinedExpressionColumn,
        sourceLines,
        declaredVariables,
        declaredFunctions
    );
    if (!expression.ok) {
        return {true, false, "", {}};
    }

    if (preserveCompoundListAppend) {
        if (expression.type != target.type) {
            recordSourceError(
                inputFile,
                lineNumber,
                expressionColumn,
                "cannot append " + cpppTypeName(expression.type) + " to " + cpppTypeName(target.type),
                sourceLines
            );
            return {true, false, "", {}};
        }

        return {
            true,
            true,
            "    ([&]() { auto __cppp_append = " + expression.generatedExpression + "; " +
                target.generatedExpression + ".insert(" + target.generatedExpression + ".end(), __cppp_append.begin(), __cppp_append.end()); }());",
            {{
                lineNumber,
                targets[0].column,
                5,
                5 + static_cast<int>(target.generatedExpression.size()) - 1
            }}
        };
    }

    if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, target.type)) {
        recordSourceError(
            inputFile,
            lineNumber,
            expressionColumn,
            "cannot implicitly convert " + cpppTypeName(expression.type) + " to " + cpppTypeName(target.type),
            sourceLines
        );
        return {true, false, "", {}};
    }

    if (expression.explicitCast &&
        expression.type != target.type &&
        !canExplicitlyCastType(expression.type, target.type)) {
        recordSourceError(
            inputFile,
            lineNumber,
            expressionColumn,
            "cannot cast " + cpppTypeName(expression.type) + " to " + cpppTypeName(target.type),
            sourceLines
        );
        return {true, false, "", {}};
    }

    std::string emittedExpression = expression.generatedExpression;
    if (expression.type != target.type) {
        emittedExpression = castExpressionTo(emittedExpression, expression.type, target.type);
    }

    return {
        true,
        true,
        "    " + target.generatedExpression + " = " + emittedExpression + ";",
        {{
            lineNumber,
            targets[0].column,
            5,
            5 + static_cast<int>(target.generatedExpression.size()) - 1
        }}
    };
}
