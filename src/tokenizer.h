#pragma once

#include <string>
#include <vector>

enum class TokenKind {
    Identifier,
    Integer,
    Float,
    String,
    Char,
    DoubleQuote,
    SingleQuote,
    LeftParen,
    RightParen,
    Comma,
    Semicolon,
    Equals,
    Operator,
    Unknown,
    EndOfFile
};

struct SourceSpan {
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
};

struct Token {
    TokenKind kind;
    std::string text;
    SourceSpan span;
};

std::vector<Token> tokenize(const std::string& source);
std::string tokenKindName(TokenKind kind);
