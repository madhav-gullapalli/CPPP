/*
 * sourceSplitter.cpp
 *
 * Splits source text into statement fragments and handles continuation and semicolon behavior.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "sourceSplitter.h"

#include <string>
#include <vector>

namespace {
// trim removes surrounding whitespace from a string.
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// firstCodeColumn implements the firstCodeColumn behavior for the sourceSplitter.cpp module.
int firstCodeColumn(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    return static_cast<int>((first == std::string::npos ? 0 : first) + 1);
}

bool isLiteralBraceContext(const std::string& text, size_t braceIndex) {
    if (braceIndex == 0) {
        return false;
    }

    size_t index = braceIndex;
    while (index > 0) {
        --index;
        if (text[index] == ' ' || text[index] == '\t' || text[index] == '\r' || text[index] == '\n') {
            continue;
        }

        const char ch = text[index];
        if (ch == ')') {
            return false;
        }

        return ch == '=' || ch == ',' || ch == ':' || ch == '(' || ch == '[' ||
            ch == '{' || ch == '+' || ch == '-' || ch == '*' || ch == '/' ||
            ch == '%' || ch == '<' || ch == '>' || ch == '&' || ch == '|' ||
            ch == '^' || ch == '!';
    }

    return false;
}

// splitSemicolonStatements splits the input into smaller logical pieces.
std::vector<SourceFragment> splitSemicolonStatements(const std::string& line, int lineNumber) {
    const size_t commentStart = findLineCommentStart(line);
    const std::string codeText = commentStart == std::string::npos ? line : line.substr(0, commentStart);
    const std::string commentText = commentStart == std::string::npos ? "" : line.substr(commentStart);
    std::vector<SourceFragment> fragments;
    bool inString = false;
    bool inChar = false;
    bool escaped = false;
    int parenDepth = 0;
    int literalBraceDepth = 0;
    size_t start = 0;

    for (size_t i = 0; i < codeText.size(); ++i) {
        const char ch = codeText[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if ((inString || inChar) && ch == '\\') {
            escaped = true;
            continue;
        }
        if (!inChar && ch == '"') {
            inString = !inString;
            continue;
        }
        if (!inString && ch == '\'') {
            inChar = !inChar;
            continue;
        }
        if (!inString && !inChar && ch == '(') {
            ++parenDepth;
            continue;
        }
        if (!inString && !inChar && ch == ')' && parenDepth > 0) {
            --parenDepth;
            continue;
        }
        if (!inString && !inChar && ch == '{') {
            if (literalBraceDepth > 0 || isLiteralBraceContext(codeText, i)) {
                ++literalBraceDepth;
                continue;
            }
        }
        if (!inString && !inChar && ch == '{' && parenDepth == 0 && literalBraceDepth == 0) {
            const std::string fragment = std::string(start, ' ') + codeText.substr(start, i - start + 1);
            fragments.push_back({lineNumber, firstCodeColumn(fragment), fragment});
            start = i + 1;
            continue;
        }
        if (!inString && !inChar && ch == '}' && literalBraceDepth > 0) {
            --literalBraceDepth;
            continue;
        }
        if (!inString && !inChar && ch == '}' && parenDepth == 0 && literalBraceDepth == 0) {
            if (i > start) {
                const std::string fragment = std::string(start, ' ') + codeText.substr(start, i - start);
                fragments.push_back({lineNumber, firstCodeColumn(fragment), fragment});
            }
            const std::string fragment = std::string(i, ' ') + codeText.substr(i, 1);
            fragments.push_back({lineNumber, firstCodeColumn(fragment), fragment});
            start = i + 1;
            continue;
        }
        if (!inString && !inChar && ch == ';' && parenDepth == 0 && literalBraceDepth == 0) {
            const std::string fragment = std::string(start, ' ') + codeText.substr(start, i - start + 1);
            fragments.push_back({lineNumber, firstCodeColumn(fragment), fragment});
            start = i + 1;
        }
    }

    std::string remainder = start < codeText.size() ? std::string(start, ' ') + codeText.substr(start) : "";
    if (!commentText.empty()) {
        if (trim(remainder).empty()) {
            remainder = std::string(commentStart, ' ') + commentText;
        } else {
            remainder += commentText;
        }
    }
    if (!trim(remainder).empty() || fragments.empty()) {
        fragments.push_back({lineNumber, firstCodeColumn(remainder), remainder});
    }
    for (SourceFragment& fragment : fragments) {
        fragment.endLineNumber = lineNumber;
        const size_t lastCode = fragment.text.find_last_not_of(" \t\r\n");
        fragment.endColumn = static_cast<int>(
            lastCode == std::string::npos ? fragment.startColumn : lastCode + 1
        );
    }
    return fragments;
}

// fragmentTerminatesStatement implements the fragmentTerminatesStatement behavior for the sourceSplitter.cpp module.
bool fragmentTerminatesStatement(const std::string& text) {
    const std::string trimmed = trim(text);
    if (trimmed.empty()) {
        return false;
    }

    const char last = trimmed.back();
    return last == ';' || last == '{' || last == '}';
}

bool hasUnterminatedQuotedLiteral(const std::string& text) {
    bool inString = false;
    bool inChar = false;
    bool escaped = false;
    for (char ch : text) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if ((inString || inChar) && ch == '\\') {
            escaped = true;
            continue;
        }
        if (!inChar && ch == '"') {
            inString = !inString;
            continue;
        }
        if (!inString && ch == '\'') {
            inChar = !inChar;
        }
    }
    return inString || inChar;
}

// unmatchedParenthesisDepth implements the unmatchedParenthesisDepth behavior for the sourceSplitter.cpp module.
int unmatchedParenthesisDepth(const std::string& text) {
    int parenDepth = 0;
    bool inString = false;
    bool inChar = false;
    bool escaped = false;

    for (char ch : text) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if ((inString || inChar) && ch == '\\') {
            escaped = true;
            continue;
        }

        if (!inChar && ch == '"') {
            inString = !inString;
            continue;
        }

        if (!inString && ch == '\'') {
            inChar = !inChar;
            continue;
        }

        if (inString || inChar) {
            continue;
        }

        if (ch == '(') {
            ++parenDepth;
        } else if (ch == ')' && parenDepth > 0) {
            --parenDepth;
        }
    }

    return parenDepth;
}

// mergeContinuationFragments implements the mergeContinuationFragments behavior for the sourceSplitter.cpp module.
std::vector<SourceFragment> mergeContinuationFragments(const std::vector<SourceFragment>& fragments) {
    std::vector<SourceFragment> merged;
    SourceFragment pending{0, 1, ""};
    std::string pendingComment;

    const auto flushPending = [&]() {
        if (trim(pending.text).empty() && pendingComment.empty()) {
            pending = {0, 1, ""};
            return;
        }

        std::string text = trim(pending.text);
        if (!pendingComment.empty()) {
            if (!text.empty()) {
                text += " ";
            }
            text += pendingComment;
        }

        SourceFragment mergedFragment{
            pending.lineNumber,
            pending.startColumn,
            text
        };
        mergedFragment.endLineNumber = pending.endLineNumber;
        mergedFragment.endColumn = pending.endColumn;
        merged.push_back(std::move(mergedFragment));
        pending = {0, 1, ""};
        pendingComment.clear();
    };

    for (const SourceFragment& fragment : fragments) {
        const size_t commentStart = findLineCommentStart(fragment.text);
        const std::string codePart = commentStart == std::string::npos ? fragment.text : fragment.text.substr(0, commentStart);
        const std::string commentPart = commentStart == std::string::npos ? "" : trim(fragment.text.substr(commentStart));
        const std::string trimmedCode = trim(codePart);

        if (trimmedCode.empty()) {
            if (!commentPart.empty()) {
                if (trim(pending.text).empty()) {
                    SourceFragment commentFragment = fragment;
                    commentFragment.text = commentPart;
                    merged.push_back(std::move(commentFragment));
                } else {
                    pendingComment = commentPart;
                }
            }
            continue;
        }

        if (pending.lineNumber == 0) {
            pending.lineNumber = fragment.lineNumber;
            pending.startColumn = fragment.startColumn;
            pending.text = trimmedCode;
            pending.endLineNumber = fragment.endLineNumber;
            pending.endColumn = fragment.endColumn;
        } else {
            pending.text += " " + trimmedCode;
            pending.endLineNumber = fragment.endLineNumber;
            pending.endColumn = fragment.endColumn;
        }

        if (!commentPart.empty()) {
            pendingComment = commentPart;
        }

        if (hasUnterminatedQuotedLiteral(codePart) ||
            (fragmentTerminatesStatement(codePart) && unmatchedParenthesisDepth(pending.text) == 0)) {
            flushPending();
        }
    }

    flushPending();
    return merged;
}

// attachDetachedOpeningBraces implements the attachDetachedOpeningBraces behavior for the sourceSplitter.cpp module.
std::vector<SourceFragment> attachDetachedOpeningBraces(const std::vector<SourceFragment>& fragments) {
    std::vector<SourceFragment> attached;
    for (const SourceFragment& fragment : fragments) {
        if (trim(fragment.text) == "{" && !attached.empty()) {
            attached.back().text += " {";
            attached.back().endLineNumber = fragment.endLineNumber;
            attached.back().endColumn = fragment.endColumn;
            continue;
        }

        attached.push_back(fragment);
    }

    return attached;
}
}

// findLineCommentStart implements the findLineCommentStart behavior for the sourceSplitter.cpp module.
size_t findLineCommentStart(const std::string& text) {
    bool inString = false;
    bool inChar = false;
    bool escaped = false;

    for (size_t i = 0; i + 1 < text.size(); ++i) {
        const char ch = text[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if ((inString || inChar) && ch == '\\') {
            escaped = true;
            continue;
        }

        if (!inChar && ch == '"') {
            inString = !inString;
            continue;
        }

        if (!inString && ch == '\'') {
            inChar = !inChar;
            continue;
        }

        if (!inString && !inChar && ch == '/' && text[i + 1] == '/') {
            return i;
        }
    }

    return std::string::npos;
}

// splitSourceFragments splits the input into smaller logical pieces.
std::vector<SourceFragment> splitSourceFragments(
    std::istream& input,
    std::map<int, std::string>& sourceLines,
    const std::string& sourceFile
) {
    std::vector<SourceFragment> sourceFragments;
    std::string rawLine;
    int rawLineNumber = 0;
    while (std::getline(input, rawLine)) {
        ++rawLineNumber;
        sourceLines[rawLineNumber] = rawLine;
        for (const SourceFragment& fragment : splitSemicolonStatements(rawLine, rawLineNumber)) {
            sourceFragments.push_back(fragment);
        }
    }

    std::vector<SourceFragment> fragments =
        attachDetachedOpeningBraces(mergeContinuationFragments(sourceFragments));
    for (SourceFragment& fragment : fragments) {
        const int endLine = fragment.endLineNumber == 0
            ? fragment.lineNumber
            : fragment.endLineNumber;
        fragment.sourceSpan = sourceSpanForRange(
            sourceFile,
            sourceLines,
            fragment.lineNumber,
            fragment.startColumn,
            endLine,
            fragment.endColumn
        );
    }
    return fragments;
}
