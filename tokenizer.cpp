#include "tokenizer.h"

#include <cctype>

namespace {
struct Scanner {
    const std::string& source;
    size_t index = 0;
    int line = 1;
    int column = 1;

    bool atEnd() const {
        return index >= source.size();
    }

    char peek() const {
        return atEnd() ? '\0' : source[index];
    }

    char peekNext() const {
        return index + 1 >= source.size() ? '\0' : source[index + 1];
    }

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

bool isIdentifierStart(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool isIdentifierPart(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

Token makeToken(TokenKind kind, const std::string& text, int startLine, int startColumn, int endLine, int endColumn) {
    return {kind, text, {startLine, startColumn, endLine, endColumn}};
}

Token scanIdentifier(Scanner& scanner) {
    const int startLine = scanner.line;
    const int startColumn = scanner.column;
    std::string text;

    while (isIdentifierPart(scanner.peek())) {
        text += scanner.advance();
    }

    return makeToken(TokenKind::Identifier, text, startLine, startColumn, scanner.line, scanner.column - 1);
}

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

Token scanQuoted(Scanner& scanner, TokenKind kind) {
    const int startLine = scanner.line;
    const int startColumn = scanner.column;
    const char quote = scanner.advance();
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

        const char consumed = scanner.advance();
        TokenKind kind = TokenKind::Unknown;
        switch (consumed) {
            case '(':
                kind = TokenKind::LeftParen;
                break;
            case ')':
                kind = TokenKind::RightParen;
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
