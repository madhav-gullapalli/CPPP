#include "printCppp.h"

#include "expressions.h"
#include "tokenizer.h"

#include <algorithm>

namespace {
struct PrintArgument {
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

std::vector<PrintArgument> splitPrintArguments(const std::string& text, const std::vector<Token>& tokens, int startColumn) {
    std::vector<PrintArgument> arguments;
    int parenDepth = 0;
    size_t argumentStartIndex = 0;
    int argumentStartColumn = startColumn;
    std::vector<Token> currentTokens;

    for (const Token& token : tokens) {
        if (token.kind == TokenKind::EndOfFile) {
            continue;
        }

        if (token.kind == TokenKind::LeftParen) {
            ++parenDepth;
        } else if (token.kind == TokenKind::RightParen && parenDepth > 0) {
            --parenDepth;
        }

        if (token.kind == TokenKind::Comma && parenDepth == 0) {
            const size_t argumentEndIndex = static_cast<size_t>(std::max(0, token.span.startColumn - 2));
            std::string argumentText = argumentEndIndex >= argumentStartIndex ?
                text.substr(argumentStartIndex, argumentEndIndex - argumentStartIndex + 1) :
                "";
            const size_t trimStart = argumentText.find_first_not_of(" \t\r\n");
            arguments.push_back({
                trim(argumentText),
                trimStart == std::string::npos ? argumentStartColumn : argumentStartColumn + static_cast<int>(trimStart),
                currentTokens
            });
            argumentStartIndex = static_cast<size_t>(token.span.endColumn);
            argumentStartColumn = startColumn + token.span.endColumn;
            currentTokens.clear();
            continue;
        }

        currentTokens.push_back(token);
    }

    std::string argumentText = argumentStartIndex < text.size() ? text.substr(argumentStartIndex) : "";
    const size_t trimStart = argumentText.find_first_not_of(" \t\r\n");
    arguments.push_back({
        trim(argumentText),
        trimStart == std::string::npos ? argumentStartColumn : argumentStartColumn + static_cast<int>(trimStart),
        currentTokens
    });
    return arguments;
}

bool isUnterminatedQuotedToken(const Token& token) {
    if (token.kind != TokenKind::String && token.kind != TokenKind::Char) {
        return false;
    }

    return token.text.size() < 2 || token.text.front() != token.text.back();
}

bool isEndOption(const PrintArgument& argument) {
    return argument.tokens.size() == 3 &&
        argument.tokens[0].kind == TokenKind::Identifier &&
        argument.tokens[0].text == "end" &&
        argument.tokens[1].kind == TokenKind::Equals;
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
        recordSourceError(inputFile, lineNumber, 1, "unsupported statement", sourceLines);
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
}

PrintEmitResult emitPrintStatement(
    const std::string& inputFile,
    int lineNumber,
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables
) {
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

    const std::vector<PrintArgument> arguments = splitPrintArguments(printArguments, tokens, argumentsStartColumn);

    if (arguments.empty() || std::any_of(arguments.begin(), arguments.end(), [](const PrintArgument& arg) {
            return arg.text.empty();
        })) {
        recordSourceError(inputFile, lineNumber, argumentsStartColumn, "empty print argument", sourceLines);
        return {false, "", {}};
    }

    std::string generatedEnd = " << '\\n'";
    size_t printableArgumentCount = arguments.size();
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (isEndOption(arguments[i])) {
            if (i != arguments.size() - 1) {
                recordSourceError(inputFile, lineNumber, arguments[i].column, "end must be the final print option", sourceLines);
                return {false, "", {}};
            }

            const Token& endValue = arguments[i].tokens[2];
            if (endValue.kind == TokenKind::String || endValue.kind == TokenKind::Char) {
                generatedEnd = " << " + endValue.text;
            } else if (endValue.kind == TokenKind::Identifier && endValue.text == "flush") {
                generatedEnd = " << '\\n' << flush";
            } else {
                recordSourceError(inputFile, lineNumber, arguments[i].column + endValue.span.startColumn - 1, "print end must be a string, char, or flush", sourceLines);
                return {false, "", {}};
            }

            printableArgumentCount = i;
            continue;
        }

        for (const Token& token : arguments[i].tokens) {
            if (token.kind == TokenKind::Identifier && token.text == "end") {
                recordSourceError(inputFile, lineNumber, arguments[i].column + token.span.startColumn - 1, "expected end = value", sourceLines);
                return {false, "", {}};
            }

            if (token.kind == TokenKind::Identifier && token.text == "flush") {
                recordSourceError(inputFile, lineNumber, arguments[i].column + token.span.startColumn - 1, "use end = flush instead of flush argument", sourceLines);
                return {false, "", {}};
            }
        }

        if (arguments[i].tokens.size() == 1 &&
            arguments[i].tokens[0].kind == TokenKind::Identifier &&
            declaredVariables.count(arguments[i].text) == 0) {
            recordSourceError(inputFile, lineNumber, arguments[i].column, "use of undeclared variable '" + arguments[i].text + "'", sourceLines);
            return {false, "", {}};
        }
    }

    if (printableArgumentCount == 0) {
        recordSourceError(inputFile, lineNumber, arguments.back().column, "print requires at least one value", sourceLines);
        return {false, "", {}};
    }

    std::string generatedStatement = "    cout";
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

        if (i > 0) {
            generatedStatement += " << ' '";
        }

        generatedStatement += " << ";
        const int generatedStartColumn = static_cast<int>(generatedStatement.size()) + 1;
        generatedStatement += expression.generatedExpression;
        ranges.push_back({
            lineNumber,
            arguments[i].column,
            generatedStartColumn,
            generatedStartColumn + static_cast<int>(expression.generatedExpression.size()) - 1
        });
    }

    generatedStatement += generatedEnd + ";";

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
    if (tokens.size() != 2 ||
        tokens[0].kind != TokenKind::Identifier ||
        tokens[1].kind != TokenKind::EndOfFile) {
        recordSourceError(inputFile, lineNumber, argumentStartColumn, "describe requires a single variable", sourceLines);
        return {false, "", {}};
    }

    const std::string variableName = tokens[0].text;
    if (declaredVariables.count(variableName) == 0) {
        recordSourceError(inputFile, lineNumber, argumentStartColumn, "use of undeclared variable '" + variableName + "'", sourceLines);
        return {false, "", {}};
    }

    const std::string generatedStatement = "    cout << \"" + variableName + ": \" << " + variableName + " << '\\n';";
    return {
        true,
        generatedStatement,
        {{
            lineNumber,
            argumentStartColumn,
            19 + static_cast<int>(variableName.size()),
            19 + static_cast<int>(variableName.size()) + static_cast<int>(variableName.size()) - 1
        }}
    };
}
