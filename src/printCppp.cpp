#include "printCppp.h"

#include "exprAst.h"
#include "expressions.h"
#include "tokenizer.h"
#include "typesCppp.h"

#include <algorithm>

namespace {
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
    int valueColumn = 0;
    std::unique_ptr<Expr> valueAst;
};

struct RawArgumentSegment {
    std::string text;
    int column;
    std::vector<Token> tokens;
};

std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

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

std::vector<RawArgumentSegment> splitCallArguments(const std::string& text, const std::vector<Token>& tokens, int startColumn) {
    std::vector<RawArgumentSegment> arguments;
    int parenDepth = 0;
    int bracketDepth = 0;
    size_t argumentStartTokenIndex = 0;
    bool endedWithTopLevelComma = false;

    for (size_t tokenIndex = 0; tokenIndex < tokens.size(); ++tokenIndex) {
        const Token& token = tokens[tokenIndex];
        if (token.kind == TokenKind::EndOfFile) {
            if (argumentStartTokenIndex < tokenIndex) {
                const Token& firstToken = tokens[argumentStartTokenIndex];
                const Token& lastToken = tokens[tokenIndex - 1];
                const size_t rawStartIndex = static_cast<size_t>(firstToken.span.startColumn - 1);
                const size_t rawLength = static_cast<size_t>(lastToken.span.endColumn - firstToken.span.startColumn + 1);
                const std::string rawText = text.substr(rawStartIndex, rawLength);
                const size_t trimStart = rawText.find_first_not_of(" \t\r\n");
                arguments.push_back({
                    trim(rawText),
                    trimStart == std::string::npos ? startColumn + firstToken.span.startColumn - 1 : startColumn + firstToken.span.startColumn - 1 + static_cast<int>(trimStart),
                    std::vector<Token>(tokens.begin() + static_cast<std::ptrdiff_t>(argumentStartTokenIndex), tokens.begin() + static_cast<std::ptrdiff_t>(tokenIndex))
                });
            } else if (endedWithTopLevelComma) {
                arguments.push_back({"", startColumn + token.span.startColumn - 1, {}});
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
                const Token& lastToken = tokens[tokenIndex - 1];
                const size_t rawStartIndex = static_cast<size_t>(firstToken.span.startColumn - 1);
                const size_t rawLength = static_cast<size_t>(lastToken.span.endColumn - firstToken.span.startColumn + 1);
                const std::string rawText = text.substr(rawStartIndex, rawLength);
                const size_t trimStart = rawText.find_first_not_of(" \t\r\n");
                arguments.push_back({
                    trim(rawText),
                    trimStart == std::string::npos ? startColumn + firstToken.span.startColumn - 1 : startColumn + firstToken.span.startColumn - 1 + static_cast<int>(trimStart),
                    std::vector<Token>(tokens.begin() + static_cast<std::ptrdiff_t>(argumentStartTokenIndex), tokens.begin() + static_cast<std::ptrdiff_t>(tokenIndex))
                });
            } else {
                arguments.push_back({"", startColumn + token.span.startColumn - 1, {}});
            }
            argumentStartTokenIndex = tokenIndex + 1;
            endedWithTopLevelComma = true;
            continue;
        }

        endedWithTopLevelComma = false;
    }
    return arguments;
}

bool isUnterminatedQuotedToken(const Token& token) {
    if (token.kind != TokenKind::String && token.kind != TokenKind::Char) {
        return false;
    }

    return token.text.size() < 2 || token.text.front() != token.text.back();
}

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
            argument.valueColumn = valueColumn;

            if (valueText.empty()) {
                return true;
            }

            argument.valueAst = parseExpressionAst(
                inputFile,
                lineNumber,
                valueText,
                valueColumn,
                sourceLines,
                declaredVariables
            );
            return argument.valueAst != nullptr;
        }
    }

    argument.kind = CallArgumentAst::Kind::Positional;
    argument.valueText = segment.text;
    argument.valueColumn = segment.column;
    argument.valueAst = parseExpressionAst(
        inputFile,
        lineNumber,
        segment.text,
        segment.column,
        sourceLines,
        declaredVariables
    );
    return argument.valueAst != nullptr;
}

bool parseCallArguments(
    const std::string& inputFile,
    int lineNumber,
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::string& functionName,
    const std::string& unclosedMessage,
    const std::map<int, std::string>& sourceLines,
    std::string& argumentsText,
    int& argumentsStartColumn
) {
    const size_t statementColumn = sourceLine.find(statementBody);
    const int statementStartColumn = static_cast<int>(statementColumn == std::string::npos ? 1 : statementColumn + 1);
    const std::vector<Token> statementTokens = tokenize(statementBody);
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
    const Token& rightParen = statementTokens[statementTokens.size() - 2];
    argumentsText = statementBody.substr(
        static_cast<size_t>(leftParen.span.endColumn),
        static_cast<size_t>(rightParen.span.startColumn - leftParen.span.endColumn - 1)
    );
    argumentsStartColumn = statementStartColumn + leftParen.span.endColumn;
    return true;
}

bool isListType(const Type& type) {
    return type.primitive == PrimitiveType::List && type.subtypes.size() == 1;
}

bool printedTypeNeedsStringHelper(const Type& type) {
    if (isStringType(type)) {
        return true;
    }

    if (type.primitive != PrimitiveType::List) {
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
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables
) {
    const std::vector<Token> statementTokens = tokenize(statementBody);
    if (statementTokens.size() >= 2 &&
        statementTokens[0].kind == TokenKind::Identifier &&
        statementTokens[0].text == "print" &&
        statementTokens[1].kind == TokenKind::LeftParen) {
        for (const Token& token : statementTokens) {
            if (token.kind == TokenKind::String && isUnterminatedQuotedToken(token)) {
                recordSourceError(inputFile, lineNumber, token.span.startColumn, "unterminated string literal in print", sourceLines);
                return {false, "", {}};
            }
        }
    }

    std::string printArguments;
    int argumentsStartColumn = 1;
    if (!parseCallArguments(inputFile, lineNumber, sourceLine, statementBody, "print", "unclosed parenthesis in print", sourceLines, printArguments, argumentsStartColumn)) {
        return {false, "", {}};
    }

    const std::vector<Token> tokens = tokenize(printArguments);
    for (const Token& token : tokens) {
        if (isUnterminatedQuotedToken(token)) {
            recordSourceError(inputFile, lineNumber, argumentsStartColumn + token.span.startColumn - 1, "unterminated string literal in print", sourceLines);
            return {false, "", {}};
        }
    }

    if (tokens.size() == 1 && tokens[0].kind == TokenKind::EndOfFile) {
        return {true, "    cout << '\\n';", {}};
    }

    const std::vector<RawArgumentSegment> rawArguments = splitCallArguments(printArguments, tokens, argumentsStartColumn);

    if (rawArguments.empty() || std::any_of(rawArguments.begin(), rawArguments.end(), [](const RawArgumentSegment& arg) {
            return arg.text.empty();
        })) {
        recordSourceError(inputFile, lineNumber, argumentsStartColumn, "empty print argument", sourceLines);
        return {false, "", {}};
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

        if (looksLikeMissingPrintComma(rawArguments[i])) {
            recordSourceError(inputFile, lineNumber, rawArguments[i].column, "expected ',' between print arguments", sourceLines);
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
                    option.valueText,
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
            option.valueText,
            option.valueColumn,
            sourceLines,
            declaredVariables
        );
        if (!expression.ok) {
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
            arguments[i].text,
            arguments[i].column,
            sourceLines,
            declaredVariables
        );
        if (!expression.ok) {
            return {false, "", {}};
        }

        if (hasDelim && !isListType(expression.type)) {
            recordSourceError(inputFile, lineNumber, arguments[i].column, "print delim requires a List value", sourceLines);
            return {false, "", {}};
        }

        if (i > 0) {
            generatedStatement += "cout << ' '; ";
        }

        const bool listArgument = isListType(expression.type);
        if (listArgument) {
            if (hasDelim) {
                requireRuntimeHelper("CPPPPrintDelimited");
            } else {
                requireRuntimeHelper("CPPPPrintValue");
            }
            if (printedTypeNeedsStringHelper(expression.type) || delimNeedsStringHelper) {
                requireRuntimeHelper("CPPPPrintValueString");
            }
            generatedStatement += hasDelim ? "CPPPPrintDelimited(cout, " : "CPPPPrintValue(cout, ";
        } else {
            generatedStatement += "cout << ";
        }
        const int generatedStartColumn = static_cast<int>(generatedStatement.size()) + 1;
        generatedStatement += expression.generatedExpression;
        if (listArgument) {
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
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables
) {
    std::string describeArgument;
    int argumentStartColumn = 1;
    if (!parseCallArguments(inputFile, lineNumber, sourceLine, statementBody, "describe", "unclosed parenthesis in describe", sourceLines, describeArgument, argumentStartColumn)) {
        return {false, "", {}};
    }

    const std::vector<Token> tokens = tokenize(describeArgument);
    if (tokens.size() == 1 && tokens[0].kind == TokenKind::EndOfFile) {
        recordSourceError(inputFile, lineNumber, argumentStartColumn, "describe requires a value", sourceLines);
        return {false, "", {}};
    }

    const std::vector<RawArgumentSegment> rawArguments = splitCallArguments(describeArgument, tokens, argumentStartColumn);
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
        argument.valueText,
        argument.valueColumn,
        sourceLines,
        declaredVariables
    );
    if (!expression.ok) {
        return {false, "", {}};
    }

    const std::string label = escapeForCppStringLiteral(trim(argument.valueText));
    requireRuntimeHelper("CPPPPrintValue");
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
