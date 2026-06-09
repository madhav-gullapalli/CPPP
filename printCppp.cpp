#include "printCppp.h"

#include <algorithm>
#include <cctype>

namespace {
struct PrintArgument {
    std::string text;
    int column;
};

std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::vector<PrintArgument> splitPrintArguments(const std::string& text, int startColumn) {
    std::vector<PrintArgument> arguments;
    std::string current;
    int currentStartColumn = startColumn;
    int parenDepth = 0;
    bool inString = false;
    char stringDelimiter = '\0';
    bool escaped = false;

    for (size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        if (inString) {
            current += ch;

            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == stringDelimiter) {
                inString = false;
            }

            continue;
        }

        if (ch == '"' || ch == '\'') {
            inString = true;
            stringDelimiter = ch;
            current += ch;
            continue;
        }

        if (ch == '(') {
            ++parenDepth;
        } else if (ch == ')' && parenDepth > 0) {
            --parenDepth;
        }

        if (ch == ',' && parenDepth == 0) {
            const size_t trimStart = current.find_first_not_of(" \t\r\n");
            arguments.push_back({
                trim(current),
                trimStart == std::string::npos ? currentStartColumn : currentStartColumn + static_cast<int>(trimStart)
            });
            current.clear();
            currentStartColumn = startColumn + static_cast<int>(index) + 1;
            continue;
        }

        current += ch;
    }

    const size_t trimStart = current.find_first_not_of(" \t\r\n");
    arguments.push_back({
        trim(current),
        trimStart == std::string::npos ? currentStartColumn : currentStartColumn + static_cast<int>(trimStart)
    });
    return arguments;
}

int findBareFlushToken(const std::string& text) {
    bool inString = false;
    char stringDelimiter = '\0';
    bool escaped = false;

    for (size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == stringDelimiter) {
                inString = false;
            }

            continue;
        }

        if (ch == '"' || ch == '\'') {
            inString = true;
            stringDelimiter = ch;
            continue;
        }

        if (text.compare(index, 5, "flush") == 0) {
            const bool validBefore = index == 0 || !std::isalnum(static_cast<unsigned char>(text[index - 1]));
            const size_t after = index + 5;
            const bool validAfter = after == text.size() || !std::isalnum(static_cast<unsigned char>(text[after]));
            if (validBefore && validAfter) {
                return static_cast<int>(index);
            }
        }
    }

    return -1;
}

bool hasBalancedStringQuotes(const std::string& text) {
    bool inString = false;
    char stringDelimiter = '\0';
    bool escaped = false;

    for (const char ch : text) {
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == stringDelimiter) {
                inString = false;
            }

            continue;
        }

        if (ch == '"' || ch == '\'') {
            inString = true;
            stringDelimiter = ch;
        }
    }

    return !inString;
}

bool isIdentifier(const std::string& text) {
    if (text.empty() || !(std::isalpha(static_cast<unsigned char>(text[0])) || text[0] == '_')) {
        return false;
    }

    for (const char ch : text) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
            return false;
        }
    }

    return true;
}

int findWhitespaceSeparatedTokens(const std::string& text) {
    bool inString = false;
    char stringDelimiter = '\0';
    bool escaped = false;
    bool sawWhitespace = false;

    for (size_t index = 0; index < text.size(); ++index) {
        const char ch = text[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == stringDelimiter) {
                inString = false;
            }

            continue;
        }

        if (ch == '"' || ch == '\'') {
            inString = true;
            stringDelimiter = ch;
            sawWhitespace = false;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch))) {
            sawWhitespace = true;
            continue;
        }

        const bool tokenChar = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
        if (sawWhitespace && tokenChar) {
            return static_cast<int>(index);
        }

        sawWhitespace = false;
    }

    return -1;
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
    if (statementBody.rfind(printPrefix, 0) != 0 || statementBody.back() != ')') {
        recordSourceError(inputFile, lineNumber, 1, "unsupported statement", sourceLines);
        return {false, "", {}};
    }

    const std::string printArguments = statementBody.substr(
        printPrefix.size(),
        statementBody.size() - printPrefix.size() - 1
    );
    const int argumentsStartColumn =
        static_cast<int>(statementColumn == std::string::npos ? 1 : statementColumn + 1) +
        static_cast<int>(printPrefix.size());

    if (!hasBalancedStringQuotes(printArguments)) {
        recordSourceError(inputFile, lineNumber, argumentsStartColumn, "unterminated string literal in print", sourceLines);
        return {false, "", {}};
    }

    const std::vector<PrintArgument> arguments = splitPrintArguments(printArguments, argumentsStartColumn);

    if (arguments.empty() || std::any_of(arguments.begin(), arguments.end(), [](const PrintArgument& arg) {
            return arg.text.empty();
        })) {
        recordSourceError(inputFile, lineNumber, argumentsStartColumn, "empty print argument", sourceLines);
        return {false, "", {}};
    }

    bool shouldFlush = false;
    for (size_t i = 0; i < arguments.size(); ++i) {
        const int separatedTokenIndex = findWhitespaceSeparatedTokens(arguments[i].text);
        if (separatedTokenIndex != -1) {
            recordSourceError(inputFile, lineNumber, arguments[i].column + separatedTokenIndex, "expected ',' between print arguments", sourceLines);
            return {false, "", {}};
        }

        if (isIdentifier(arguments[i].text) && arguments[i].text != "flush" && declaredVariables.count(arguments[i].text) == 0) {
            recordSourceError(inputFile, lineNumber, arguments[i].column, "use of undeclared variable '" + arguments[i].text + "'", sourceLines);
            return {false, "", {}};
        }

        const int bareFlushIndex = findBareFlushToken(arguments[i].text);
        if (bareFlushIndex != -1 && arguments[i].text != "flush") {
            recordSourceError(inputFile, lineNumber, arguments[i].column + bareFlushIndex, "expected ',' before flush", sourceLines);
            return {false, "", {}};
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
