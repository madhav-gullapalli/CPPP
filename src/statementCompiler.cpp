/*
 * statementCompiler.cpp
 *
 * Lowers parsed statements into generated C++ code with indentation and helper tracking.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "statementCompiler.h"

#include "assignmentCppp.h"
#include "controlFlow.h"
#include "errors.h"
#include "functions.h"
#include "listsCppp.h"
#include "printCppp.h"
#include "sourceSplitter.h"
#include "statementParser.h"
#include "typesCppp.h"

#include <algorithm>
#include <set>
#include <string>

namespace {
// trim removes surrounding whitespace from a string.
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// indentForDepth implements the indentForDepth behavior for the statementCompiler.cpp module.
std::string indentForDepth(int depth) {
    return std::string(static_cast<size_t>((depth + 1) * 4), ' ');
}

// indentGeneratedStatement implements the indentGeneratedStatement behavior for the statementCompiler.cpp module.
std::string indentGeneratedStatement(const std::string& generatedStatement, int depth) {
    return indentForDepth(depth) + trim(generatedStatement);
}

// stripGeneratedStatement implements the stripGeneratedStatement behavior for the statementCompiler.cpp module.
std::string stripGeneratedStatement(const std::string& generatedStatement) {
    std::string text = trim(generatedStatement);
    if (!text.empty() && text.back() == ';') {
        text.pop_back();
    }

    return text;
}

// isLoopBlockKind returns whether the supplied input satisfies the relevant condition.
bool isLoopBlockKind(const std::string& kind) {
    return kind == "for" || kind == "while" || kind == "rep";
}

// isRepCountType returns whether the supplied input satisfies the relevant condition.
bool isRepCountType(Type type) {
    return type == PrimitiveType::Bool ||
        type == PrimitiveType::Char ||
        type == PrimitiveType::Int ||
        type == PrimitiveType::Float;
}

// isNamedCallStatement returns whether the supplied input satisfies the relevant condition.
bool isNamedCallStatement(const std::vector<Token>& tokens, const std::string& name) {
    return tokens.size() >= 3 &&
        tokens[0].kind == TokenKind::Identifier &&
        tokens[0].text == name &&
        tokens[1].kind == TokenKind::LeftParen;
}

// isBuiltinCallName returns whether the supplied input satisfies the relevant condition.
bool isBuiltinCallName(const std::string& name) {
    return name == "print" ||
        name == "describe" ||
        name == "input" ||
        name == "len" ||
        name == "min" ||
        name == "max" ||
        name == "sum" ||
        name == "abs" ||
        name == "split";
}

bool isUnsupportedBareCallStatement(
    const std::vector<Token>& tokens,
    const std::map<std::string, FunctionSignature>& declaredFunctions
) {
    if (tokens.size() < 4 ||
        tokens[0].kind != TokenKind::Identifier ||
        tokens[1].kind != TokenKind::LeftParen ||
        tokens[tokens.size() - 2].kind != TokenKind::RightParen ||
        tokens.back().kind != TokenKind::EndOfFile) {
        return false;
    }

    return declaredFunctions.count(tokens[0].text) == 0 && !isBuiltinCallName(tokens[0].text);
}

// containsIncrementOrDecrement implements the containsIncrementOrDecrement behavior for the statementCompiler.cpp module.
bool containsIncrementOrDecrement(const std::vector<Token>& tokens) {
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::Operator && (token.text == "++" || token.text == "--")) {
            return true;
        }
    }
    return false;
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

        const int statementStartColumn = fragment.startColumn;
        const StatementParseResult parsed = parseStatementAst(statement, statementStartColumn);

        const size_t functionBrace = statement.find('{');
        if (context.blockDepth == 0 && !context.inFunction && functionBrace != std::string::npos) {
            const ParsedFunctionHeader functionHeader = parseFunctionHeader(
                context.options.inputFile,
                lineNumber,
                trim(statement.substr(0, functionBrace + 1)),
                statementStartColumn,
                context.sourceLines
            );
            if (functionHeader.matched) {
                if (!functionHeader.ok) {
                    ++context.blockDepth;
                    context.pushBlock("suppressed");
                    ++context.suppressedBlockDepth;
                    context.canAttachElse = false;
                    continue;
                }
                if (context.declaredFunctions.count(functionHeader.signature.name) != 0) {
                    recordSourceError(context.options.inputFile, lineNumber, functionHeader.nameColumn, "duplicate function '" + functionHeader.signature.name + "'", context.sourceLines);
                    ++context.blockDepth;
                    context.pushBlock("suppressed");
                    ++context.suppressedBlockDepth;
                    context.canAttachElse = false;
                    continue;
                }
                context.declaredFunctions[functionHeader.signature.name] = functionHeader.signature;
                context.queueFunctionLine(functionHeader.generatedSignature + (hasComment ? " " + commentText : ""), lineNumber);
                context.savedDeclaredVariables = context.declaredVariables;
                context.declaredVariables.clear();
                for (const FunctionParameter& parameter : functionHeader.signature.parameters) {
                    context.declaredVariables[parameter.name] = parameter.type;
                }
                context.currentFunction = functionHeader.signature;
                context.inFunction = true;
                context.outputTarget = OutputTarget::Function;
                ++context.blockDepth;
                context.pushBlock("function");
                context.canAttachElse = false;
                continue;
            }
        }

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
                partColumn,
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

        if (parsed.kind == StatementParseResult::Kind::CloseBrace) {
            const CloseBraceStmt& closeBrace = static_cast<const CloseBraceStmt&>(*parsed.statement);
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

            const std::string afterBrace = closeBrace.trailingText;
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
                if (closedBlock == "function") {
                    context.inFunction = false;
                    context.outputTarget = OutputTarget::Main;
                    context.declaredVariables = context.savedDeclaredVariables;
                    context.savedDeclaredVariables.clear();
                    context.currentFunction = FunctionSignature{};
                    context.canAttachElse = false;
                    context.pendingLoopElse.active = false;
                    context.pendingLoopElse.breakFlagName.clear();
                }
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

        const ConditionParseResult elseIfResult =
            parsed.kind == StatementParseResult::Kind::ElseIf
                ? ConditionParseResult{true, parsed.ok, static_cast<const ElseIfStmt&>(*parsed.statement).header, parsed.errorOffset, parsed.message}
                : ConditionParseResult{};
        if (context.canAttachElse) {
            if (parsed.kind == StatementParseResult::Kind::Else) {
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

        if (parsed.kind == StatementParseResult::Kind::Else) {
            recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "else without matching if", context.sourceLines);
            ++context.blockDepth;
            context.pushBlock("else");
            ++context.suppressedBlockDepth;
            context.pendingLoopElse.active = false;
            context.canAttachElse = false;
            continue;
        }

        context.pendingLoopElse.active = false;

        const ForParseResult forResult =
            parsed.kind == StatementParseResult::Kind::For
                ? ForParseResult{true, parsed.ok, static_cast<const ForStmt&>(*parsed.statement).header, parsed.errorOffset, parsed.message}
                : ForParseResult{};
        const ForEachParseResult forEachResult =
            parsed.kind == StatementParseResult::Kind::ForEach
                ? ForEachParseResult{true, parsed.ok, static_cast<const ForEachStmt&>(*parsed.statement).header, parsed.errorOffset, parsed.message}
                : ForEachParseResult{};
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

            if (!isListType(iterable.type) && !isSetType(iterable.type) && !isMapType(iterable.type) && !isRangeType(iterable.type)) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + static_cast<int>(forEachHeader.iterableOffset), "for-in expects a List value", context.sourceLines);
                context.declaredVariables.erase(forEachHeader.variableName);
                ++context.blockDepth;
                context.pushBlock("for");
                ++context.suppressedBlockDepth;
                context.canAttachElse = false;
                continue;
            }

            const Type elementType = isRangeType(iterable.type)
                ? Type(PrimitiveType::Int)
                : (isMapType(iterable.type)
                ? Type(PrimitiveType::Pair, {iterable.type.subtypes[0], iterable.type.subtypes[1]})
                : iterable.type.subtypes[0]);
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

        const ConditionParseResult repResult =
            parsed.kind == StatementParseResult::Kind::Rep
                ? ConditionParseResult{true, parsed.ok, static_cast<const RepStmt&>(*parsed.statement).header, parsed.errorOffset, parsed.message}
                : ConditionParseResult{};
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

        const ConditionParseResult ifResult =
            parsed.kind == StatementParseResult::Kind::If
                ? ConditionParseResult{true, parsed.ok, static_cast<const IfStmt&>(*parsed.statement).header, parsed.errorOffset, parsed.message}
                : ConditionParseResult{};
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

        const ConditionParseResult whileResult =
            parsed.kind == StatementParseResult::Kind::While
                ? ConditionParseResult{true, parsed.ok, static_cast<const WhileStmt&>(*parsed.statement).header, parsed.errorOffset, parsed.message}
                : ConditionParseResult{};
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
        const std::vector<Token> statementTokens = tokenize(statementBody);
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

        if (statementTokens.size() >= 2 &&
            statementTokens[0].kind == TokenKind::Identifier &&
            statementTokens[0].text == "return") {
            if (!context.inFunction) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "return can only be used inside a function", context.sourceLines);
                continue;
            }

            const std::string returnExpressionText =
                statementBody == "return" ? "" : trim(statementBody.substr(std::string("return").size()));
            const int returnExpressionColumn =
                statementStartColumn + static_cast<int>(statementBody.find("return")) + static_cast<int>(std::string("return").size());

            if (returnExpressionText.empty()) {
                if (!context.currentFunction.returnsVoid) {
                    recordSourceError(
                        context.options.inputFile,
                        lineNumber,
                        statementStartColumn,
                        "non-void function must return a value of type " + cpppTypeName(context.currentFunction.returnType),
                        context.sourceLines
                    );
                    continue;
                }

                context.queueGeneratedLine(indentForDepth(context.blockDepth) + "return;" + (hasComment ? " " + commentText : ""), lineNumber);
                continue;
            }

            if (context.currentFunction.returnsVoid) {
                recordSourceError(context.options.inputFile, lineNumber, returnExpressionColumn + 1, "void function cannot return a value", context.sourceLines);
                continue;
            }

            const ExpressionEmitResult returnExpression = emitExpression(
                context.options.inputFile,
                lineNumber,
                returnExpressionText,
                returnExpressionColumn + 1,
                context.sourceLines,
                context.declaredVariables
            );
            if (!returnExpression.ok) {
                continue;
            }

            if (!isImplicitlyConvertible(returnExpression.type, context.currentFunction.returnType)) {
                recordSourceError(
                    context.options.inputFile,
                    lineNumber,
                    returnExpressionColumn + 1,
                    "cannot implicitly convert " + cpppTypeName(returnExpression.type) + " to " + cpppTypeName(context.currentFunction.returnType),
                    context.sourceLines
                );
                continue;
            }

            std::string generatedReturnExpression = returnExpression.generatedExpression;
            if (returnExpression.type != context.currentFunction.returnType) {
                generatedReturnExpression = castExpressionTo(
                    generatedReturnExpression,
                    returnExpression.type,
                    context.currentFunction.returnType
                );
            }

            context.queueGeneratedLine(
                indentForDepth(context.blockDepth) + "return " + generatedReturnExpression + ";" + (hasComment ? " " + commentText : ""),
                lineNumber
            );
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
            statementStartColumn,
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

        if (isNamedCallStatement(statementTokens, "describe")) {
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

        if (containsIncrementOrDecrement(statementTokens) &&
            !isNamedCallStatement(statementTokens, "print") &&
            !isNamedCallStatement(statementTokens, "describe")) {
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

        const PrintEmitResult printResult = emitPrintStatement(context.options.inputFile, lineNumber, line, statementBody, context.sourceLines, context.declaredVariables);
        if (isNamedCallStatement(statementTokens, "print")) {
            if (!printResult.ok) {
                continue;
            }

            context.queueGeneratedLine(
                indentGeneratedStatement(printResult.generatedStatement, context.blockDepth) + (hasComment ? " " + commentText : ""),
                lineNumber,
                printResult.sourceRanges
            );
            continue;
        }

        if (isUnsupportedBareCallStatement(statementTokens, context.declaredFunctions)) {
            recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "unsupported statement", context.sourceLines);
            continue;
        }

        if (printResult.ok) {
            context.queueGeneratedLine(
                indentGeneratedStatement(printResult.generatedStatement, context.blockDepth) + (hasComment ? " " + commentText : ""),
                lineNumber,
                printResult.sourceRanges
            );
            continue;
        }

        const ExpressionEmitResult expressionStatement = emitExpression(
            context.options.inputFile,
            lineNumber,
            statementBody,
            statementStartColumn,
            context.sourceLines,
            context.declaredVariables
        );
        if (!expressionStatement.ok) {
            continue;
        }

        context.queueGeneratedLine(
            indentForDepth(context.blockDepth) + expressionStatement.generatedExpression + ";" + (hasComment ? " " + commentText : ""),
            lineNumber
        );
    }
}
