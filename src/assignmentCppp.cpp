/*
 * assignmentCppp.cpp
 *
 * Emits assignments whose structure has already been established by ProgramAst.
 * Only expression and lvalue token slices remain as compatibility adapters.
 */

#include "assignmentCppp.h"

#include "functions.h"
#include "typesCppp.h"

AssignmentEmitResult emitParsedAssignment(
    const std::string& inputFile,
    int lineNumber,
    int statementColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    bool emitRuntimeChecks,
    const std::string& operation,
    const Token& operationToken,
    const std::vector<std::vector<Token>>& targetTokens,
    const std::vector<size_t>& targetOffsets,
    const std::vector<std::vector<Token>>& valueTokens,
    const std::vector<size_t>& valueOffsets
) {
    if (targetTokens.empty()) return {true, false, "", {}};
    const bool simple = operation == "=";
    const bool compound = operation.size() > 1 && operation.back() == '=';
    if (targetTokens.size() > 1 && !simple) return {true, false, "", {}};
    if (valueTokens.empty()) {
        recordSourceError(inputFile, lineNumber,
            statementColumn + operationToken.span.endColumn,
            "expected expression after assignment", sourceLines);
        return {true, false, "", {}};
    }

    std::vector<LvalueEmitResult> targets;
    targets.reserve(targetTokens.size());
    for (size_t index = 0; index < targetTokens.size(); ++index) {
        const int column = statementColumn + static_cast<int>(targetOffsets[index]);
        const LvalueEmitResult target = emitLvalueExpression(
            inputFile, lineNumber, targetTokens[index], column, sourceLines,
            declaredVariables, declaredFunctions, emitRuntimeChecks);
        if (!target.ok) return {true, false, "", {}};
        targets.push_back(target);
    }

    const int expressionColumn = statementColumn + static_cast<int>(valueOffsets.front());
    if (targets.size() > 1) {
        if (valueTokens.size() == 1 && isInputCall(valueTokens.front())) {
            std::string generated = "    ";
            std::vector<SourceRange> ranges;
            for (size_t index = 0; index < targets.size(); ++index) {
                if (index > 0) generated += " ";
                std::string inputExpression;
                if (!emitInputCallForType(
                        inputFile, lineNumber, valueTokens.front(), expressionColumn,
                        targets[index].type, sourceLines, declaredVariables,
                        inputExpression)) {
                    return {true, false, "", {}};
                }
                const int generatedStart = static_cast<int>(generated.size()) + 1;
                generated += targets[index].generatedExpression + " = " + inputExpression + ";";
                ranges.push_back({
                    lineNumber,
                    statementColumn + static_cast<int>(targetOffsets[index]),
                    generatedStart,
                    generatedStart + static_cast<int>(targets[index].generatedExpression.size()) - 1
                });
            }
            return {true, true, generated, ranges};
        }

        if (valueTokens.size() != targets.size()) {
            recordSourceError(inputFile, lineNumber, expressionColumn,
                "multi-assignment requires the same number of values as targets", sourceLines);
            return {true, false, "", {}};
        }
        std::vector<std::string> expressions;
        for (size_t index = 0; index < valueTokens.size(); ++index) {
            const int column = statementColumn + static_cast<int>(valueOffsets[index]);
            const ExpressionEmitResult expression = emitExpression(
                inputFile, lineNumber, valueTokens[index], column, sourceLines,
                declaredVariables, declaredFunctions);
            if (!expression.ok) return {true, false, "", {}};
            if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, targets[index].type)) {
                recordSourceError(inputFile, lineNumber, column,
                    "cannot implicitly convert " + cpppTypeName(expression.type) + " to " + cpppTypeName(targets[index].type),
                    sourceLines);
                return {true, false, "", {}};
            }
            expressions.push_back(expression.type == targets[index].type
                ? expression.generatedExpression
                : castExpressionTo(expression.generatedExpression, expression.type, targets[index].type));
        }

        std::string generated = "    ";
        for (size_t index = 0; index < expressions.size(); ++index) {
            generated += "auto __cppp_tmp_" + std::to_string(index) + " = " + expressions[index] + "; ";
        }
        std::vector<SourceRange> ranges;
        for (size_t index = 0; index < targets.size(); ++index) {
            const int generatedStart = static_cast<int>(generated.size()) + 1;
            generated += targets[index].generatedExpression + " = __cppp_tmp_" + std::to_string(index) + ";";
            ranges.push_back({
                lineNumber,
                statementColumn + static_cast<int>(targetOffsets[index]),
                generatedStart,
                generatedStart + static_cast<int>(targets[index].generatedExpression.size()) - 1
            });
        }
        return {true, true, generated, ranges};
    }

    const LvalueEmitResult& target = targets.front();
    const std::vector<Token>& value = valueTokens.front();
    if (simple && isInputCall(value)) {
        std::string inputExpression;
        if (!emitInputCallForType(
                inputFile, lineNumber, value, expressionColumn, target.type,
                sourceLines, declaredVariables, inputExpression)) {
            return {true, false, "", {}};
        }
        return {
            true,
            true,
            "    " + target.generatedExpression + " = " + inputExpression + ";",
            {{lineNumber, statementColumn + static_cast<int>(targetOffsets.front()), 5,
              5 + static_cast<int>(target.generatedExpression.size()) - 1}}
        };
    }

    const bool preserveListAppend = compound && operation == "+=" &&
        target.type.primitive == PrimitiveType::List && target.type.subtypes.size() == 1 &&
        targetTokens.front().size() == 1 && targetTokens.front()[0].kind == TokenKind::Identifier &&
        target.generatedExpression == targetTokens.front()[0].text;

    std::vector<Token> combined = value;
    int combinedColumn = expressionColumn;
    if (compound && !preserveListAppend) {
        combined = targetTokens.front();
        Token binary = operationToken;
        binary.kind = TokenKind::Operator;
        binary.text = operation.substr(0, operation.size() - 1);
        combined.push_back(binary);
        Token leftParen = binary;
        leftParen.kind = TokenKind::LeftParen;
        leftParen.text = "(";
        combined.push_back(leftParen);
        combined.insert(combined.end(), value.begin(), value.end());
        Token rightParen = value.back();
        rightParen.kind = TokenKind::RightParen;
        rightParen.text = ")";
        combined.push_back(rightParen);
        combinedColumn = statementColumn + static_cast<int>(targetOffsets.front());
    }

    const ExpressionEmitResult expression = emitExpression(
        inputFile, lineNumber, combined, combinedColumn, sourceLines,
        declaredVariables, declaredFunctions);
    if (!expression.ok) return {true, false, "", {}};

    if (preserveListAppend) {
        if (expression.type != target.type) {
            recordSourceError(inputFile, lineNumber, expressionColumn,
                "cannot append " + cpppTypeName(expression.type) + " to " + cpppTypeName(target.type),
                sourceLines);
            return {true, false, "", {}};
        }
        return {
            true,
            true,
            "    ([&]() { auto __cppp_append = " + expression.generatedExpression + "; " +
                target.generatedExpression + ".insert(" + target.generatedExpression +
                ".end(), __cppp_append.begin(), __cppp_append.end()); }());",
            {{lineNumber, statementColumn + static_cast<int>(targetOffsets.front()), 5,
              5 + static_cast<int>(target.generatedExpression.size()) - 1}}
        };
    }

    if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, target.type)) {
        recordSourceError(inputFile, lineNumber, expressionColumn,
            "cannot implicitly convert " + cpppTypeName(expression.type) + " to " + cpppTypeName(target.type),
            sourceLines);
        return {true, false, "", {}};
    }
    if (expression.explicitCast && expression.type != target.type &&
        !canExplicitlyCastType(expression.type, target.type)) {
        recordSourceError(inputFile, lineNumber, expressionColumn,
            "cannot cast " + cpppTypeName(expression.type) + " to " + cpppTypeName(target.type),
            sourceLines);
        return {true, false, "", {}};
    }
    const std::string emitted = expression.type == target.type
        ? expression.generatedExpression
        : castExpressionTo(expression.generatedExpression, expression.type, target.type);
    return {
        true,
        true,
        "    " + target.generatedExpression + " = " + emitted + ";",
        {{lineNumber, statementColumn + static_cast<int>(targetOffsets.front()), 5,
          5 + static_cast<int>(target.generatedExpression.size()) - 1}}
    };
}
