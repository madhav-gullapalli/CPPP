#include "statementCompiler.h"

#include "assignmentCppp.h"
#include "controlFlow.h"
#include "errors.h"
#include "listsCppp.h"
#include "printCppp.h"
#include "sourceSplitter.h"
#include "typesCppp.h"

#include <algorithm>
#include <set>
#include <string>

namespace {
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::string indentForDepth(int depth) {
    return std::string(static_cast<size_t>((depth + 1) * 4), ' ');
}

std::string indentGeneratedStatement(const std::string& generatedStatement, int depth) {
    return indentForDepth(depth) + trim(generatedStatement);
}

std::string stripGeneratedStatement(const std::string& generatedStatement) {
    std::string text = trim(generatedStatement);
    if (!text.empty() && text.back() == ';') {
        text.pop_back();
    }

    return text;
}

bool isLoopBlockKind(const std::string& kind) {
    return kind == "for" || kind == "while" || kind == "rep";
}

bool isRepCountType(Type type) {
    return type == PrimitiveType::Bool ||
        type == PrimitiveType::Char ||
        type == PrimitiveType::Int ||
        type == PrimitiveType::Float;
}
}

void compileSourceFragments(CompileContext& context, const std::vector<SourceFragment>& sourceFragments) {
    for (const SourceFragment& fragment : sourceFragments) {
        const int lineNumber = fragment.lineNumber;
        const std::string& line = fragment.text;
        const size_t commentStart = findLineCommentStart(line);
        const std::string commentText = commentStart == std::string::npos ? "" : trim(line.substr(commentStart));
        const bool hasComment = !commentText.empty();
        const std::string codeText = commentStart == std::string::npos ? line : line.substr(0, commentStart);
        const std::string statement = trim(codeText);

        if (statement.empty()) {
            if (hasComment) {
                context.queueGeneratedLine(indentForDepth(context.blockDepth) + commentText, lineNumber);
            }
            continue;
        }

        if (context.suppressedBlockDepth > 0 && statement[0] != '}') {
            if (statement.back() == '{') {
                ++context.blockDepth;
                context.pushBlock("suppressed");
                ++context.suppressedBlockDepth;
            }
            continue;
        }

        const size_t statementStart = codeText.find(statement);
        const int statementStartColumn = static_cast<int>(statementStart == std::string::npos ? 1 : statementStart + 1);

        const auto emitConditionHeader = [&](const std::string& keyword, const ConditionHeader& header, size_t absoluteOffset = 0, const std::string& breakFlag = std::string()) {
            const size_t conditionOffset = absoluteOffset + header.conditionOffset;
            if (header.condition.empty()) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + static_cast<int>(conditionOffset), "expected condition", context.sourceLines);
                ++context.blockDepth;
                context.pushBlock(keyword, breakFlag);
                context.canAttachElse = false;
                return false;
            }

            const ExpressionEmitResult condition = emitExpression(
                context.options.inputFile,
                lineNumber,
                header.condition,
                statementStartColumn + static_cast<int>(conditionOffset),
                context.sourceLines,
                context.declaredVariables
            );
            if (!condition.ok) {
                ++context.blockDepth;
                context.pushBlock(keyword, breakFlag);
                context.canAttachElse = false;
                return false;
            }

            if (!isImplicitlyConvertible(condition.type, PrimitiveType::Bool)) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + static_cast<int>(conditionOffset), keyword + " condition must be bool", context.sourceLines);
                ++context.blockDepth;
                context.pushBlock(keyword, breakFlag);
                context.canAttachElse = false;
                return false;
            }

            std::string generatedCondition = condition.generatedExpression;
            if (condition.type != PrimitiveType::Bool) {
                generatedCondition = castExpressionTo(generatedCondition, condition.type, PrimitiveType::Bool);
            }

            if (!breakFlag.empty()) {
                context.queueGeneratedLine(indentForDepth(context.blockDepth) + "bool " + breakFlag + " = true;", lineNumber);
            }
            context.queueGeneratedLine(indentForDepth(context.blockDepth) + keyword + " (" + generatedCondition + ") {" + (hasComment ? " " + commentText : ""), lineNumber);
            ++context.blockDepth;
            context.pushBlock(keyword, breakFlag);
            context.canAttachElse = false;
            context.pendingLoopElse.active = false;
            return true;
        };

        const auto emitForPart = [&](const std::string& part, int partColumn, bool allowDeclaration, std::string& generatedPart) {
            generatedPart.clear();
            if (part.empty()) {
                return true;
            }

            if (allowDeclaration) {
                const TypeEmitResult typeResult = emitTypeDeclaration(context.options.inputFile, lineNumber, line, part, context.sourceLines, context.declaredVariables);
                if (typeResult.matched) {
                    if (!typeResult.ok) {
                        return false;
                    }

                    generatedPart = stripGeneratedStatement(typeResult.generatedStatement);
                    return true;
                }
            }

            const AssignmentEmitResult assignmentResult = emitAssignmentStatement(
                context.options.inputFile,
                lineNumber,
                part,
                context.sourceLines,
                context.declaredVariables,
                !context.options.shouldSubmit
            );
            if (assignmentResult.matched) {
                if (!assignmentResult.ok) {
                    return false;
                }

                generatedPart = stripGeneratedStatement(assignmentResult.generatedStatement);
                return true;
            }

            const ExpressionEmitResult expression = emitExpression(
                context.options.inputFile,
                lineNumber,
                part,
                partColumn,
                context.sourceLines,
                context.declaredVariables
            );
            if (!expression.ok) {
                return false;
            }

            generatedPart = expression.generatedExpression;
            return true;
        };

        if (statement[0] == '}') {
            if (context.blockDepth == 0) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "unmatched closing brace", context.sourceLines);
                continue;
            }

            const bool closingSuppressed = context.suppressedBlockDepth > 0;
            if (closingSuppressed) {
                --context.suppressedBlockDepth;
            }
            --context.blockDepth;
            const std::string closedBlock = context.blockKinds.empty() ? "" : context.blockKinds.back();
            const std::string closedBreakFlag = context.blockBreakFlags.empty() ? "" : context.blockBreakFlags.back();
            const std::vector<std::string> closedDeclaredNames = context.blockDeclaredNames.empty() ? std::vector<std::string>{} : context.blockDeclaredNames.back();
            if (!context.blockKinds.empty()) {
                context.blockKinds.pop_back();
                context.blockBreakFlags.pop_back();
                context.blockDeclaredNames.pop_back();
            }
            context.eraseDeclaredNames(closedDeclaredNames);
            context.canAttachElse = closedBlock == "if" || closedBlock == "else if";
            context.pendingLoopElse.active = isLoopBlockKind(closedBlock);
            context.pendingLoopElse.breakFlagName = context.pendingLoopElse.active ? closedBreakFlag : "";

            const std::string afterBrace = trim(statement.substr(1));
            if (closingSuppressed) {
                if (!afterBrace.empty() && afterBrace.back() == '{') {
                    ++context.blockDepth;
                    context.pushBlock("suppressed");
                    ++context.suppressedBlockDepth;
                }
                continue;
            }
            if (afterBrace.empty()) {
                context.queueGeneratedLine(indentForDepth(context.blockDepth) + "}" + (hasComment ? " " + commentText : ""), lineNumber);
                continue;
            }

            if (!context.canAttachElse) {
                if (context.pendingLoopElse.active && parseElseHeader(afterBrace)) {
                    context.queueGeneratedLine(indentForDepth(context.blockDepth) + "} if (" + context.pendingLoopElse.breakFlagName + ") {" + (hasComment ? " " + commentText : ""), lineNumber);
                    ++context.blockDepth;
                    context.pushBlock("loop else");
                    context.pendingLoopElse.active = false;
                    continue;
                }

                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + 1, "else without matching if", context.sourceLines);
                if (parseElseHeader(afterBrace)) {
                    ++context.blockDepth;
                    context.pushBlock("else");
                } else {
                    ConditionHeader recoveryHeader;
                    if (parseElseIfHeader(afterBrace, recoveryHeader)) {
                        ++context.blockDepth;
                        context.pushBlock("else if");
                    }
                }
                context.pendingLoopElse.active = false;
                continue;
            }

            if (parseElseHeader(afterBrace)) {
                context.queueGeneratedLine(indentForDepth(context.blockDepth) + "} else {" + (hasComment ? " " + commentText : ""), lineNumber);
                ++context.blockDepth;
                context.pushBlock("else");
                context.canAttachElse = false;
                context.pendingLoopElse.active = false;
                continue;
            }

            ConditionHeader header;
            if (parseElseIfHeader(afterBrace, header)) {
                if (header.condition.empty()) {
                    recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + 1 + static_cast<int>(header.conditionOffset), "expected condition", context.sourceLines);
                    ++context.blockDepth;
                    context.pushBlock("else if");
                    context.canAttachElse = false;
                    context.pendingLoopElse.active = false;
                    continue;
                }

                const ExpressionEmitResult condition = emitExpression(
                    context.options.inputFile,
                    lineNumber,
                    header.condition,
                    statementStartColumn + 1 + static_cast<int>(header.conditionOffset),
                    context.sourceLines,
                    context.declaredVariables
                );
                if (!condition.ok) {
                    ++context.blockDepth;
                    context.pushBlock("else if");
                    context.canAttachElse = false;
                    context.pendingLoopElse.active = false;
                    continue;
                }

                if (!isImplicitlyConvertible(condition.type, PrimitiveType::Bool)) {
                    recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + 1 + static_cast<int>(header.conditionOffset), "else if condition must be bool", context.sourceLines);
                    ++context.blockDepth;
                    context.pushBlock("else if");
                    context.canAttachElse = false;
                    context.pendingLoopElse.active = false;
                    continue;
                }

                std::string generatedCondition = condition.generatedExpression;
                if (condition.type != PrimitiveType::Bool) {
                    generatedCondition = castExpressionTo(generatedCondition, condition.type, PrimitiveType::Bool);
                }

                context.queueGeneratedLine(indentForDepth(context.blockDepth) + "} else if (" + generatedCondition + ") {" + (hasComment ? " " + commentText : ""), lineNumber);
                ++context.blockDepth;
                context.pushBlock("else if");
                context.canAttachElse = false;
                context.pendingLoopElse.active = false;
                continue;
            }

            recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + 1, "else without matching if", context.sourceLines);
            if (parseElseHeader(afterBrace)) {
                ++context.blockDepth;
                context.pushBlock("else");
            }
            context.pendingLoopElse.active = false;
            continue;
        }

        if (context.pendingLoopElse.active) {
            if (parseElseHeader(statement)) {
                context.queueGeneratedLine(indentForDepth(context.blockDepth) + "if (" + context.pendingLoopElse.breakFlagName + ") {" + (hasComment ? " " + commentText : ""), lineNumber);
                ++context.blockDepth;
                context.pushBlock("loop else");
                context.pendingLoopElse.active = false;
                context.canAttachElse = false;
                continue;
            }

            context.pendingLoopElse.active = false;
        }

        ConditionParseResult elseIfResult = parseElseIfHeaderDetailed(statement);
        if (context.canAttachElse) {
            if (parseElseHeader(statement)) {
                context.queueGeneratedLine(indentForDepth(context.blockDepth) + "else {" + (hasComment ? " " + commentText : ""), lineNumber);
                ++context.blockDepth;
                context.pushBlock("else");
                context.canAttachElse = false;
                continue;
            }

            if (elseIfResult.matched) {
                if (!elseIfResult.ok) {
                    recordSourceError(
                        context.options.inputFile,
                        lineNumber,
                        statementStartColumn + static_cast<int>(elseIfResult.errorOffset),
                        elseIfResult.message,
                        context.sourceLines
                    );
                    ++context.blockDepth;
                    context.pushBlock("else if");
                    ++context.suppressedBlockDepth;
                    context.pendingLoopElse.active = false;
                    context.canAttachElse = false;
                    continue;
                }

                emitConditionHeader("else if", elseIfResult.header);
                continue;
            }
        }

        if (elseIfResult.matched) {
            recordSourceError(
                context.options.inputFile,
                lineNumber,
                statementStartColumn + static_cast<int>(elseIfResult.ok ? 0 : elseIfResult.errorOffset),
                "else without matching if",
                context.sourceLines
            );
            ++context.blockDepth;
            context.pushBlock("else if");
            ++context.suppressedBlockDepth;
            context.pendingLoopElse.active = false;
            context.canAttachElse = false;
            continue;
        }

        if (parseElseHeader(statement)) {
            recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "else without matching if", context.sourceLines);
            ++context.blockDepth;
            context.pushBlock("else");
            ++context.suppressedBlockDepth;
            context.pendingLoopElse.active = false;
            context.canAttachElse = false;
            continue;
        }

        context.pendingLoopElse.active = false;

        const ForParseResult forResult = parseForHeaderDetailed(statement);
        const ForEachParseResult forEachResult = parseForEachHeader(statement);
        ForHeader forHeader;
        if (forEachResult.matched && !forEachResult.ok) {
            recordSourceError(
                context.options.inputFile,
                lineNumber,
                statementStartColumn + static_cast<int>(forEachResult.errorOffset),
                forEachResult.message,
                context.sourceLines
            );
            ++context.blockDepth;
            context.pushBlock("for");
            ++context.suppressedBlockDepth;
            context.canAttachElse = false;
            continue;
        }

        if (forEachResult.matched) {
            const std::string breakFlagName = "__cppp_loop_completed_" + std::to_string(context.loopControlIndex++);
            const ForEachHeader& forEachHeader = forEachResult.header;
            const TypeEmitResult declarationResult = emitTypeDeclaration(
                context.options.inputFile,
                lineNumber,
                line,
                forEachHeader.declaration,
                context.sourceLines,
                context.declaredVariables
            );
            if (!declarationResult.matched || !declarationResult.ok) {
                ++context.blockDepth;
                context.pushBlock("for");
                ++context.suppressedBlockDepth;
                context.canAttachElse = false;
                continue;
            }

            const auto loopVariable = context.declaredVariables.find(forEachHeader.variableName);
            const ExpressionEmitResult iterable = emitExpression(
                context.options.inputFile,
                lineNumber,
                forEachHeader.iterable,
                statementStartColumn + static_cast<int>(forEachHeader.iterableOffset),
                context.sourceLines,
                context.declaredVariables
            );
            if (!iterable.ok) {
                context.declaredVariables.erase(forEachHeader.variableName);
                ++context.blockDepth;
                context.pushBlock("for");
                ++context.suppressedBlockDepth;
                context.canAttachElse = false;
                continue;
            }

            if (iterable.type.primitive != PrimitiveType::List || iterable.type.subtypes.size() != 1) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + static_cast<int>(forEachHeader.iterableOffset), "for-in expects a List value", context.sourceLines);
                context.declaredVariables.erase(forEachHeader.variableName);
                ++context.blockDepth;
                context.pushBlock("for");
                ++context.suppressedBlockDepth;
                context.canAttachElse = false;
                continue;
            }

            const Type elementType = iterable.type.subtypes[0];
            if (!isImplicitlyConvertible(elementType, loopVariable->second)) {
                recordSourceError(
                    context.options.inputFile,
                    lineNumber,
                    statementStartColumn + static_cast<int>(forEachHeader.variableOffset),
                    "cannot implicitly convert " + cpppTypeName(elementType) + " to " + cpppTypeName(loopVariable->second),
                    context.sourceLines
                );
                context.declaredVariables.erase(forEachHeader.variableName);
                ++context.blockDepth;
                context.pushBlock("for");
                ++context.suppressedBlockDepth;
                context.canAttachElse = false;
                continue;
            }

            std::string loopDeclaration = stripGeneratedStatement(declarationResult.generatedStatement);
            const size_t initializer = loopDeclaration.find(" = ");
            if (initializer != std::string::npos) {
                loopDeclaration = loopDeclaration.substr(0, initializer);
            }

            context.queueGeneratedLine(indentForDepth(context.blockDepth) + "bool " + breakFlagName + " = true;", lineNumber);
            context.queueGeneratedLine(
                indentForDepth(context.blockDepth) + "for (" + loopDeclaration + " : " + iterable.generatedExpression + ") {" + (hasComment ? " " + commentText : ""),
                lineNumber
            );
            ++context.blockDepth;
            context.pushBlock("for", breakFlagName, {forEachHeader.variableName});
            context.canAttachElse = false;
            continue;
        }

        if (!forEachResult.matched && forResult.matched && !forResult.ok) {
            recordSourceError(
                context.options.inputFile,
                lineNumber,
                statementStartColumn + static_cast<int>(forResult.errorOffset),
                forResult.message,
                context.sourceLines
            );
            ++context.blockDepth;
            context.pushBlock("for");
            ++context.suppressedBlockDepth;
            context.canAttachElse = false;
            continue;
        }

        if (!forEachResult.matched && forResult.matched) {
            const std::string breakFlagName = "__cppp_loop_completed_" + std::to_string(context.loopControlIndex++);
            forHeader = forResult.header;
            std::string generatedInitializer;
            std::string generatedIteration;
            std::vector<std::string> loopScopedNames;
            std::set<std::string> declarationsBefore;
            for (const auto& variable : context.declaredVariables) {
                declarationsBefore.insert(variable.first);
            }
            if (!emitForPart(
                    forHeader.initializer,
                    statementStartColumn + static_cast<int>(forHeader.initializerOffset),
                    true,
                    generatedInitializer)) {
                ++context.blockDepth;
                context.pushBlock("for");
                ++context.suppressedBlockDepth;
                context.canAttachElse = false;
                continue;
            }
            for (const auto& variable : context.declaredVariables) {
                if (declarationsBefore.count(variable.first) == 0) {
                    loopScopedNames.push_back(variable.first);
                }
            }

            std::string generatedCondition = "true";
            if (!forHeader.condition.empty()) {
                const ExpressionEmitResult condition = emitExpression(
                    context.options.inputFile,
                    lineNumber,
                    forHeader.condition,
                    statementStartColumn + static_cast<int>(forHeader.conditionOffset),
                    context.sourceLines,
                    context.declaredVariables
                );
                if (!condition.ok) {
                    context.eraseDeclaredNames(loopScopedNames);
                    ++context.blockDepth;
                    context.pushBlock("for");
                    ++context.suppressedBlockDepth;
                    context.canAttachElse = false;
                    continue;
                }

                if (!isImplicitlyConvertible(condition.type, PrimitiveType::Bool)) {
                    recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + static_cast<int>(forHeader.conditionOffset), "for condition must be bool", context.sourceLines);
                    context.eraseDeclaredNames(loopScopedNames);
                    ++context.blockDepth;
                    context.pushBlock("for");
                    ++context.suppressedBlockDepth;
                    context.canAttachElse = false;
                    continue;
                }

                generatedCondition = condition.generatedExpression;
                if (condition.type != PrimitiveType::Bool) {
                    generatedCondition = castExpressionTo(generatedCondition, condition.type, PrimitiveType::Bool);
                }
            }

            if (!emitForPart(
                    forHeader.iteration,
                    statementStartColumn + static_cast<int>(forHeader.iterationOffset),
                    false,
                    generatedIteration)) {
                context.eraseDeclaredNames(loopScopedNames);
                ++context.blockDepth;
                context.pushBlock("for");
                ++context.suppressedBlockDepth;
                context.canAttachElse = false;
                continue;
            }

            context.queueGeneratedLine(indentForDepth(context.blockDepth) + "bool " + breakFlagName + " = true;", lineNumber);
            context.queueGeneratedLine(indentForDepth(context.blockDepth) + "for (" + generatedInitializer + "; " + generatedCondition + "; " + generatedIteration + ") {" + (hasComment ? " " + commentText : ""), lineNumber);
            ++context.blockDepth;
            context.pushBlock("for", breakFlagName, loopScopedNames);
            context.canAttachElse = false;
            continue;
        }

        const ConditionParseResult repResult = parseConditionHeaderDetailed(statement, "rep", "rep");
        if (repResult.matched && !repResult.ok) {
            recordSourceError(
                context.options.inputFile,
                lineNumber,
                statementStartColumn + static_cast<int>(repResult.errorOffset),
                repResult.message,
                context.sourceLines
            );
            ++context.blockDepth;
            context.pushBlock("rep");
            ++context.suppressedBlockDepth;
            context.canAttachElse = false;
            continue;
        }

        if (repResult.matched) {
            const std::string breakFlagName = "__cppp_loop_completed_" + std::to_string(context.loopControlIndex++);
            const ConditionHeader& header = repResult.header;
            if (header.condition.empty()) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + static_cast<int>(header.conditionOffset), "expected rep count", context.sourceLines);
                ++context.blockDepth;
                context.pushBlock("rep", breakFlagName);
                ++context.suppressedBlockDepth;
                context.canAttachElse = false;
                continue;
            }

            const ExpressionEmitResult count = emitExpression(
                context.options.inputFile,
                lineNumber,
                header.condition,
                statementStartColumn + static_cast<int>(header.conditionOffset),
                context.sourceLines,
                context.declaredVariables
            );
            if (!count.ok) {
                ++context.blockDepth;
                context.pushBlock("rep", breakFlagName);
                ++context.suppressedBlockDepth;
                context.canAttachElse = false;
                continue;
            }

            if (!isRepCountType(count.type)) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + static_cast<int>(header.conditionOffset), "rep count must be numeric", context.sourceLines);
                ++context.blockDepth;
                context.pushBlock("rep", breakFlagName);
                ++context.suppressedBlockDepth;
                context.canAttachElse = false;
                continue;
            }

            context.queueGeneratedLine(indentForDepth(context.blockDepth) + "bool " + breakFlagName + " = true;", lineNumber);
            if (context.options.shouldSubmit) {
                const std::string indexName = "_" + std::to_string(context.repLoopIndex);
                ++context.repLoopIndex;
                context.queueGeneratedLine(
                    indentForDepth(context.blockDepth) +
                    "for (int " + indexName + " = 0; " + indexName + " < " + castExpressionTo(count.generatedExpression, count.type, PrimitiveType::Int) +
                    "; ++" + indexName + ") {" +
                    (hasComment ? " " + commentText : ""),
                    lineNumber
                );
            } else {
                const std::string indexName = "__cppp_rep_" + std::to_string(context.repLoopIndex);
                const std::string limitName = "__cppp_rep_limit_" + std::to_string(context.repLoopIndex);
                ++context.repLoopIndex;
                context.queueGeneratedLine(
                    indentForDepth(context.blockDepth) +
                    "for (long long " + indexName + " = 0, " + limitName + " = " + castExpressionTo(count.generatedExpression, count.type, PrimitiveType::Int) +
                    "; " + indexName + " < " + limitName + "; ++" + indexName + ") {" +
                    (hasComment ? " " + commentText : ""),
                    lineNumber
                );
            }
            ++context.blockDepth;
            context.pushBlock("rep", breakFlagName);
            context.canAttachElse = false;
            continue;
        }

        const ConditionParseResult ifResult = parseConditionHeaderDetailed(statement, "if", "if");
        if (ifResult.matched && !ifResult.ok) {
            recordSourceError(
                context.options.inputFile,
                lineNumber,
                statementStartColumn + static_cast<int>(ifResult.errorOffset),
                ifResult.message,
                context.sourceLines
            );
            ++context.blockDepth;
            context.pushBlock("if");
            ++context.suppressedBlockDepth;
            context.canAttachElse = false;
            continue;
        }

        if (ifResult.matched) {
            emitConditionHeader("if", ifResult.header);
            continue;
        }

        const ConditionParseResult whileResult = parseConditionHeaderDetailed(statement, "while", "while");
        if (whileResult.matched && !whileResult.ok) {
            recordSourceError(
                context.options.inputFile,
                lineNumber,
                statementStartColumn + static_cast<int>(whileResult.errorOffset),
                whileResult.message,
                context.sourceLines
            );
            ++context.blockDepth;
            context.pushBlock("while");
            ++context.suppressedBlockDepth;
            context.canAttachElse = false;
            continue;
        }

        if (whileResult.matched) {
            emitConditionHeader("while", whileResult.header, 0, "__cppp_loop_completed_" + std::to_string(context.loopControlIndex++));
            continue;
        }

        context.canAttachElse = false;

        const bool hasSemicolon = statement.back() == ';';
        if (!hasSemicolon) {
            const int column = static_cast<int>(codeText.find_last_not_of(" \t\r\n")) + 1;
            recordSourceError(context.options.inputFile, lineNumber, column, "missing semicolon", context.sourceLines);
            continue;
        }

        const std::string statementBody = trim(statement.substr(0, statement.size() - 1));
        if (statementBody == "break") {
            const std::string breakFlagName = context.nearestLoopBreakFlag();
            if (breakFlagName.empty()) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "break can only be used inside a loop", context.sourceLines);
                continue;
            }

            context.queueGeneratedLine(indentForDepth(context.blockDepth) + breakFlagName + " = false; break;" + (hasComment ? " " + commentText : ""), lineNumber);
            continue;
        }

        if (statementBody == "continue") {
            if (context.nearestLoopBreakFlag().empty()) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "continue can only be used inside a loop", context.sourceLines);
                continue;
            }

            context.queueGeneratedLine(indentForDepth(context.blockDepth) + "continue;" + (hasComment ? " " + commentText : ""), lineNumber);
            continue;
        }

        const TypeEmitResult typeResult = emitTypeDeclaration(context.options.inputFile, lineNumber, line, statementBody, context.sourceLines, context.declaredVariables);
        if (typeResult.matched) {
            if (!typeResult.ok) {
                continue;
            }

            context.queueGeneratedLine(
                indentGeneratedStatement(typeResult.generatedStatement, context.blockDepth) + (hasComment ? " " + commentText : ""),
                lineNumber,
                typeResult.sourceRanges
            );
            continue;
        }

        const AssignmentEmitResult assignmentResult = emitAssignmentStatement(
            context.options.inputFile,
            lineNumber,
            statementBody,
            context.sourceLines,
            context.declaredVariables,
            !context.options.shouldSubmit
        );
        if (assignmentResult.matched) {
            if (!assignmentResult.ok) {
                continue;
            }

            context.queueGeneratedLine(
                indentGeneratedStatement(assignmentResult.generatedStatement, context.blockDepth) + (hasComment ? " " + commentText : ""),
                lineNumber,
                assignmentResult.sourceRanges
            );
            continue;
        }

        const ListEmitResult listResult = emitListStatement(
            context.options.inputFile,
            lineNumber,
            statementBody,
            context.sourceLines,
            context.declaredVariables,
            !context.options.shouldSubmit
        );
        if (listResult.matched) {
            if (!listResult.ok) {
                continue;
            }

            context.queueGeneratedLine(
                indentGeneratedStatement(listResult.generatedStatement, context.blockDepth) + (hasComment ? " " + commentText : ""),
                lineNumber,
                listResult.sourceRanges
            );
            continue;
        }

        if (statementBody.find("++") != std::string::npos || statementBody.find("--") != std::string::npos) {
            const ExpressionEmitResult expression = emitExpression(
                context.options.inputFile,
                lineNumber,
                statementBody,
                statementStartColumn,
                context.sourceLines,
                context.declaredVariables
            );
            if (!expression.ok) {
                continue;
            }

            context.queueGeneratedLine(indentForDepth(context.blockDepth) + expression.generatedExpression + ";" + (hasComment ? " " + commentText : ""), lineNumber);
            continue;
        }

        if (statementBody.rfind("describe", 0) == 0) {
            const PrintEmitResult describeResult = emitDescribeStatement(context.options.inputFile, lineNumber, line, statementBody, context.sourceLines, context.declaredVariables);
            if (!describeResult.ok) {
                continue;
            }

            context.queueGeneratedLine(
                indentGeneratedStatement(describeResult.generatedStatement, context.blockDepth) + (hasComment ? " " + commentText : ""),
                lineNumber,
                describeResult.sourceRanges
            );
            continue;
        }

        const PrintEmitResult printResult = emitPrintStatement(context.options.inputFile, lineNumber, line, statementBody, context.sourceLines, context.declaredVariables);
        if (!printResult.ok) {
            continue;
        }

        context.queueGeneratedLine(
            indentGeneratedStatement(printResult.generatedStatement, context.blockDepth) + (hasComment ? " " + commentText : ""),
            lineNumber,
            printResult.sourceRanges
        );
    }
}
