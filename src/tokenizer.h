/*
 * tokenizer.h
 *
 * Declares token kinds, spans, and token structures used throughout the compiler.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

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
    LeftBracket,
    RightBracket,
    Comma,
    Semicolon,
    Equals,
    Operator,
    Unknown,
    EndOfFile
};

// SourceSpan implements the SourceSpan behavior for the tokenizer.h module.
struct SourceSpan {
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
};

// Token implements the Token behavior for the tokenizer.h module.
struct Token {
    TokenKind kind;
    std::string text;
    SourceSpan span;
};

// tokenize tokenizes the input source into the stream consumed by later passes.
std::vector<Token> tokenize(const std::string& source);
// tokenKindName implements the tokenKindName behavior for the tokenizer.h module.
std::string tokenKindName(TokenKind kind);
