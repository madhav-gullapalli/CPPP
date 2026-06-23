#include "sourceSplitter.h"

#include <string>
#include <vector>

namespace {
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::vector<std::string> splitSemicolonStatements(const std::string& line) {
    const size_t commentStart = findLineCommentStart(line);
    const std::string codeText = commentStart == std::string::npos ? line : line.substr(0, commentStart);
    const std::string commentText = commentStart == std::string::npos ? "" : line.substr(commentStart);
    std::vector<std::string> fragments;
    bool inString = false;
    bool inChar = false;
    bool escaped = false;
    int parenDepth = 0;
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
        if (!inString && !inChar && ch == '{' && parenDepth == 0) {
            fragments.push_back(std::string(start, ' ') + codeText.substr(start, i - start + 1));
            start = i + 1;
            continue;
        }
        if (!inString && !inChar && ch == '}' && parenDepth == 0) {
            if (i > start) {
                fragments.push_back(std::string(start, ' ') + codeText.substr(start, i - start));
            }
            fragments.push_back(std::string(i, ' ') + codeText.substr(i, 1));
            start = i + 1;
            continue;
        }
        if (!inString && !inChar && ch == ';' && parenDepth == 0) {
            fragments.push_back(std::string(start, ' ') + codeText.substr(start, i - start + 1));
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
        fragments.push_back(remainder);
    }
    return fragments;
}

bool fragmentTerminatesStatement(const std::string& text) {
    const std::string trimmed = trim(text);
    if (trimmed.empty()) {
        return false;
    }

    const char last = trimmed.back();
    return last == ';' || last == '{' || last == '}';
}

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

std::vector<SourceFragment> mergeContinuationFragments(const std::vector<SourceFragment>& fragments) {
    std::vector<SourceFragment> merged;
    SourceFragment pending{0, ""};
    std::string pendingComment;

    const auto flushPending = [&]() {
        if (trim(pending.text).empty() && pendingComment.empty()) {
            pending = {0, ""};
            return;
        }

        std::string text = trim(pending.text);
        if (!pendingComment.empty()) {
            if (!text.empty()) {
                text += " ";
            }
            text += pendingComment;
        }

        merged.push_back({pending.lineNumber, text});
        pending = {0, ""};
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
                    merged.push_back({fragment.lineNumber, commentPart});
                } else {
                    pendingComment = commentPart;
                }
            }
            continue;
        }

        if (pending.lineNumber == 0) {
            pending.lineNumber = fragment.lineNumber;
            pending.text = trimmedCode;
        } else {
            pending.text += " " + trimmedCode;
        }

        if (!commentPart.empty()) {
            pendingComment = commentPart;
        }

        if (fragmentTerminatesStatement(codePart) && unmatchedParenthesisDepth(pending.text) == 0) {
            flushPending();
        }
    }

    flushPending();
    return merged;
}

std::vector<SourceFragment> attachDetachedOpeningBraces(const std::vector<SourceFragment>& fragments) {
    std::vector<SourceFragment> attached;
    for (const SourceFragment& fragment : fragments) {
        if (trim(fragment.text) == "{" && !attached.empty()) {
            attached.back().text += " {";
            continue;
        }

        attached.push_back(fragment);
    }

    return attached;
}
}

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

std::vector<SourceFragment> splitSourceFragments(std::istream& input, std::map<int, std::string>& sourceLines) {
    std::vector<SourceFragment> sourceFragments;
    std::string rawLine;
    int rawLineNumber = 0;
    while (std::getline(input, rawLine)) {
        ++rawLineNumber;
        sourceLines[rawLineNumber] = rawLine;
        for (const std::string& fragment : splitSemicolonStatements(rawLine)) {
            sourceFragments.push_back({rawLineNumber, fragment});
        }
    }

    return attachDetachedOpeningBraces(mergeContinuationFragments(sourceFragments));
}
