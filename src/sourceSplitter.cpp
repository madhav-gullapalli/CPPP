/*
 * sourceSplitter.cpp
 *
 * Groups the canonical whole-file token stream into logical statements for
 * the legacy statement lowering pass. This is deliberately token-driven: raw
 * the complete source file is scanned exactly once by tokenizer.cpp.
 */

#include "sourceSplitter.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
struct PhysicalFragment {
    std::vector<Token> codeTokens;
    bool terminatesStatement = false;
    bool hasComment = false;
    Token comment;
};

struct LogicalFragment {
    std::vector<Token> codeTokens;
    bool hasComment = false;
    Token comment;
};

bool isLiteralBraceContext(const std::vector<Token>& tokens) {
    if (tokens.empty()) {
        return false;
    }
    const Token& previous = tokens.back();
    if (previous.kind == TokenKind::RightParen) {
        return false;
    }
    if (previous.kind == TokenKind::LeftParen ||
        previous.kind == TokenKind::LeftBracket ||
        previous.kind == TokenKind::LeftBrace ||
        previous.kind == TokenKind::Comma ||
        previous.kind == TokenKind::Equals) {
        return true;
    }
    if (previous.kind == TokenKind::Identifier && previous.text == "return") {
        return true;
    }
    return previous.kind == TokenKind::Operator && previous.text != ".";
}

bool isUnterminatedQuotedToken(const Token& token) {
    if (token.kind != TokenKind::String && token.kind != TokenKind::Char) {
        return false;
    }
    return token.text.size() < 2 || token.text.front() != token.text.back();
}

bool containsUnterminatedQuotedToken(const std::vector<Token>& tokens) {
    return std::any_of(tokens.begin(), tokens.end(), isUnterminatedQuotedToken);
}

bool looksLikeCompleteBareCall(const std::vector<Token>& tokens) {
    if (tokens.size() < 3 ||
        tokens.front().kind != TokenKind::Identifier ||
        tokens[1].kind != TokenKind::LeftParen ||
        tokens.back().kind != TokenKind::RightParen) {
        return false;
    }

    int depth = 0;
    for (size_t index = 1; index < tokens.size(); ++index) {
        if (tokens[index].kind == TokenKind::LeftParen) {
            ++depth;
        } else if (tokens[index].kind == TokenKind::RightParen) {
            --depth;
            if (depth == 0 && index + 1 != tokens.size()) {
                return false;
            }
            if (depth < 0) {
                return false;
            }
        }
    }
    return depth == 0;
}

std::vector<PhysicalFragment> splitPhysicalFragments(const TokenStream& stream) {
    std::vector<PhysicalFragment> fragments;
    PhysicalFragment current;
    int parenDepth = 0;
    int bracketDepth = 0;
    int literalBraceDepth = 0;
    int currentLine = 0;

    const auto flush = [&](bool terminates) {
        if (current.codeTokens.empty() && !current.hasComment) {
            return;
        }
        current.terminatesStatement = terminates;
        fragments.push_back(std::move(current));
        current = PhysicalFragment{};
        currentLine = 0;
    };

    for (const Token& token : stream.tokens) {
        if (token.kind == TokenKind::EndOfFile) {
            break;
        }

        if (currentLine != 0 && token.span.startLine > currentLine) {
            flush(false);
        }
        currentLine = std::max(currentLine, token.span.endLine);

        if (token.kind == TokenKind::LineComment) {
            current.hasComment = true;
            current.comment = token;
            flush(false);
            continue;
        }

        if (token.kind == TokenKind::LeftParen) {
            current.codeTokens.push_back(token);
            ++parenDepth;
            continue;
        }
        if (token.kind == TokenKind::RightParen) {
            current.codeTokens.push_back(token);
            if (parenDepth > 0) {
                --parenDepth;
            }
            continue;
        }
        if (token.kind == TokenKind::LeftBracket) {
            current.codeTokens.push_back(token);
            ++bracketDepth;
            continue;
        }
        if (token.kind == TokenKind::RightBracket) {
            current.codeTokens.push_back(token);
            if (bracketDepth > 0) {
                --bracketDepth;
            }
            continue;
        }
        if (token.kind == TokenKind::LeftBrace) {
            if (literalBraceDepth > 0 || isLiteralBraceContext(current.codeTokens)) {
                current.codeTokens.push_back(token);
                ++literalBraceDepth;
            } else if (parenDepth == 0 && bracketDepth == 0) {
                current.codeTokens.push_back(token);
                flush(true);
            } else {
                current.codeTokens.push_back(token);
                ++literalBraceDepth;
            }
            continue;
        }
        if (token.kind == TokenKind::RightBrace) {
            if (literalBraceDepth > 0) {
                current.codeTokens.push_back(token);
                --literalBraceDepth;
            } else if (parenDepth == 0 && bracketDepth == 0) {
                flush(false);
                current.codeTokens.push_back(token);
                flush(true);
            } else {
                current.codeTokens.push_back(token);
            }
            continue;
        }

        current.codeTokens.push_back(token);
        if (token.kind == TokenKind::Semicolon &&
            parenDepth == 0 && bracketDepth == 0 && literalBraceDepth == 0) {
            flush(true);
        }
    }
    flush(false);
    return fragments;
}

std::vector<LogicalFragment> mergeLogicalFragments(
    const std::vector<PhysicalFragment>& physicalFragments
) {
    std::vector<LogicalFragment> merged;
    LogicalFragment pending;

    const auto flush = [&]() {
        if (pending.codeTokens.empty() && !pending.hasComment) {
            return;
        }
        merged.push_back(std::move(pending));
        pending = LogicalFragment{};
    };

    for (const PhysicalFragment& fragment : physicalFragments) {
        if (fragment.codeTokens.empty()) {
            if (fragment.hasComment) {
                if (pending.codeTokens.empty()) {
                    LogicalFragment commentOnly;
                    commentOnly.hasComment = true;
                    commentOnly.comment = fragment.comment;
                    merged.push_back(std::move(commentOnly));
                } else {
                    pending.hasComment = true;
                    pending.comment = fragment.comment;
                }
            }
            continue;
        }

        pending.codeTokens.insert(
            pending.codeTokens.end(),
            fragment.codeTokens.begin(),
            fragment.codeTokens.end()
        );
        if (fragment.hasComment) {
            pending.hasComment = true;
            pending.comment = fragment.comment;
        }

        if (fragment.terminatesStatement ||
            containsUnterminatedQuotedToken(fragment.codeTokens) ||
            looksLikeCompleteBareCall(pending.codeTokens)) {
            flush();
        }
    }
    flush();

    std::vector<LogicalFragment> attached;
    for (LogicalFragment& fragment : merged) {
        if (fragment.codeTokens.size() == 1 &&
            fragment.codeTokens[0].kind == TokenKind::LeftBrace &&
            !fragment.hasComment &&
            !attached.empty()) {
            attached.back().codeTokens.push_back(fragment.codeTokens[0]);
            continue;
        }
        attached.push_back(std::move(fragment));
    }
    return attached;
}

std::string gapBetween(
    const TokenStream& stream,
    const Token& previous,
    const Token& next
) {
    if (previous.span.endLine != next.span.startLine ||
        next.span.startOffset < previous.span.endOffset ||
        previous.span.endOffset > stream.source.size()) {
        return " ";
    }
    const size_t end = std::min(next.span.startOffset, stream.source.size());
    return stream.source.substr(previous.span.endOffset, end - previous.span.endOffset);
}

SourceSpan insertionAfter(const TokenStream& stream, const Token* token) {
    if (token != nullptr && token->sourceSpan.valid()) {
        return {
            token->sourceSpan.source,
            token->sourceSpan.endOffset,
            token->sourceSpan.endOffset
        };
    }
    if (stream.sourceSpan.valid()) {
        return {
            stream.sourceSpan.source,
            stream.sourceSpan.startOffset,
            stream.sourceSpan.startOffset
        };
    }
    return {};
}

SourceFragment renderFragment(
    const TokenStream& stream,
    const LogicalFragment& logical
) {
    SourceFragment fragment;
    std::string codeText;
    const Token* previous = nullptr;

    for (const Token& canonical : logical.codeTokens) {
        if (previous != nullptr) {
            codeText += gapBetween(stream, *previous, canonical);
        }
        const size_t localStart = codeText.size();
        codeText += canonical.text;
        Token local = canonical;
        local.span = {
            1,
            static_cast<int>(localStart + 1),
            1,
            static_cast<int>(codeText.size()),
            localStart,
            codeText.size()
        };
        fragment.tokens.push_back(std::move(local));
        previous = &canonical;
    }

    const Token* lastCode = logical.codeTokens.empty() ? nullptr : &logical.codeTokens.back();
    const size_t eofOffset = codeText.size();
    fragment.tokens.push_back({
        TokenKind::EndOfFile,
        "",
        {1, static_cast<int>(eofOffset + 1), 1, static_cast<int>(eofOffset + 1), eofOffset, eofOffset},
        insertionAfter(stream, lastCode)
    });

    if (logical.hasComment) {
        fragment.commentText = logical.comment.text;
    }

    const Token* first = !logical.codeTokens.empty()
        ? &logical.codeTokens.front()
        : (logical.hasComment ? &logical.comment : nullptr);
    const Token* last = logical.hasComment
        ? &logical.comment
        : (!logical.codeTokens.empty() ? &logical.codeTokens.back() : nullptr);
    if (first != nullptr) {
        fragment.lineNumber = first->span.startLine;
        fragment.startColumn = first->span.startColumn;
    }
    if (last != nullptr) {
        fragment.endLineNumber = last->span.endLine;
        fragment.endColumn = last->span.endColumn;
    }

    size_t spanStart = std::numeric_limits<size_t>::max();
    size_t spanEnd = 0;
    SourceId source;
    const auto includeSpan = [&](const Token& token) {
        if (!token.sourceSpan.valid()) {
            return;
        }
        source = token.sourceSpan.source;
        spanStart = std::min(spanStart, token.sourceSpan.startOffset);
        spanEnd = std::max(spanEnd, token.sourceSpan.endOffset);
    };
    for (const Token& token : logical.codeTokens) {
        includeSpan(token);
    }
    if (logical.hasComment) {
        includeSpan(logical.comment);
    }
    if (source.value != 0 && spanStart != std::numeric_limits<size_t>::max()) {
        fragment.sourceSpan = {source, spanStart, spanEnd};
    }
    return fragment;
}
}

std::vector<SourceFragment> splitTokenStream(const TokenStream& tokenStream) {
    const std::vector<PhysicalFragment> physical = splitPhysicalFragments(tokenStream);
    const std::vector<LogicalFragment> logical = mergeLogicalFragments(physical);
    std::vector<SourceFragment> fragments;
    fragments.reserve(logical.size());
    for (const LogicalFragment& fragment : logical) {
        fragments.push_back(renderFragment(tokenStream, fragment));
    }
    return fragments;
}
