/*
 * tokenizer.h
 *
 * Declares token kinds, spans, and token structures used throughout the compiler.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include "errors.h"

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

// TokenSpan is the token's range relative to the string passed to tokenize().
// Diagnostic SourceSpan values use canonical offsets in the original file.
struct TokenSpan {
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
};

// Token implements the Token behavior for the tokenizer.h module.
struct Token {
    TokenKind kind;
    std::string text;
    TokenSpan span;
    SourceSpan sourceSpan;
};

// tokenize tokenizes the input source into the stream consumed by later passes.
std::vector<Token> tokenize(const std::string& source);
std::vector<Token> tokenize(const std::string& source, SourceSpan sourceSpan);
// tokenKindName implements the tokenKindName behavior for the tokenizer.h module.
std::string tokenKindName(TokenKind kind);
