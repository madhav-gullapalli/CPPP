#include "printCppp.h"

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

bool isValueToken(const Token& token) {
    return token.kind == TokenKind::Identifier ||
        token.kind == TokenKind::Integer ||
        token.kind == TokenKind::Float ||
        token.kind == TokenKind::String ||
        token.kind == TokenKind::Char;
}
}

PrintEmitResult emitPrintStatement(
    const std::string& inputFile,
    int lineNumber,
    const std::string& sourceLine,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::set<std::string>& declaredVariables
) {
    const size_t statementColumn = sourceLine.find(statementBody);
    const std::string printPrefix = "print(";
    if (statementBody.rfind(printPrefix, 0) != 0) {
        recordSourceError(inputFile, lineNumber, 1, "unsupported statement", sourceLines);
        return {false, "", {}};
    }

    if (statementBody.back() != ')') {
        recordSourceError(
            inputFile,
            lineNumber,
            static_cast<int>((statementColumn == std::string::npos ? 0 : statementColumn) + printPrefix.size()),
            "unclosed parenthesis in print",
            sourceLines
        );
        return {false, "", {}};
    }

    const std::string printArguments = statementBody.substr(
        printPrefix.size(),
        statementBody.size() - printPrefix.size() - 1
    );
    const int argumentsStartColumn =
        static_cast<int>(statementColumn == std::string::npos ? 1 : statementColumn + 1) +
        static_cast<int>(printPrefix.size());

    const std::vector<Token> tokens = tokenize(printArguments);
    for (const Token& token : tokens) {
        if (isUnterminatedQuotedToken(token)) {
            recordSourceError(inputFile, lineNumber, argumentsStartColumn + token.span.startColumn - 1, "unterminated string literal in print", sourceLines);
            return {false, "", {}};
        }
    }

    const std::vector<PrintArgument> arguments = splitPrintArguments(printArguments, tokens, argumentsStartColumn);

    if (arguments.empty() || std::any_of(arguments.begin(), arguments.end(), [](const PrintArgument& arg) {
            return arg.text.empty();
        })) {
        recordSourceError(inputFile, lineNumber, argumentsStartColumn, "empty print argument", sourceLines);
        return {false, "", {}};
    }

    bool shouldFlush = false;
    for (size_t i = 0; i < arguments.size(); ++i) {
        for (size_t tokenIndex = 1; tokenIndex < arguments[i].tokens.size(); ++tokenIndex) {
            const Token& previous = arguments[i].tokens[tokenIndex - 1];
            const Token& current = arguments[i].tokens[tokenIndex];
            if (isValueToken(previous) && isValueToken(current) && current.span.startColumn > previous.span.endColumn + 1) {
                const std::string message = current.text == "flush" ? "expected ',' before flush" : "expected ',' between print arguments";
                recordSourceError(inputFile, lineNumber, arguments[i].column + current.span.startColumn - 1, message, sourceLines);
                return {false, "", {}};
            }
        }

        if (arguments[i].tokens.size() == 1 &&
            arguments[i].tokens[0].kind == TokenKind::Identifier &&
            arguments[i].text != "flush" &&
            declaredVariables.count(arguments[i].text) == 0) {
            recordSourceError(inputFile, lineNumber, arguments[i].column, "use of undeclared variable '" + arguments[i].text + "'", sourceLines);
            return {false, "", {}};
        }

        for (const Token& token : arguments[i].tokens) {
            if (token.kind == TokenKind::Identifier && token.text == "flush" && arguments[i].text != "flush") {
                recordSourceError(inputFile, lineNumber, arguments[i].column + token.span.startColumn - 1, "expected ',' before flush", sourceLines);
                return {false, "", {}};
            }
        }

        if (arguments[i].text == "flush") {
            if (i != arguments.size() - 1) {
                recordSourceError(inputFile, lineNumber, arguments[i].column, "flush must be the final print argument", sourceLines);
                return {false, "", {}};
            }

            shouldFlush = true;
        }
    }

    const size_t printableArgumentCount = shouldFlush ? arguments.size() - 1 : arguments.size();
    if (printableArgumentCount == 0) {
        recordSourceError(inputFile, lineNumber, arguments.back().column, "print requires at least one value before flush", sourceLines);
        return {false, "", {}};
    }

    std::string generatedStatement = "    cout";
    std::vector<SourceRange> ranges;
    for (size_t i = 0; i < printableArgumentCount; ++i) {
        if (i > 0) {
            generatedStatement += " << ' '";
        }

        generatedStatement += " << ";
        const int generatedStartColumn = static_cast<int>(generatedStatement.size()) + 1;
        generatedStatement += arguments[i].text;
        ranges.push_back({
            lineNumber,
            arguments[i].column,
            generatedStartColumn,
            generatedStartColumn + static_cast<int>(arguments[i].text.size()) - 1
        });
    }

    if (shouldFlush) {
        generatedStatement += " << '\\n' << flush;";
    } else {
        generatedStatement += " << '\\n';";
    }

    return {true, generatedStatement, ranges};
}
