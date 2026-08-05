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

bool containsCustomType(const Type& type) {
    if (isStructType(type)) return true;
    for (const Type& subtype : type.subtypes) {
        if (containsCustomType(subtype)) return true;
    }
    return false;
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

std::vector<Token> withoutTrailingSemicolon(const std::vector<Token>& tokens) {
    std::vector<Token> statementTokens;
    statementTokens.reserve(tokens.size());
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::EndOfFile) {
            break;
        }
        statementTokens.push_back(token);
    }
    if (!statementTokens.empty() && statementTokens.back().kind == TokenKind::Semicolon) {
        statementTokens.pop_back();
    }

    Token eof = tokens.empty() ? Token{} : tokens.back();
    eof.kind = TokenKind::EndOfFile;
    eof.text.clear();
    if (!statementTokens.empty()) {
        const Token& last = statementTokens.back();
        eof.span.startLine = last.span.endLine;
        eof.span.endLine = last.span.endLine;
        eof.span.startColumn = last.span.endColumn + 1;
        eof.span.endColumn = eof.span.startColumn;
        eof.span.startOffset = last.span.endOffset;
        eof.span.endOffset = last.span.endOffset;
        eof.sourceSpan = last.sourceSpan.valid()
            ? SourceSpan{last.sourceSpan.source, last.sourceSpan.endOffset, last.sourceSpan.endOffset}
            : SourceSpan{};
    }
    statementTokens.push_back(std::move(eof));
    return statementTokens;
}

std::vector<Token> tokenSlice(
    const std::vector<Token>& tokens,
    size_t startOffset,
    size_t textLength
) {
    std::vector<Token> result;
    const size_t endOffset = startOffset + textLength;
    for (const Token& sourceToken : tokens) {
        if (sourceToken.kind == TokenKind::EndOfFile) {
            break;
        }
        if (sourceToken.span.startOffset < startOffset || sourceToken.span.endOffset > endOffset) {
            continue;
        }
        Token token = sourceToken;
        token.span.startOffset -= startOffset;
        token.span.endOffset -= startOffset;
        token.span.startColumn -= static_cast<int>(startOffset);
        token.span.endColumn -= static_cast<int>(startOffset);
        result.push_back(std::move(token));
    }
    return result;
}

std::string tokenText(const std::vector<Token>& tokens) {
    if (tokens.empty()) return "";
    std::string result;
    size_t previousEnd = tokens.front().span.startOffset;
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::EndOfFile) break;
        if (token.span.startOffset > previousEnd) {
            result.append(token.span.startOffset - previousEnd, ' ');
        }
        result += token.text;
        previousEnd = token.span.endOffset;
    }
    return result;
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
        name == "split" ||
        name == "copy" ||
        name == "range";
}

bool isUnsupportedBareCallStatement(
    const std::vector<Token>& tokens,
    const std::map<std::string, Type>& declaredVariables,
    const std::map<std::string, FunctionSignature>& declaredFunctions,
    const std::map<std::string, std::map<std::string, Type>>& declaredStructs
) {
    if (tokens.size() < 4 ||
        tokens[0].kind != TokenKind::Identifier ||
        tokens[1].kind != TokenKind::LeftParen ||
        tokens[tokens.size() - 2].kind != TokenKind::RightParen ||
        tokens.back().kind != TokenKind::EndOfFile) {
        return false;
    }

    const auto variable = declaredVariables.find(tokens[0].text);
    return (variable == declaredVariables.end() || !isFunctionType(variable->second)) &&
        declaredFunctions.count(tokens[0].text) == 0 &&
        declaredStructs.count(tokens[0].text) == 0 &&
        !isBuiltinCallName(tokens[0].text);
}

bool recordContextualStatementSuggestion(
    CompileContext& context,
    const std::vector<Token>& tokens,
    bool blockHeaderOnly
) {
    if (tokens.empty() ||
        tokens[0].kind != TokenKind::Identifier ||
        !tokens[0].sourceSpan.valid()) {
        return false;
    }

    const auto functionVariable = context.declaredVariables.find(tokens[0].text);
    if (functionVariable != context.declaredVariables.end() && isFunctionType(functionVariable->second)) {
        return false;
    }

    std::vector<std::string> candidates;
    if (blockHeaderOnly) {
        candidates = {"if", "while", "for", "rep", "class", "struct"};
    } else if (tokens.size() >= 2 && tokens[1].kind == TokenKind::LeftParen) {
        candidates = {
            "print", "describe", "input", "len", "min", "max", "sum",
            "abs", "copy", "range"
        };
        for (const auto& function : context.declaredFunctions) {
            candidates.push_back(function.first);
        }
        for (const auto& structure : context.declaredStructs) {
            candidates.push_back(structure.first);
        }
    } else if (tokens.size() >= 2 &&
               (tokens[1].kind == TokenKind::Identifier ||
                (tokens[1].kind == TokenKind::Operator &&
                 tokens[1].text == "<"))) {
        candidates = {
            "bool", "char", "int", "float", "string", "List", "Set",
            "Map", "Pair", "Stack", "Queue", "Deque", "return"
        };
        for (const auto& structure : context.declaredStructs) {
            candidates.push_back(structure.first);
        }
    } else {
        return false;
    }

    const std::string closest = closestDiagnosticCandidate(
        tokens[0].text,
        candidates
    );
    if (closest.empty()) {
        return false;
    }

    Diagnostic diagnostic;
    diagnostic.message = "unsupported statement";
    diagnostic.labels.push_back({tokens[0].sourceSpan, "", true});
    diagnostic.suggestions.push_back({
        tokens[0].sourceSpan,
        closest,
        "did you mean '" + closest + "'?",
        SuggestionApplicability::MaybeIncorrect
    });
    recordDiagnostic(std::move(diagnostic));
    return true;
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

bool needsCharRuntimeHelperForType(const Type& type) {
    if (type == PrimitiveType::Char) {
        return true;
    }

    for (const Type& subtype : type.subtypes) {
        if (needsCharRuntimeHelperForType(subtype)) {
            return true;
        }
    }

    return false;
}

bool needsRangeRuntimeHelperForType(const Type& type) {
    if (type == PrimitiveType::Range) {
        return true;
    }

    for (const Type& subtype : type.subtypes) {
        if (needsRangeRuntimeHelperForType(subtype)) {
            return true;
        }
    }

    return false;
}
}

void compileTokenStream(CompileContext& context, const TokenStream& tokenStream) {
    setDeclaredStructsForExpressions(&context.declaredStructs);
    setDeclaredClassNamesForExpressions(&context.declaredClassNames);
    setDeclaredStructFieldOrdersForExpressions(&context.declaredStructFieldOrders);
    setDeclaredStructMethodsForExpressions(&context.declaredStructMethods);
    const std::vector<SourceFragment> sourceFragments = splitTokenStream(tokenStream);
    for (const SourceFragment& fragment : sourceFragments) {
        const std::string requirementOwner = !context.currentStructMethodName.empty()
            ? "method:" + context.currentStructName + "." + context.currentStructMethodName
            : (!context.currentStructName.empty()
                ? "struct:" + context.currentStructName
                : (context.currentTopLevelFunctionName.empty() ? "" : "function:" + context.currentTopLevelFunctionName));
        setRuntimeRequirementOwner(requirementOwner);
        const int lineNumber = fragment.lineNumber;
        const std::string& commentText = fragment.commentText;
        const bool hasComment = !commentText.empty();
        const std::string statement = trim(fragment.codeText);

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
        const std::vector<Token>& lexicalTokens = fragment.tokens;
        bool hasLexicalError = false;
        for (const Token& token : lexicalTokens) {
            if (token.kind == TokenKind::Unknown) {
                Diagnostic diagnostic;
                diagnostic.message = "unrecognized token '" + token.text + "'";
                diagnostic.labels.push_back({token.sourceSpan, "", true});
                diagnostic.helps.push_back("remove it or replace it with a valid CP++ token");
                recordDiagnostic(std::move(diagnostic));
                hasLexicalError = true;
                break;
            }
            if ((token.kind != TokenKind::String && token.kind != TokenKind::Char) ||
                (token.text.size() >= 2 && token.text.front() == token.text.back())) {
                continue;
            }

            const bool printString =
                token.kind == TokenKind::String &&
                lexicalTokens.size() >= 2 &&
                lexicalTokens[0].kind == TokenKind::Identifier &&
                lexicalTokens[0].text == "print";
            const char quote = token.kind == TokenKind::Char ? '\'' : '"';
            Diagnostic diagnostic;
            diagnostic.message = printString
                ? "unterminated string literal in print"
                : (token.kind == TokenKind::Char
                    ? "unterminated char literal"
                    : "unterminated string literal");
            diagnostic.labels.push_back({token.sourceSpan, "", true});
            const SourceSpan insertion = {
                token.sourceSpan.source,
                token.sourceSpan.endOffset,
                token.sourceSpan.endOffset
            };
            diagnostic.suggestions.push_back({
                insertion,
                std::string(1, quote),
                std::string("add a closing `") + quote + "`",
                SuggestionApplicability::MachineApplicable
            });
            recordDiagnostic(std::move(diagnostic));
            hasLexicalError = true;
            break;
        }
        if (hasLexicalError) {
            context.canAttachElse = false;
            continue;
        }

        StatementParseResult parsed = parseStatementAst(lexicalTokens, statementStartColumn);
        if (parsed.statement) {
            parsed.statement->sourceSpan = fragment.sourceSpan;
        }

        if (!context.inStruct && context.blockDepth == 0) {
            const std::vector<Token>& structTokens = lexicalTokens;
            if (structTokens.size() >= 4 && structTokens[0].kind == TokenKind::Identifier &&
                (structTokens[0].text == "struct" || structTokens[0].text == "class")) {
                const bool isClass = structTokens[0].text == "class";
                const std::string kind = isClass ? "class" : "struct";
                if (structTokens[1].kind != TokenKind::Identifier || structTokens[2].text != "{") {
                    recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, kind + " declarations use syntax " + kind + " Name {", context.sourceLines);
                    ++context.blockDepth;
                    context.pushBlock("suppressed");
                    ++context.suppressedBlockDepth;
                    continue;
                }
                const std::string structName = structTokens[1].text;
                if (context.declaredStructs.count(structName) != 0 || declaredTypeForName(structName) != PrimitiveType::Unknown) {
                    recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + structTokens[1].span.startColumn - 1, kind + " '" + structName + "' is already declared", context.sourceLines);
                    ++context.blockDepth;
                    context.pushBlock("suppressed");
                    ++context.suppressedBlockDepth;
                    continue;
                }
                context.declaredStructs[structName] = {};
                context.declaredStructFieldOrders[structName] = {};
                context.declaredStructMethods[structName] = {};
                if (isClass) context.declaredClassNames.insert(structName);
                context.inStruct = true;
                context.currentStructIsClass = isClass;
                context.currentStructName = structName;
                context.currentStructFields.clear();
                ++context.blockDepth;
                context.pushBlock("struct");
                context.queueTopLevelLine("struct " + structName + " {");
                continue;
            }
        }

        if (context.inStruct && !context.inFunction && parsed.kind == StatementParseResult::Kind::CloseBrace) {
            const std::vector<Token>& trailingTokens =
                static_cast<const CloseBraceStmt&>(*parsed.statement).trailingTokens;
            if (!trailingTokens.empty() && trailingTokens[0].kind != TokenKind::EndOfFile) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "unexpected text after " + std::string(context.currentStructIsClass ? "class" : "struct") + " closing brace", context.sourceLines);
            }
            if (!context.currentStructFields.empty()) {
                std::string constructor = "    " + context.currentStructName + "(";
                size_t i = 0;
                for (const std::string& fieldName : context.declaredStructFieldOrders[context.currentStructName]) {
                    if (i > 0) {
                        constructor += ", ";
                    }
                    constructor += cppTypeForType(context.declaredStructs[context.currentStructName][fieldName]) + " value_" + fieldName;
                    ++i;
                }
                constructor += ") : ";
                i = 0;
                for (const std::string& fieldName : context.declaredStructFieldOrders[context.currentStructName]) {
                    if (i > 0) {
                        constructor += ", ";
                    }
                    constructor += fieldName + "(std::move(value_" + fieldName + "))";
                    ++i;
                }
                constructor += " {}";
                context.queueTopLevelLine(constructor, lineNumber);
            }
            const auto& fields = context.declaredStructs[context.currentStructName];
            const auto& fieldOrder = context.declaredStructFieldOrders[context.currentStructName];
            if (context.currentStructIsClass) {
                context.queueTopLevelLine("    " + context.currentStructName + "(const " + context.currentStructName + "& other) {", lineNumber);
                for (const std::string& fieldName : fieldOrder) {
                    requireCopyHelpersForType(fields.at(fieldName));
                    context.queueTopLevelLine("        " + fieldName + " = CPPPCopy(other." + fieldName + ");", lineNumber);
                }
                context.queueTopLevelLine("    }", lineNumber);
                context.queueTopLevelLine("    " + context.currentStructName + "& operator=(const " + context.currentStructName + "& other) {", lineNumber);
                context.queueTopLevelLine("        if (this == &other) return *this;", lineNumber);
                for (const std::string& fieldName : fieldOrder) {
                    context.queueTopLevelLine("        " + fieldName + " = other." + fieldName + ";", lineNumber);
                }
                context.queueTopLevelLine("        return *this;", lineNumber);
                context.queueTopLevelLine("    }", lineNumber);
            } else {
                context.queueTopLevelLine("    " + context.currentStructName + "() = default;", lineNumber);
            }
            std::string equal = "    bool operator==(const " + context.currentStructName + "& other) const { return ";
            if (fields.empty()) {
                equal += "true";
            }
            size_t fieldIndex = 0;
            for (const std::string& fieldName : fieldOrder) {
                const Type& fieldType = fields.at(fieldName);
                if (fieldIndex++ > 0) {
                    equal += " && ";
                }
                if (isClassType(fieldType)) {
                    equal += "((" + fieldName + " && other." + fieldName + ") ? (*" + fieldName + " == *other." + fieldName + ") : (!" + fieldName + " && !other." + fieldName + "))";
                } else {
                    if (isCollectionType(fieldType) || isPairType(fieldType)) {
                        requireContainerMember(fieldType, "compare_eq");
                    }
                    if (isHeapType(fieldType)) {
                        requireContainerMember(fieldType, "to_list");
                        requireContainerMember(Type(PrimitiveType::List, fieldType.subtypes), "compare_eq");
                    }
                    equal += fieldName + " == other." + fieldName;
                }
            }
            equal += "; }";
            context.queueTopLevelLine(equal, lineNumber);
            for (const std::string& fieldName : fieldOrder) {
                requirePrintHelpersForType(fields.at(fieldName));
            }
            context.queueTopLevelLine("    friend ostream& operator<<(ostream& output, const " + context.currentStructName + "& value) {", lineNumber);
            context.queueTopLevelLine("        output << '{';", lineNumber);
            fieldIndex = 0;
            for (const std::string& fieldName : fieldOrder) {
                if (fieldIndex++ > 0) {
                    context.queueTopLevelLine("        output << \", \";", lineNumber);
                }
                context.queueTopLevelLine("        output << \"" + fieldName + ": \"; CPPPPrintValue(output, value." + fieldName + ");", lineNumber);
            }
            context.queueTopLevelLine("        return output << '}';", lineNumber);
            context.queueTopLevelLine("    }", lineNumber);
            context.queueTopLevelLine("};", lineNumber);
            if (!context.currentStructIsClass) {
                for (const std::string& fieldName : fieldOrder) {
                    requireCopyHelpersForType(fields.at(fieldName));
                }
                std::string copier = "template <> struct CPPPDeepCopier<" + context.currentStructName + "> { static " + context.currentStructName + " run(const " + context.currentStructName + "& value) { return " + context.currentStructName + "(";
                for (size_t index = 0; index < fieldOrder.size(); ++index) {
                    if (index > 0) copier += ", ";
                    copier += "CPPPCopy(value." + fieldOrder[index] + ")";
                }
                copier += "); } };";
                context.queueTopLevelLine(copier, lineNumber);
            }
            context.inStruct = false;
            context.currentStructIsClass = false;
            context.currentStructName.clear();
            context.currentStructFields.clear();
            context.outputTarget = OutputTarget::Main;
            --context.blockDepth;
            if (!context.blockKinds.empty()) {
                context.blockKinds.pop_back();
                context.blockBreakFlags.pop_back();
                context.blockDeclaredNames.pop_back();
            }
            continue;
        }

        if (context.inStruct && !context.inFunction) {
            const size_t methodBrace = statement.find('{');
            if (!context.inFunction && methodBrace != std::string::npos) {
                const ParsedFunctionHeader methodHeader = parseFunctionHeader(
                    context.options.inputFile,
                    lineNumber,
                    statementStartColumn,
                    context.sourceLines,
                    lexicalTokens
                );
                if (methodHeader.matched && methodHeader.ok) {
                    if (context.declaredStructMethods[context.currentStructName].count(methodHeader.signature.name) != 0) {
                        recordSourceError(context.options.inputFile, lineNumber, methodHeader.nameColumn, "duplicate method '" + methodHeader.signature.name + "'", context.sourceLines);
                        continue;
                    }
                    context.declaredStructMethods[context.currentStructName][methodHeader.signature.name] = methodHeader.signature;
                    context.currentStructMethodName = methodHeader.signature.name;
                    context.queueTopLevelLine("    " + methodHeader.generatedSignature, lineNumber);
                    context.savedDeclaredVariables = context.declaredVariables;
                    context.declaredVariables = context.declaredStructs[context.currentStructName];
                    for (const FunctionParameter& parameter : methodHeader.signature.parameters) {
                        context.declaredVariables[parameter.name] = parameter.type;
                    }
                    context.currentFunction = methodHeader.signature;
                    context.inFunction = true;
                    context.outputTarget = OutputTarget::TopLevel;
                    ++context.blockDepth;
                    context.pushBlock("function");
                    continue;
                }
            }
            std::map<std::string, Type> fieldNames;
            const std::vector<Token> fieldTokens = withoutTrailingSemicolon(lexicalTokens);
            const TypeEmitResult fieldResult = emitTypeDeclaration(
                context.options.inputFile,
                lineNumber,
                statementStartColumn,
                context.sourceLines,
                fieldNames,
                fieldTokens
            );
            if (fieldResult.matched) {
                if (!fieldResult.ok) {
                    continue;
                }
                bool fieldsAccepted = true;
                for (const auto& field : fieldNames) {
                    if (context.declaredStructs[context.currentStructName].count(field.first) != 0) {
                        recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "field '" + field.first + "' is already declared", context.sourceLines);
                        fieldsAccepted = false;
                        continue;
                    }
                    if (!context.currentStructIsClass && containsCustomType(field.second)) {
                        recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "struct fields cannot contain custom types; use a class for recursive or nested custom values", context.sourceLines);
                        fieldsAccepted = false;
                        continue;
                    }
                    context.declaredStructs[context.currentStructName][field.first] = field.second;
                    context.currentStructFields.push_back(field.first);
                    context.declaredStructFieldOrders[context.currentStructName].push_back(field.first);
                }
                if (fieldsAccepted) {
                    context.queueTopLevelLine("    " + trim(fieldResult.generatedStatement), lineNumber);
                }
                continue;
            }
            recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "struct bodies currently require typed fields", context.sourceLines);
            continue;
        }

        const size_t functionBrace = statement.find('{');
        if (context.blockDepth == 0 && !context.inFunction && functionBrace != std::string::npos) {
            const ParsedFunctionHeader functionHeader = parseFunctionHeader(
                context.options.inputFile,
                lineNumber,
                statementStartColumn,
                context.sourceLines,
                lexicalTokens
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
                context.currentTopLevelFunctionName = functionHeader.signature.name;
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
            if (header.conditionTokens.empty()) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + static_cast<int>(conditionOffset), "expected condition", context.sourceLines);
                ++context.blockDepth;
                context.pushBlock(keyword, breakFlag);
                context.canAttachElse = false;
                return false;
            }

            const ExpressionEmitResult condition = emitExpression(
                context.options.inputFile,
                lineNumber,
                header.conditionTokens,
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

        const auto emitForPart = [&](const std::vector<Token>& partTokens, int partColumn, bool allowDeclaration, std::string& generatedPart) {
            generatedPart.clear();
            if (partTokens.empty()) {
                return true;
            }
            const std::string part = tokenText(partTokens);

            if (allowDeclaration) {
                const TypeEmitResult typeResult = emitTypeDeclaration(context.options.inputFile, lineNumber, partColumn, context.sourceLines, context.declaredVariables, partTokens);
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
                context.sourceLines,
                context.declaredVariables,
                context.declaredFunctions,
                !context.options.shouldSubmit,
                partTokens
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
                    partTokens,
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

            const std::vector<Token>& afterBraceTokens = closeBrace.trailingTokens;
            const bool hasAfterBrace = !afterBraceTokens.empty() && afterBraceTokens[0].kind != TokenKind::EndOfFile;
            if (closingSuppressed) {
                if (hasAfterBrace && afterBraceTokens[afterBraceTokens.size() - 2].kind == TokenKind::LeftBrace) {
                    ++context.blockDepth;
                    context.pushBlock("suppressed");
                    ++context.suppressedBlockDepth;
                }
                continue;
            }
            if (!hasAfterBrace) {
                context.queueGeneratedLine(indentForDepth(context.blockDepth) + "}" + (hasComment ? " " + commentText : ""), lineNumber);
                if (closedBlock == "function") {
                    context.inFunction = false;
                    context.outputTarget = context.inStruct ? OutputTarget::TopLevel : OutputTarget::Main;
                    context.declaredVariables = context.savedDeclaredVariables;
                    context.savedDeclaredVariables.clear();
                    context.currentFunction = FunctionSignature{};
                    if (context.inStruct) {
                        context.currentStructMethodName.clear();
                    } else {
                        context.currentTopLevelFunctionName.clear();
                    }
                    context.canAttachElse = false;
                    context.pendingLoopElse.active = false;
                    context.pendingLoopElse.breakFlagName.clear();
                }
                continue;
            }

            if (!context.canAttachElse) {
                if (context.pendingLoopElse.active && parseNobreakHeader(afterBraceTokens)) {
                    context.queueGeneratedLine(indentForDepth(context.blockDepth) + "} if (" + context.pendingLoopElse.breakFlagName + ") {" + (hasComment ? " " + commentText : ""), lineNumber);
                    ++context.blockDepth;
                    context.pushBlock("loop nobreak");
                    context.pendingLoopElse.active = false;
                    continue;
                }

                const bool trailingNobreak = parseNobreakHeader(afterBraceTokens);
                recordSourceError(
                    context.options.inputFile,
                    lineNumber,
                    statementStartColumn + 1,
                    trailingNobreak ? "nobreak without matching loop" : "else without matching if",
                    context.sourceLines
                );
                if (parseElseHeader(afterBraceTokens) || trailingNobreak) {
                    ++context.blockDepth;
                    context.pushBlock(trailingNobreak ? "loop nobreak" : "else");
                } else {
                    const ConditionParseResult recovery = parseElseIfHeaderDetailed(afterBraceTokens);
                    if (recovery.matched && recovery.ok) {
                        ++context.blockDepth;
                        context.pushBlock("else if");
                    }
                }
                context.pendingLoopElse.active = false;
                continue;
            }

            if (parseElseHeader(afterBraceTokens)) {
                context.queueGeneratedLine(indentForDepth(context.blockDepth) + "} else {" + (hasComment ? " " + commentText : ""), lineNumber);
                ++context.blockDepth;
                context.pushBlock("else");
                context.canAttachElse = false;
                context.pendingLoopElse.active = false;
                continue;
            }

            const ConditionParseResult trailingElseIf = parseElseIfHeaderDetailed(afterBraceTokens);
            if (trailingElseIf.matched && trailingElseIf.ok) {
                const ConditionHeader& header = trailingElseIf.header;
                if (header.conditionTokens.empty()) {
                    recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + static_cast<int>(header.conditionOffset), "expected condition", context.sourceLines);
                    ++context.blockDepth;
                    context.pushBlock("else if");
                    context.canAttachElse = false;
                    context.pendingLoopElse.active = false;
                    continue;
                }

                const ExpressionEmitResult condition = emitExpression(
                    context.options.inputFile,
                    lineNumber,
                    header.conditionTokens,
                    statementStartColumn + static_cast<int>(header.conditionOffset),
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
                    recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + static_cast<int>(header.conditionOffset), "else if condition must be bool", context.sourceLines);
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
            if (parseElseHeader(afterBraceTokens)) {
                ++context.blockDepth;
                context.pushBlock("else");
            }
            context.pendingLoopElse.active = false;
            continue;
        }

        if (context.pendingLoopElse.active) {
            if (parseNobreakHeader(lexicalTokens)) {
                context.queueGeneratedLine(indentForDepth(context.blockDepth) + "if (" + context.pendingLoopElse.breakFlagName + ") {" + (hasComment ? " " + commentText : ""), lineNumber);
                ++context.blockDepth;
                context.pushBlock("loop nobreak");
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

        if (parsed.kind == StatementParseResult::Kind::Nobreak) {
            recordSourceError(context.options.inputFile, lineNumber, statementStartColumn, "nobreak without matching loop", context.sourceLines);
            ++context.blockDepth;
            context.pushBlock("loop nobreak");
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
            const ExpressionEmitResult iterable = emitExpression(
                context.options.inputFile,
                lineNumber,
                forEachHeader.iterableTokens,
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

            std::string loopDeclaration;
            Type loopVariableType;
            if (forEachHeader.usesVar) {
                if (context.declaredVariables.count(forEachHeader.variableName) != 0) {
                    recordSourceError(
                        context.options.inputFile,
                        lineNumber,
                        statementStartColumn + static_cast<int>(forEachHeader.variableOffset),
                        "variable '" + forEachHeader.variableName + "' is already declared",
                        context.sourceLines
                    );
                    ++context.blockDepth;
                    context.pushBlock("for");
                    ++context.suppressedBlockDepth;
                    context.canAttachElse = false;
                    continue;
                }
            } else {
                const TypeEmitResult declarationResult = emitTypeDeclaration(
                    context.options.inputFile,
                    lineNumber,
                    statementStartColumn + static_cast<int>(forEachHeader.variableOffset),
                    context.sourceLines,
                    context.declaredVariables,
                    forEachHeader.declarationTokens
                );
                if (!declarationResult.matched || !declarationResult.ok) {
                    ++context.blockDepth;
                    context.pushBlock("for");
                    ++context.suppressedBlockDepth;
                    context.canAttachElse = false;
                    continue;
                }

                const auto loopVariable = context.declaredVariables.find(forEachHeader.variableName);
                loopVariableType = loopVariable->second;
                loopDeclaration = stripGeneratedStatement(declarationResult.generatedStatement);
                const size_t initializer = loopDeclaration.find(" = ");
                if (initializer != std::string::npos) {
                    loopDeclaration = loopDeclaration.substr(0, initializer);
                }
            }

            if (!isListType(iterable.type) && !isSetType(iterable.type) && !isMapType(iterable.type) && !isRangeType(iterable.type)) {
                recordSourceError(context.options.inputFile, lineNumber, statementStartColumn + static_cast<int>(forEachHeader.iterableOffset), "for-in expects a List value", context.sourceLines);
                if (!forEachHeader.usesVar) {
                    context.declaredVariables.erase(forEachHeader.variableName);
                }
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
            if (!isRangeType(iterable.type)) {
                requireContainerMember(iterable.type, "begin_mut");
                requireContainerMember(iterable.type, "end_mut");
            }
            if (forEachHeader.usesVar) {
                loopVariableType = elementType;
                loopDeclaration = cppTypeForType(loopVariableType) + " " + forEachHeader.variableName;
                context.declaredVariables[forEachHeader.variableName] = loopVariableType;
                if (needsCharRuntimeHelperForType(loopVariableType)) {
                    requireRuntimeHelper("CPPPCharType");
                }
                if (needsRangeRuntimeHelperForType(loopVariableType)) {
                    requireRuntimeHelper("CPPPRangeType");
                }
            }

            if (!isImplicitlyConvertible(elementType, loopVariableType)) {
                recordSourceError(
                    context.options.inputFile,
                    lineNumber,
                    statementStartColumn + static_cast<int>(forEachHeader.variableOffset),
                    "cannot implicitly convert " + cpppTypeName(elementType) + " to " + cpppTypeName(loopVariableType),
                    context.sourceLines
                );
                context.declaredVariables.erase(forEachHeader.variableName);
                ++context.blockDepth;
                context.pushBlock("for");
                ++context.suppressedBlockDepth;
                context.canAttachElse = false;
                continue;
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
                    forHeader.initializerTokens,
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
            if (!forHeader.conditionTokens.empty()) {
                const ExpressionEmitResult condition = emitExpression(
                    context.options.inputFile,
                    lineNumber,
                    forHeader.conditionTokens,
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
                    forHeader.iterationTokens,
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
            if (header.conditionTokens.empty()) {
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
                header.conditionTokens,
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

        if (!statement.empty() &&
            statement.back() == '{' &&
            recordContextualStatementSuggestion(context, lexicalTokens, true)) {
            continue;
        }

        const bool hasSemicolon = statement.back() == ';';
        if (!hasSemicolon) {
            const int insertionColumn =
                statementStartColumn + static_cast<int>(statement.size());
            const SourceSpan insertion = sourceInsertionSpan(
                context.options.inputFile,
                context.sourceLines,
                lineNumber,
                insertionColumn
            );
            Diagnostic diagnostic;
            diagnostic.message = "missing semicolon";
            diagnostic.labels.push_back({insertion, "statement ends here", true});
            diagnostic.suggestions.push_back({
                insertion,
                ";",
                "add `;` to terminate the statement",
                SuggestionApplicability::MachineApplicable
            });
            recordDiagnostic(std::move(diagnostic));
            continue;
        }

        const std::string statementBody = trim(statement.substr(0, statement.size() - 1));
        const std::vector<Token> statementTokens = withoutTrailingSemicolon(lexicalTokens);
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
                tokenSlice(
                    lexicalTokens,
                    static_cast<size_t>(returnExpressionColumn + 1 - statementStartColumn),
                    returnExpressionText.size()
                ),
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

        const std::map<std::string, Type> declarationsBefore = context.declaredVariables;
        const TypeEmitResult typeResult = emitTypeDeclaration(
            context.options.inputFile,
            lineNumber,
            statementStartColumn,
            context.sourceLines,
            context.declaredVariables,
            statementTokens
        );
        if (typeResult.matched) {
            // Invalid declarations reserve their names to suppress cascaded errors.  They
            // still belong to the enclosing lexical block and must leave scope with it.
            if (!context.blockDeclaredNames.empty()) {
                for (const auto& variable : context.declaredVariables) {
                    if (declarationsBefore.count(variable.first) == 0) {
                        context.blockDeclaredNames.back().push_back(variable.first);
                    }
                }
            }
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
            context.sourceLines,
            context.declaredVariables,
            context.declaredFunctions,
            !context.options.shouldSubmit,
            statementTokens
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
            context.sourceLines,
            context.declaredVariables,
            !context.options.shouldSubmit,
            statementTokens
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
            const PrintEmitResult describeResult = emitDescribeStatement(
                context.options.inputFile,
                lineNumber,
                statementStartColumn,
                context.sourceLines,
                context.declaredVariables,
                statementTokens
            );
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
                statementTokens,
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

        const PrintEmitResult printResult = emitPrintStatement(
            context.options.inputFile,
            lineNumber,
            statementStartColumn,
            context.sourceLines,
            context.declaredVariables,
            statementTokens
        );
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

        if (recordContextualStatementSuggestion(context, lexicalTokens, false)) {
            continue;
        }

        if (isUnsupportedBareCallStatement(
                statementTokens,
                context.declaredVariables,
                context.declaredFunctions,
                context.declaredStructs)) {
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
            statementTokens,
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
