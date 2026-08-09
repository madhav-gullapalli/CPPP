/*
 * printCppp.cpp
 *
 * Parses and lowers print statements, including argument splitting and string emission.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "printCppp.h"

#include "exprAst.h"
#include "expressions.h"
#include "tokenizer.h"
#include "typesCppp.h"

#include <algorithm>

namespace {
// CallArgumentAst implements the CallArgumentAst behavior for the printCppp.cpp module.
struct CallArgumentAst {
    enum class Kind {
        Positional,
        Named
    };

    Kind kind = Kind::Positional;
    std::string text;
    int column = 0;
    std::string name;
    int nameColumn = 0;
    std::string valueText;
    std::vector<Token> valueTokens;
    int valueColumn = 0;
    std::unique_ptr<Expr> valueAst;
};

// RawArgumentSegment implements the RawArgumentSegment behavior for the printCppp.cpp module.
struct RawArgumentSegment {
    std::string text;
    int column;
    std::vector<Token> tokens;
};

// trim removes surrounding whitespace from a string.
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// escapeForCppStringLiteral implements the escapeForCppStringLiteral behavior for the printCppp.cpp module.
std::string escapeForCppStringLiteral(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}

// splitCallArguments splits the input into smaller logical pieces.
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

std::string tokenText(const std::vector<Token>& tokens) {
    if (tokens.empty()) return "";
    std::string result;
    size_t previousEnd = tokens.front().span.startOffset;
    for (const Token& token : tokens) {
        if (token.span.startOffset > previousEnd) {
            result.append(token.span.startOffset - previousEnd, ' ');
        }
        result += token.text;
        previousEnd = token.span.endOffset;
    }
    return result;
}

std::vector<RawArgumentSegment> splitCallArguments(
    const std::vector<Token>& tokens,
    size_t begin,
    size_t end,
    int statementStartColumn
) {
    std::vector<RawArgumentSegment> arguments;
    int parenDepth = 0;
    int bracketDepth = 0;
    size_t argumentStartTokenIndex = begin;
    bool endedWithTopLevelComma = false;

    for (size_t tokenIndex = begin; tokenIndex <= end; ++tokenIndex) {
        const bool atEnd = tokenIndex == end;
        const Token& token = atEnd ? tokens[end] : tokens[tokenIndex];
        if (atEnd) {
            if (argumentStartTokenIndex < tokenIndex) {
                const Token& firstToken = tokens[argumentStartTokenIndex];
                const std::vector<Token> argumentTokens = tokenRange(tokens, argumentStartTokenIndex, tokenIndex);
                arguments.push_back({
                    tokenText(argumentTokens),
                    statementStartColumn + firstToken.span.startColumn - 1,
                    argumentTokens
                });
            } else if (endedWithTopLevelComma) {
                arguments.push_back({"", statementStartColumn + token.span.startColumn - 1, {}});
            }
            break;
        }

        if (token.kind == TokenKind::LeftParen) {
            ++parenDepth;
        } else if (token.kind == TokenKind::RightParen && parenDepth > 0) {
            --parenDepth;
        } else if (token.kind == TokenKind::LeftBracket) {
            ++bracketDepth;
        } else if (token.kind == TokenKind::RightBracket && bracketDepth > 0) {
            --bracketDepth;
        }

        if (token.kind == TokenKind::Comma && parenDepth == 0 && bracketDepth == 0) {
            if (argumentStartTokenIndex < tokenIndex) {
                const Token& firstToken = tokens[argumentStartTokenIndex];
                const std::vector<Token> argumentTokens = tokenRange(tokens, argumentStartTokenIndex, tokenIndex);
                arguments.push_back({
                    tokenText(argumentTokens),
                    statementStartColumn + firstToken.span.startColumn - 1,
                    argumentTokens
                });
            } else {
                arguments.push_back({"", statementStartColumn + token.span.startColumn - 1, {}});
            }
            argumentStartTokenIndex = tokenIndex + 1;
            endedWithTopLevelComma = true;
            continue;
        }

        endedWithTopLevelComma = false;
    }
    return arguments;
}

// isUnterminatedQuotedToken returns whether the supplied input satisfies the relevant condition.
bool isUnterminatedQuotedToken(const Token& token) {
    if (token.kind != TokenKind::String && token.kind != TokenKind::Char) {
        return false;
    }

    return token.text.size() < 2 || token.text.front() != token.text.back();
}

// looksLikeMissingPrintComma implements the looksLikeMissingPrintComma behavior for the printCppp.cpp module.
bool looksLikeMissingPrintComma(const RawArgumentSegment& argument) {
    if (argument.tokens.size() < 2) {
        return false;
    }

    for (const Token& token : argument.tokens) {
        if (token.kind == TokenKind::Identifier && token.text == "in") {
            return false;
        }
        if (token.kind == TokenKind::Operator ||
            token.kind == TokenKind::LeftParen ||
            token.kind == TokenKind::RightParen ||
            token.kind == TokenKind::LeftBracket ||
            token.kind == TokenKind::RightBracket ||
            token.kind == TokenKind::Equals) {
            return false;
        }
    }

    return true;
}

bool parseCallArgumentAst(
    const std::string& inputFile,
    int lineNumber,
    const RawArgumentSegment& segment,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    CallArgumentAst& argument
) {
    argument.text = segment.text;
    argument.column = segment.column;
    if (segment.text.empty()) {
        return true;
    }

    int parenDepth = 0;
    int bracketDepth = 0;
    for (size_t i = 0; i < segment.tokens.size(); ++i) {
        const Token& token = segment.tokens[i];
        if (token.kind == TokenKind::LeftParen) {
            ++parenDepth;
        } else if (token.kind == TokenKind::RightParen && parenDepth > 0) {
            --parenDepth;
        } else if (token.kind == TokenKind::LeftBracket) {
            ++bracketDepth;
        } else if (token.kind == TokenKind::RightBracket && bracketDepth > 0) {
            --bracketDepth;
        } else if (token.kind == TokenKind::Equals && parenDepth == 0 && bracketDepth == 0) {
            if (i != 1 ||
                segment.tokens[0].kind != TokenKind::Identifier ||
                segment.tokens.size() < 3) {
                break;
            }

            const Token& nameToken = segment.tokens[0];
            const size_t equalsIndex = segment.text.find('=');
            const std::string rawValueText = equalsIndex == std::string::npos ? "" : segment.text.substr(equalsIndex + 1);
            const size_t trimStart = rawValueText.find_first_not_of(" \t\r\n");
            const std::string valueText = trim(rawValueText);
            const int valueColumn = trimStart == std::string::npos
                ? segment.column + static_cast<int>(equalsIndex == std::string::npos ? segment.text.size() : equalsIndex + 1)
                : segment.column + static_cast<int>(equalsIndex + 1 + trimStart);

            argument.kind = CallArgumentAst::Kind::Named;
            argument.name = nameToken.text;
            argument.nameColumn = segment.column;
            argument.valueText = valueText;
            argument.valueTokens = tokenRange(segment.tokens, i + 1, segment.tokens.size());
            argument.valueColumn = valueColumn;

            if (valueText.empty()) {
                return true;
            }

            argument.valueAst = parseExpressionAst(
                inputFile,
                lineNumber,
                argument.valueTokens,
                valueColumn,
                sourceLines,
                declaredVariables
            );
            return argument.valueAst != nullptr;
        }
    }

    argument.kind = CallArgumentAst::Kind::Positional;
    argument.valueText = segment.text;
    argument.valueTokens = segment.tokens;
    argument.valueColumn = segment.column;
    argument.valueAst = parseExpressionAst(
        inputFile,
        lineNumber,
        segment.tokens,
        segment.column,
        sourceLines,
        declaredVariables
    );
    return argument.valueAst != nullptr;
}

bool parseCallArguments(
    const std::string& inputFile,
    int lineNumber,
    int statementStartColumn,
    const std::string& functionName,
    const std::string& unclosedMessage,
    const std::map<int, std::string>& sourceLines,
    size_t& argumentsBegin,
    size_t& argumentsEnd,
    int& argumentsStartColumn,
    const std::vector<Token>& statementTokens
) {
    if (statementTokens.size() < 2 ||
        statementTokens[0].kind != TokenKind::Identifier ||
        statementTokens[0].text != functionName ||
        statementTokens[1].kind != TokenKind::LeftParen) {
        return false;
    }

    if (statementTokens.size() < 4 || statementTokens[statementTokens.size() - 2].kind != TokenKind::RightParen) {
        recordSourceError(
            inputFile,
            lineNumber,
            statementStartColumn + statementTokens[1].span.startColumn - 1,
            unclosedMessage,
            sourceLines
        );
        return false;
    }

    const Token& leftParen = statementTokens[1];
    argumentsBegin = 2;
    argumentsEnd = statementTokens.size() - 2;
    argumentsStartColumn = statementStartColumn + leftParen.span.endColumn;
    return true;
}

// printedTypeNeedsStringHelper prints the relevant diagnostic or output text.
bool printedTypeNeedsStringHelper(const Type& type) {
    if (isStringType(type)) {
        return true;
    }

    if (isPairType(type)) {
        return printedTypeNeedsStringHelper(type.subtypes[0]) ||
            printedTypeNeedsStringHelper(type.subtypes[1]);
    }

    if (!isCollectionType(type)) {
        return false;
    }

    for (const Type& subtype : type.subtypes) {
        if (printedTypeNeedsStringHelper(subtype)) {
            return true;
        }
    }

    return false;
}
}

PrintEmitResult emitPrintStatement(
    const std::string& inputFile,
    int lineNumber,
    int statementStartColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::vector<Token>& sourceTokens
) {
    const auto reportUnterminatedString = [&](const Token& token, int baseColumn) {
        const int startColumn = baseColumn + token.span.startColumn - 1;
        const int endColumn = baseColumn + token.span.endColumn - 1;
        Diagnostic diagnostic;
        diagnostic.message = "unterminated string literal in print";
        diagnostic.labels.push_back({
            sourceSpanForColumns(
                inputFile,
                sourceLines,
                lineNumber,
                startColumn,
                endColumn
            ),
            "",
            true
        });
        const SourceSpan insertion = sourceInsertionSpan(
            inputFile,
            sourceLines,
            lineNumber,
            endColumn + 1
        );
        diagnostic.suggestions.push_back({
            insertion,
            "\"",
            "add a closing `\"`",
            SuggestionApplicability::MachineApplicable
        });
        recordDiagnostic(std::move(diagnostic));
    };

    const std::vector<Token>& statementTokens = sourceTokens;
    if (statementTokens.size() >= 2 &&
        statementTokens[0].kind == TokenKind::Identifier &&
        statementTokens[0].text == "print" &&
        statementTokens[1].kind == TokenKind::LeftParen) {
    for (const Token& token : statementTokens) {
            if (token.kind == TokenKind::String && isUnterminatedQuotedToken(token)) {
                reportUnterminatedString(token, statementStartColumn);
                return {false, "", {}};
            }
        }
    }

    size_t argumentsBegin = 0;
    size_t argumentsEnd = 0;
    int argumentsStartColumn = 1;
    if (!parseCallArguments(inputFile, lineNumber, statementStartColumn, "print", "unclosed parenthesis in print", sourceLines, argumentsBegin, argumentsEnd, argumentsStartColumn, statementTokens)) {
        return {false, "", {}};
    }

    for (size_t index = argumentsBegin; index < argumentsEnd; ++index) {
        const Token& token = statementTokens[index];
        if (isUnterminatedQuotedToken(token)) {
            reportUnterminatedString(token, argumentsStartColumn);
            return {false, "", {}};
        }
    }

    if (argumentsBegin == argumentsEnd) {
        return {true, "    cout << '\\n';", {}};
    }

    const std::vector<RawArgumentSegment> rawArguments = splitCallArguments(statementTokens, argumentsBegin, argumentsEnd, statementStartColumn);

    if (rawArguments.empty() || std::any_of(rawArguments.begin(), rawArguments.end(), [](const RawArgumentSegment& arg) {
            return arg.text.empty();
        })) {
        recordSourceError(inputFile, lineNumber, argumentsStartColumn, "empty print argument", sourceLines);
        return {false, "", {}};
    }

    for (const RawArgumentSegment& rawArgument : rawArguments) {
        const bool startsWithNamedOption =
            rawArgument.tokens.size() >= 2 &&
            rawArgument.tokens[0].kind == TokenKind::Identifier &&
            rawArgument.tokens[1].kind == TokenKind::Equals;
        if (startsWithNamedOption) {
            continue;
        }

        for (const Token& token : rawArgument.tokens) {
            if (token.kind == TokenKind::Identifier && token.text == "end") {
                recordSourceError(inputFile, lineNumber, rawArgument.column + token.span.startColumn - 1, "expected end = value", sourceLines);
                return {false, "", {}};
            }

            if (token.kind == TokenKind::Identifier && token.text == "delim") {
                recordSourceError(inputFile, lineNumber, rawArgument.column + token.span.startColumn - 1, "expected delim = value", sourceLines);
                return {false, "", {}};
            }

            if (token.kind == TokenKind::Identifier && token.text == "flush") {
                recordSourceError(inputFile, lineNumber, rawArgument.column + token.span.startColumn - 1, "use end = flush instead of flush argument", sourceLines);
                return {false, "", {}};
            }
        }

        if (looksLikeMissingPrintComma(rawArgument)) {
            recordSourceError(inputFile, lineNumber, rawArgument.column, "expected ',' between print arguments", sourceLines);
            return {false, "", {}};
        }
    }

    std::vector<CallArgumentAst> arguments;
    arguments.reserve(rawArguments.size());
    for (const RawArgumentSegment& rawArgument : rawArguments) {
        CallArgumentAst argument;
        if (!parseCallArgumentAst(inputFile, lineNumber, rawArgument, sourceLines, declaredVariables, argument)) {
            return {false, "", {}};
        }
        arguments.push_back(std::move(argument));
    }

    size_t printableArgumentCount = arguments.size();
    bool sawOption = false;
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (arguments[i].kind == CallArgumentAst::Kind::Named &&
            (arguments[i].name == "end" || arguments[i].name == "delim")) {
            if (!sawOption) {
                printableArgumentCount = i;
                sawOption = true;
            }
            if (arguments[i].valueText.empty()) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    arguments[i].nameColumn,
                    "expected " + arguments[i].name + " = value",
                    sourceLines
                );
                return {false, "", {}};
            }
            continue;
        }

        if (sawOption) {
            recordSourceError(inputFile, lineNumber, arguments[i].column, "print options must come after all printed values", sourceLines);
            return {false, "", {}};
        }

        if (arguments[i].kind == CallArgumentAst::Kind::Named) {
            if (arguments[i].name == "flush") {
                recordSourceError(inputFile, lineNumber, arguments[i].nameColumn, "use end = flush instead of flush argument", sourceLines);
                return {false, "", {}};
            }

            recordSourceError(inputFile, lineNumber, arguments[i].nameColumn, "unexpected print option '" + arguments[i].name + "'", sourceLines);
            return {false, "", {}};
        }

    }

    if (printableArgumentCount == 0) {
        recordSourceError(inputFile, lineNumber, arguments.back().column, "print requires at least one value", sourceLines);
        return {false, "", {}};
    }

    std::string generatedEnd = "cout << '\\n';";
    std::string generatedDelim;
    bool hasDelim = false;
    bool delimNeedsStringHelper = false;
    for (size_t i = printableArgumentCount; i < arguments.size(); ++i) {
        const CallArgumentAst& option = arguments[i];
        if (option.name == "end") {
            if (option.valueAst &&
                dynamic_cast<VariableExpr*>(option.valueAst.get()) != nullptr &&
                static_cast<VariableExpr*>(option.valueAst.get())->name == "flush") {
                generatedEnd = "cout << '\\n' << flush;";
            } else {
                const ExpressionEmitResult expression = emitExpression(
                    inputFile,
                    lineNumber,
                    option.valueTokens,
                    option.valueColumn,
                    sourceLines,
                    declaredVariables
                );
                if (!expression.ok) {
                    return {false, "", {}};
                }
                if (expression.type != PrimitiveType::Char && !isStringType(expression.type)) {
                    recordSourceError(inputFile, lineNumber, option.valueColumn, "print end must be a string, char, or flush", sourceLines);
                    return {false, "", {}};
                }
                if (isStringType(expression.type)) {
                    requireRuntimeHelper("CPPPPrintValueString");
                    generatedEnd = "CPPPPrintValue(cout, " + expression.generatedExpression + ");";
                } else {
                    generatedEnd = "cout << " + expression.generatedExpression + ";";
                }
            }
            continue;
        }

        if (option.valueAst &&
            dynamic_cast<VariableExpr*>(option.valueAst.get()) != nullptr &&
            static_cast<VariableExpr*>(option.valueAst.get())->name == "flush") {
            recordSourceError(inputFile, lineNumber, option.valueColumn, "print delim must be a string or char", sourceLines);
            return {false, "", {}};
        }
        const ExpressionEmitResult expression = emitExpression(
            inputFile,
            lineNumber,
            option.valueTokens,
            option.valueColumn,
            sourceLines,
            declaredVariables
        );
        if (!expression.ok) {
            return {false, "", {}};
        }

        if (isFunctionType(expression.type)) {
            recordSourceError(inputFile, lineNumber, arguments[i].column,
                "function values cannot be printed; call the function or compare it with == or !=", sourceLines);
            return {false, "", {}};
        }
        if (expression.type != PrimitiveType::Char && !isStringType(expression.type)) {
            recordSourceError(inputFile, lineNumber, option.valueColumn, "print delim must be a string or char", sourceLines);
            return {false, "", {}};
        }
        generatedDelim = expression.generatedExpression;
        hasDelim = true;
        delimNeedsStringHelper = isStringType(expression.type);
    }

    std::string generatedStatement = "    ";
    std::vector<SourceRange> ranges;
    for (size_t i = 0; i < printableArgumentCount; ++i) {
        const ExpressionEmitResult expression = emitExpression(
            inputFile,
            lineNumber,
            arguments[i].valueTokens,
            arguments[i].column,
            sourceLines,
            declaredVariables
        );
        if (!expression.ok) {
            return {false, "", {}};
        }

        if (hasDelim && !isListType(expression.type) && !isSetType(expression.type)) {
            recordSourceError(inputFile, lineNumber, arguments[i].column, "print delim requires a List value", sourceLines);
            return {false, "", {}};
        }

        if (i > 0) {
            generatedStatement += "cout << ' '; ";
        }

        const bool stringArgument = isStringType(expression.type);
        const bool delimitedArgument = (isListType(expression.type) && !stringArgument) || isSetType(expression.type) || (stringArgument && hasDelim);
        const bool helperArgument = delimitedArgument || isLinearDataStructureType(expression.type) || isHeapType(expression.type) ||
            isMapType(expression.type) || isPairType(expression.type) || isStructType(expression.type);
        if (helperArgument) {
            requirePrintHelpersForType(expression.type);
            if (hasDelim) {
                requireRuntimeHelper("CPPPPrintDelimited");
            }
            if (printedTypeNeedsStringHelper(expression.type) || delimNeedsStringHelper) {
                requireRuntimeHelper("CPPPPrintValueString");
            }
            generatedStatement += hasDelim ? "CPPPPrintDelimited(cout, " : "CPPPPrintValue(cout, ";
        } else if (stringArgument) {
            requireRuntimeHelper("CPPPPrintValueString");
            generatedStatement += "CPPPPrintValue(cout, ";
        } else {
            if (expression.type == PrimitiveType::Char) {
                requireRuntimeHelper("CPPPCharOutput");
            }
            generatedStatement += "cout << ";
        }
        const int generatedStartColumn = static_cast<int>(generatedStatement.size()) + 1;
        generatedStatement += expression.generatedExpression;
        if (helperArgument || stringArgument) {
            if (hasDelim) {
                generatedStatement += ", " + generatedDelim + "); ";
            } else {
                generatedStatement += "); ";
            }
        } else {
            generatedStatement += "; ";
        }
        ranges.push_back({
            lineNumber,
            arguments[i].column,
            generatedStartColumn,
            generatedStartColumn + static_cast<int>(expression.generatedExpression.size()) - 1
        });
    }

    generatedStatement += generatedEnd;

    return {true, generatedStatement, ranges};
}

PrintEmitResult emitDescribeStatement(
    const std::string& inputFile,
    int lineNumber,
    int statementStartColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::vector<Token>& sourceTokens
) {
    size_t argumentsBegin = 0;
    size_t argumentsEnd = 0;
    int argumentStartColumn = 1;
    if (!parseCallArguments(inputFile, lineNumber, statementStartColumn, "describe", "unclosed parenthesis in describe", sourceLines, argumentsBegin, argumentsEnd, argumentStartColumn, sourceTokens)) {
        return {false, "", {}};
    }

    if (argumentsBegin == argumentsEnd) {
        recordSourceError(inputFile, lineNumber, argumentStartColumn, "describe requires a value", sourceLines);
        return {false, "", {}};
    }

    const std::vector<RawArgumentSegment> rawArguments = splitCallArguments(sourceTokens, argumentsBegin, argumentsEnd, statementStartColumn);
    if (rawArguments.empty() || std::any_of(rawArguments.begin(), rawArguments.end(), [](const RawArgumentSegment& arg) {
            return arg.text.empty();
        })) {
        recordSourceError(inputFile, lineNumber, argumentStartColumn, "describe requires a value", sourceLines);
        return {false, "", {}};
    }
    if (rawArguments.size() != 1) {
        recordSourceError(inputFile, lineNumber, rawArguments[1].column, "describe takes exactly one value", sourceLines);
        return {false, "", {}};
    }

    CallArgumentAst argument;
    if (!parseCallArgumentAst(inputFile, lineNumber, rawArguments[0], sourceLines, declaredVariables, argument)) {
        return {false, "", {}};
    }
    if (argument.kind == CallArgumentAst::Kind::Named) {
        recordSourceError(inputFile, lineNumber, argument.nameColumn, "describe does not support named arguments", sourceLines);
        return {false, "", {}};
    }

    const ExpressionEmitResult expression = emitExpression(
        inputFile,
        lineNumber,
        argument.valueTokens,
        argument.valueColumn,
        sourceLines,
        declaredVariables
    );
    if (!expression.ok) {
        return {false, "", {}};
    }

    const std::string label = escapeForCppStringLiteral(trim(argument.valueText));
    requirePrintHelpersForType(expression.type);
    if (printedTypeNeedsStringHelper(expression.type)) {
        requireRuntimeHelper("CPPPPrintValueString");
    }
    const std::string generatedStatement =
        "    { auto __cppp_describe_value = " + expression.generatedExpression +
        "; cout << \"" + label + ": \"; CPPPPrintValue(cout, __cppp_describe_value); cout << '\\n'; }";
    return {
        true,
        generatedStatement,
        {{
            lineNumber,
            argument.valueColumn,
            34,
            33 + static_cast<int>(expression.generatedExpression.size())
        }}
    };
}
