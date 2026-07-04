/*
 * tokenizer.cpp
 *
 * Tokenizes CP++ source into the language tokens used by later passes.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "tokenizer.h"

#include <cctype>

namespace {
// Scanner implements the Scanner behavior for the tokenizer.cpp module.
struct Scanner {
    const std::string& source;
    size_t index = 0;
    int line = 1;
    int column = 1;

// atEnd implements the atEnd behavior for the tokenizer.cpp module.
    bool atEnd() const {
        return index >= source.size();
    }

// peek implements the peek behavior for the tokenizer.cpp module.
    char peek() const {
        return atEnd() ? '\0' : source[index];
    }

// peekNext implements the peekNext behavior for the tokenizer.cpp module.
    char peekNext() const {
        return index + 1 >= source.size() ? '\0' : source[index + 1];
    }

// advance implements the advance behavior for the tokenizer.cpp module.
    char advance() {
        const char ch = source[index++];
        if (ch == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }

        return ch;
    }
};

// isIdentifierStart returns whether the supplied input satisfies the relevant condition.
bool isIdentifierStart(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

// isIdentifierPart returns whether the supplied input satisfies the relevant condition.
bool isIdentifierPart(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

// makeToken creates the requested object or intermediate value.
Token makeToken(TokenKind kind, const std::string& text, int startLine, int startColumn, int endLine, int endColumn) {
    return {kind, text, {startLine, startColumn, endLine, endColumn}};
}

// scanIdentifier implements the scanIdentifier behavior for the tokenizer.cpp module.
Token scanIdentifier(Scanner& scanner) {
    const int startLine = scanner.line;
    const int startColumn = scanner.column;
    std::string text;

    while (isIdentifierPart(scanner.peek())) {
        text += scanner.advance();
    }

    return makeToken(TokenKind::Identifier, text, startLine, startColumn, scanner.line, scanner.column - 1);
}

// scanNumber implements the scanNumber behavior for the tokenizer.cpp module.
Token scanNumber(Scanner& scanner) {
    const int startLine = scanner.line;
    const int startColumn = scanner.column;
    std::string text;
    bool isFloat = false;

    while (std::isdigit(static_cast<unsigned char>(scanner.peek()))) {
        text += scanner.advance();
    }

    if (scanner.peek() == '.' && std::isdigit(static_cast<unsigned char>(scanner.peekNext()))) {
        isFloat = true;
        text += scanner.advance();
        while (std::isdigit(static_cast<unsigned char>(scanner.peek()))) {
            text += scanner.advance();
        }
    }

    if (scanner.peek() == 'e' || scanner.peek() == 'E') {
        isFloat = true;
        text += scanner.advance();
        if (scanner.peek() == '+' || scanner.peek() == '-') {
            text += scanner.advance();
        }

        while (std::isdigit(static_cast<unsigned char>(scanner.peek()))) {
            text += scanner.advance();
        }
    }

    return makeToken(isFloat ? TokenKind::Float : TokenKind::Integer, text, startLine, startColumn, scanner.line, scanner.column - 1);
}

// scanQuoted implements the scanQuoted behavior for the tokenizer.cpp module.
Token scanQuoted(Scanner& scanner, TokenKind kind) {
    const int startLine = scanner.line;
    const int startColumn = scanner.column;
    const char quote = scanner.advance();
// text implements the text behavior for the tokenizer.cpp module.
    std::string text(1, quote);
    bool escaped = false;

    while (!scanner.atEnd()) {
        const char ch = scanner.advance();
        text += ch;

        if (escaped) {
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == quote) {
            break;
        } else if (ch == '\n') {
            break;
        }
    }

    return makeToken(kind, text, startLine, startColumn, scanner.line, scanner.column - 1);
}
}

// tokenize tokenizes the input source into the stream consumed by later passes.
std::vector<Token> tokenize(const std::string& source) {
    Scanner scanner{source};
    std::vector<Token> tokens;

    while (!scanner.atEnd()) {
        const char ch = scanner.peek();

        if (std::isspace(static_cast<unsigned char>(ch))) {
            scanner.advance();
            continue;
        }

        const int startLine = scanner.line;
        const int startColumn = scanner.column;

        if (isIdentifierStart(ch)) {
            tokens.push_back(scanIdentifier(scanner));
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(ch))) {
            tokens.push_back(scanNumber(scanner));
            continue;
        }

        if (ch == '"') {
            tokens.push_back(scanQuoted(scanner, TokenKind::String));
            continue;
        }

        if (ch == '\'') {
            tokens.push_back(scanQuoted(scanner, TokenKind::Char));
            continue;
        }

        const char third = scanner.index + 2 < scanner.source.size() ? scanner.source[scanner.index + 2] : '\0';
        if (((ch == '<' && scanner.peekNext() == '<') ||
             (ch == '>' && scanner.peekNext() == '>') ||
             (ch == '&' && scanner.peekNext() == '&') ||
             (ch == '|' && scanner.peekNext() == '|')) &&
            third == '=') {
            std::string text;
            text += scanner.advance();
            text += scanner.advance();
            text += scanner.advance();
            tokens.push_back(makeToken(TokenKind::Operator, text, startLine, startColumn, scanner.line, scanner.column - 1));
            continue;
        }

        if ((scanner.peekNext() == '=' &&
                (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' ||
                 ch == '<' || ch == '>' || ch == '&' || ch == '|' || ch == '^')) ||
            (ch == '<' && (scanner.peekNext() == '<' || scanner.peekNext() == '=')) ||
            (ch == '>' && (scanner.peekNext() == '>' || scanner.peekNext() == '=')) ||
            (ch == '=' && scanner.peekNext() == '=') ||
            (ch == '!' && scanner.peekNext() == '=') ||
            (ch == '+' && scanner.peekNext() == '+') ||
            (ch == '-' && scanner.peekNext() == '-') ||
            (ch == '&' && scanner.peekNext() == '&') ||
            (ch == '|' && scanner.peekNext() == '|')) {
            std::string text;
            text += scanner.advance();
            text += scanner.advance();
            tokens.push_back(makeToken(TokenKind::Operator, text, startLine, startColumn, scanner.line, scanner.column - 1));
            continue;
        }

        const char consumed = scanner.advance();
        TokenKind kind = TokenKind::Unknown;
        switch (consumed) {
            case '(':
                kind = TokenKind::LeftParen;
                break;
            case ')':
                kind = TokenKind::RightParen;
                break;
            case '[':
                kind = TokenKind::LeftBracket;
                break;
            case ']':
                kind = TokenKind::RightBracket;
                break;
            case ',':
                kind = TokenKind::Comma;
                break;
            case ';':
                kind = TokenKind::Semicolon;
                break;
            case '=':
                kind = TokenKind::Equals;
                break;
            case '+':
            case '-':
            case '*':
            case '/':
            case '%':
            case '<':
            case '>':
            case '!':
            case '&':
            case '|':
            case '^':
            case '.':
            case ':':
                kind = TokenKind::Operator;
                break;
            default:
                kind = TokenKind::Unknown;
                break;
        }

        tokens.push_back(makeToken(kind, std::string(1, consumed), startLine, startColumn, scanner.line, scanner.column - 1));
    }

    tokens.push_back(makeToken(TokenKind::EndOfFile, "", scanner.line, scanner.column, scanner.line, scanner.column));
    return tokens;
}

// tokenKindName implements the tokenKindName behavior for the tokenizer.cpp module.
std::string tokenKindName(TokenKind kind) {
    switch (kind) {
        case TokenKind::Identifier:
            return "Identifier";
        case TokenKind::Integer:
            return "Integer";
        case TokenKind::Float:
            return "Float";
        case TokenKind::String:
            return "String";
        case TokenKind::Char:
            return "Char";
        case TokenKind::DoubleQuote:
            return "DoubleQuote";
        case TokenKind::SingleQuote:
            return "SingleQuote";
        case TokenKind::LeftParen:
            return "LeftParen";
        case TokenKind::RightParen:
            return "RightParen";
        case TokenKind::LeftBracket:
            return "LeftBracket";
        case TokenKind::RightBracket:
            return "RightBracket";
        case TokenKind::Comma:
            return "Comma";
        case TokenKind::Semicolon:
            return "Semicolon";
        case TokenKind::Equals:
            return "Equals";
        case TokenKind::Operator:
            return "Operator";
        case TokenKind::Unknown:
            return "Unknown";
        case TokenKind::EndOfFile:
            return "EndOfFile";
    }

    return "Unknown";
}
