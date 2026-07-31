/*
 * errors.cpp
 *
 * Collects diagnostics, renders user-facing error messages, and tracks source ranges for failures.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "errors.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <regex>
#include <utility>
#include <vector>

namespace {
// RuntimeFailureLocation provides runtime support for generated code.
struct RuntimeFailureLocation {
    int lineNumber;
    int column;
    char operation;
};

SourceManager& diagnosticSources() {
    static SourceManager sources;
    return sources;
}

std::vector<Diagnostic>& diagnostics() {
    static std::vector<Diagnostic> errors;
    return errors;
}

size_t levenshteinDistance(const std::string& left, const std::string& right) {
    std::vector<size_t> previous(right.size() + 1);
    std::vector<size_t> current(right.size() + 1);
    for (size_t index = 0; index <= right.size(); ++index) {
        previous[index] = index;
    }

    for (size_t leftIndex = 1; leftIndex <= left.size(); ++leftIndex) {
        current[0] = leftIndex;
        for (size_t rightIndex = 1; rightIndex <= right.size(); ++rightIndex) {
            const size_t substitutionCost =
                left[leftIndex - 1] == right[rightIndex - 1] ? 0 : 1;
            current[rightIndex] = std::min({
                previous[rightIndex] + 1,
                current[rightIndex - 1] + 1,
                previous[rightIndex - 1] + substitutionCost
            });
        }
        previous.swap(current);
    }

    return previous.back();
}

char automaticClosingDelimiter(const std::string& message) {
    if (message.rfind("unclosed parenthesis", 0) == 0) {
        return ')';
    }
    if (message.rfind("unclosed bracket", 0) == 0) {
        return ']';
    }
    if (message == "unclosed brace in set or map literal") {
        return '}';
    }
    return '\0';
}

bool isInsertionBoundary(char ch, char closingDelimiter) {
    if (ch == ';') {
        return true;
    }
    if (closingDelimiter == ')') {
        return ch == ')' || ch == ']' || ch == '}';
    }
    if (closingDelimiter == ']') {
        return ch == ')' || ch == '}';
    }
    return ch == ')' || ch == ']';
}

int delimiterInsertionColumn(
    const std::string& sourceLine,
    int searchColumn,
    char closingDelimiter
) {
    bool inString = false;
    bool inChar = false;
    bool escaped = false;
    const size_t start = static_cast<size_t>(std::max(0, searchColumn - 1));
    for (size_t index = std::min(start, sourceLine.size()); index < sourceLine.size(); ++index) {
        const char ch = sourceLine[index];
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
        if (!inString && !inChar &&
            (isInsertionBoundary(ch, closingDelimiter) ||
             (closingDelimiter == ')' && ch == '{'))) {
            return static_cast<int>(index) + 1;
        }
    }

    const size_t lastCode = sourceLine.find_last_not_of(" \t\r\n");
    return lastCode == std::string::npos
        ? 1
        : static_cast<int>(lastCode) + 2;
}

bool isUtf8Continuation(unsigned char ch) {
    return (ch & 0xc0U) == 0x80U;
}

int displayWidth(const std::string& text, int startingColumn = 1) {
    int column = std::max(1, startingColumn);
    const int initialColumn = column;
    for (unsigned char ch : text) {
        if (ch == '\t') {
            column += 4 - ((column - 1) % 4);
        } else if (!isUtf8Continuation(ch)) {
            ++column;
        }
    }
    return column - initialColumn;
}

std::string expandTabs(const std::string& text) {
    std::string expanded;
    int column = 1;
    for (char ch : text) {
        if (ch == '\t') {
            const int spaces = 4 - ((column - 1) % 4);
            expanded.append(static_cast<size_t>(spaces), ' ');
            column += spaces;
        } else {
            expanded += ch;
            if (!isUtf8Continuation(static_cast<unsigned char>(ch))) {
                ++column;
            }
        }
    }
    return expanded;
}

std::string severityName(DiagnosticSeverity severity) {
    return severity == DiagnosticSeverity::Warning ? "warning" : "error";
}

const DiagnosticLabel* primaryLabel(const Diagnostic& diagnostic) {
    for (const DiagnosticLabel& label : diagnostic.labels) {
        if (label.primary && label.span.valid()) {
            return &label;
        }
    }
    for (const DiagnosticLabel& label : diagnostic.labels) {
        if (label.span.valid()) {
            return &label;
        }
    }
    return nullptr;
}

int decimalWidth(int value) {
    return static_cast<int>(std::to_string(std::max(1, value)).size());
}

std::string leftPadNumber(int value, int width) {
    const std::string number = std::to_string(value);
    return std::string(static_cast<size_t>(std::max(0, width - static_cast<int>(number.size()))), ' ') + number;
}

int printLabel(std::ostream& stream, const DiagnosticLabel& label) {
    SourceManager& sources = diagnosticSources();
    const SourceLocation start = sources.location(label.span.source, label.span.startOffset);
    const SourceLocation end = sources.location(label.span.source, label.span.endOffset);
    const std::string firstLine = sources.lineText(label.span.source, start.line);
    const int gutterWidth = decimalWidth(std::max(start.line, end.line));
    stream << (label.primary ? " --> " : " ::: ")
           << sources.sourceFile(label.span.source) << ':'
           << start.line << ':' << start.column << '\n';
    stream << std::string(static_cast<size_t>(gutterWidth + 1), ' ') << "|\n";
    if (firstLine.empty()) {
        return gutterWidth;
    }

    const size_t startByte = static_cast<size_t>(std::max(0, start.byteColumn - 1));
    const int displayColumn = displayWidth(firstLine.substr(0, std::min(startByte, firstLine.size()))) + 1;
    int width = 1;
    if (start.line == end.line && label.span.endOffset > label.span.startOffset) {
        const size_t endByte = static_cast<size_t>(std::max(0, end.byteColumn - 1));
        width = std::max(
            1,
            displayWidth(firstLine.substr(
                std::min(startByte, firstLine.size()),
                std::min(endByte, firstLine.size()) - std::min(startByte, firstLine.size())
            ), displayColumn)
        );
    } else if (start.line != end.line) {
        width = std::max(
            1,
            displayWidth(firstLine.substr(std::min(startByte, firstLine.size())), displayColumn)
        );
    }

    stream << leftPadNumber(start.line, gutterWidth) << " | " << expandTabs(firstLine) << '\n';
    stream << std::string(static_cast<size_t>(gutterWidth + 1), ' ') << "| "
           << std::string(static_cast<size_t>(displayColumn - 1), ' ');
    if (label.primary) {
        stream << '^';
        if (width > 1) {
            stream << std::string(static_cast<size_t>(width - 1), '~');
        }
    } else {
        stream << std::string(static_cast<size_t>(width), '-');
    }
    if (!label.message.empty()) {
        stream << ' ' << label.message;
    }
    stream << '\n';

    if (start.line == end.line) {
        return gutterWidth;
    }

    for (int lineNumber = start.line + 1; lineNumber <= end.line; ++lineNumber) {
        const std::string sourceLine = sources.lineText(label.span.source, lineNumber);
        if (sourceLine.empty() && lineNumber == end.line && end.column == 1) {
            break;
        }
        const int lineWidth = lineNumber == end.line
            ? std::max(1, displayWidth(sourceLine.substr(0, static_cast<size_t>(std::max(0, end.column - 1)))))
            : std::max(1, displayWidth(sourceLine));
        stream << leftPadNumber(lineNumber, gutterWidth) << " | " << expandTabs(sourceLine) << '\n';
        stream << std::string(static_cast<size_t>(gutterWidth + 1), ' ') << "| "
               << (label.primary ? "^" : "-");
        if (lineWidth > 1) {
            stream << std::string(
                static_cast<size_t>(lineWidth - 1),
                label.primary ? '~' : '-'
            );
        }
        stream << '\n';
    }
    return gutterWidth;
}

void printSubdiagnostic(std::ostream& stream, int gutterWidth, const std::string& kind, const std::string& message) {
    stream << std::string(static_cast<size_t>(gutterWidth + 1), ' ')
           << "= " << kind << ": " << message << '\n';
}

void printDiagnostic(std::ostream& stream, const Diagnostic& diagnostic) {
    const DiagnosticLabel* primary = primaryLabel(diagnostic);
    if (primary == nullptr) {
        stream << severityName(diagnostic.severity) << ": " << diagnostic.message << '\n';
        return;
    }

    stream << severityName(diagnostic.severity);
    if (!diagnostic.code.empty()) {
        stream << '[' << diagnostic.code << ']';
    }
    stream << ": " << diagnostic.message << '\n';

    int gutterWidth = printLabel(stream, *primary);
    for (const DiagnosticLabel& label : diagnostic.labels) {
        if (&label != primary && label.span.valid()) {
            gutterWidth = std::max(gutterWidth, printLabel(stream, label));
        }
    }
    const bool hasSubdiagnostics =
        !diagnostic.notes.empty() ||
        !diagnostic.helps.empty() ||
        !diagnostic.suggestions.empty();
    if (hasSubdiagnostics) {
        stream << std::string(static_cast<size_t>(gutterWidth + 1), ' ') << "|\n";
    }
    for (const std::string& note : diagnostic.notes) {
        printSubdiagnostic(stream, gutterWidth, "note", note);
    }
    for (const std::string& help : diagnostic.helps) {
        printSubdiagnostic(stream, gutterWidth, "help", help);
    }
    for (const DiagnosticSuggestion& suggestion : diagnostic.suggestions) {
        printSubdiagnostic(stream, gutterWidth, "help", suggestion.message);
    }
}

Diagnostic pointDiagnostic(
    const std::string& sourceFile,
    int lineNumber,
    int column,
    const std::string& message,
    const std::map<int, std::string>& sourceLines
) {
    Diagnostic diagnostic;
    diagnostic.message = message;
    diagnostic.labels.push_back({
        sourceTokenSpan(sourceFile, sourceLines, lineNumber, column),
        "",
        true
    });
    addAutomaticSyntaxSuggestion(
        diagnostic,
        sourceFile,
        lineNumber,
        column,
        sourceLines
    );
    return diagnostic;
}

void printRuntimeDiagnostic(
    const std::string& sourceFile,
    int lineNumber,
    int column,
    const std::string& message,
    const std::map<int, std::string>& sourceLines
) {
    printDiagnostic(std::cout, pointDiagnostic(
        sourceFile,
        lineNumber,
        column,
        message,
        sourceLines
    ));
}

// lowerText lowers the construct into the internal code-generation form.
std::string lowerText(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

// looksLikeRuntimeArithmeticOperator implements the looksLikeRuntimeArithmeticOperator behavior for the errors.cpp module.
bool looksLikeRuntimeArithmeticOperator(const std::string& line, size_t index) {
    const char ch = line[index];
    if (ch == '%') {
        return true;
    }

    if (ch != '/') {
        return false;
    }

    return index + 1 >= line.size() || line[index + 1] != '/';
}

// likelyRuntimeFailureLocation implements the likelyRuntimeFailureLocation behavior for the errors.cpp module.
RuntimeFailureLocation likelyRuntimeFailureLocation(const std::map<int, std::string>& sourceLines) {
    for (const auto& sourceLine : sourceLines) {
        bool inString = false;
        bool inChar = false;
        bool escaped = false;
        const std::string& text = sourceLine.second;

        for (size_t i = 0; i < text.size(); ++i) {
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

            if (!inString && !inChar && i + 1 < text.size() && ch == '/' && text[i + 1] == '/') {
                break;
            }

            if (!inString && !inChar && looksLikeRuntimeArithmeticOperator(text, i)) {
                return {sourceLine.first, static_cast<int>(i) + 1, text[i]};
            }
        }
    }

    return {sourceLines.empty() ? 1 : sourceLines.begin()->first, 1, '\0'};
}

std::string readableRuntimeMessageForLog(
    const std::string& loweredLogText,
    const RuntimeFailureLocation& location
) {
    if (loweredLogText.find("overflow") != std::string::npos) {
        return "runtime error: integer overflow";
    }

    if (loweredLogText.find("division by zero") != std::string::npos ||
        loweredLogText.find("divide by zero") != std::string::npos ||
        loweredLogText.find("integer divide") != std::string::npos ||
        loweredLogText.find("floating point exception") != std::string::npos) {
        if (location.operation == '%') {
            return "runtime error: modulo by zero";
        }
        return "runtime error: division by zero";
    }

    if (location.operation == '/') {
        return "runtime error: division by zero";
    }
    if (location.operation == '%') {
        return "runtime error: modulo by zero";
    }

    return "runtime error: generated program stopped before finishing";
}

// readableRuntimeMessage reads the requested input or source data.
std::string readableRuntimeMessage(const std::string& message) {
    const std::string lowered = lowerText(message);
    if (lowered.find("overflow") != std::string::npos) {
        return "runtime error: integer overflow";
    }
    if (lowered.find("division by zero") != std::string::npos ||
        lowered.find("divide by zero") != std::string::npos ||
        lowered.find("integer divide") != std::string::npos) {
        return "runtime error: division by zero";
    }
    if (lowered.find("modulo by zero") != std::string::npos ||
        lowered.find("remainder by zero") != std::string::npos) {
        return "runtime error: modulo by zero";
    }

    return "runtime error: " + message;
}
}

const SourceManager::SourceFile* SourceManager::find(SourceId source) const {
    for (const SourceFile& candidate : sources) {
        if (candidate.id == source) {
            return &candidate;
        }
    }
    return nullptr;
}

SourceManager::SourceFile* SourceManager::findByPath(const std::string& sourceFile) {
    for (SourceFile& candidate : sources) {
        if (candidate.path == sourceFile) {
            return &candidate;
        }
    }
    return nullptr;
}

SourceId SourceManager::addSource(
    const std::string& sourceFile,
    const std::map<int, std::string>& sourceLines
) {
    if (SourceFile* existing = findByPath(sourceFile)) {
        return existing->id;
    }

    SourceFile source;
    source.id = {nextSourceId++};
    source.path = sourceFile;
    const int lastLine = sourceLines.empty() ? 1 : std::max(1, sourceLines.rbegin()->first);
    source.lines.reserve(static_cast<size_t>(lastLine));
    source.lineStarts.reserve(static_cast<size_t>(lastLine));
    for (int lineNumber = 1; lineNumber <= lastLine; ++lineNumber) {
        source.lineStarts.push_back(source.contents.size());
        const auto line = sourceLines.find(lineNumber);
        source.lines.push_back(line == sourceLines.end() ? "" : line->second);
        source.contents += source.lines.back();
        if (lineNumber != lastLine) {
            source.contents += '\n';
        }
    }

    const SourceId id = source.id;
    sources.push_back(std::move(source));
    return id;
}

SourceSpan SourceManager::spanForColumns(
    SourceId source,
    int lineNumber,
    int startColumn,
    int endColumn
) const {
    return spanForRange(
        source,
        lineNumber,
        startColumn,
        lineNumber,
        endColumn
    );
}

SourceSpan SourceManager::spanForRange(
    SourceId source,
    int startLine,
    int startColumn,
    int endLine,
    int endColumn
) const {
    const SourceFile* file = find(source);
    if (file == nullptr ||
        startLine < 1 ||
        endLine < startLine ||
        static_cast<size_t>(startLine) > file->lines.size() ||
        static_cast<size_t>(endLine) > file->lines.size()) {
        return {};
    }

    const std::string& firstLine = file->lines[static_cast<size_t>(startLine - 1)];
    const std::string& lastLine = file->lines[static_cast<size_t>(endLine - 1)];
    const size_t firstLineStart = file->lineStarts[static_cast<size_t>(startLine - 1)];
    const size_t lastLineStart = file->lineStarts[static_cast<size_t>(endLine - 1)];
    const size_t startByte = std::min(
        firstLine.size(),
        static_cast<size_t>(std::max(1, startColumn) - 1)
    );
    const size_t endByte = startLine == endLine && endColumn < startColumn
        ? startByte
        : std::min(lastLine.size(), static_cast<size_t>(std::max(0, endColumn)));
    const size_t startOffset = firstLineStart + startByte;
    const size_t endOffset = lastLineStart + endByte;
    return {source, startOffset, std::max(startOffset, endOffset)};
}

SourceSpan SourceManager::insertionSpan(SourceId source, int lineNumber, int column) const {
    return spanForColumns(source, lineNumber, column, column - 1);
}

SourceSpan SourceManager::tokenSpanAt(SourceId source, int lineNumber, int column) const {
    const SourceFile* file = find(source);
    if (file == nullptr || lineNumber < 1 || static_cast<size_t>(lineNumber) > file->lines.size()) {
        return {};
    }

    const std::string& line = file->lines[static_cast<size_t>(lineNumber - 1)];
    const size_t target = std::min(
        line.size(),
        static_cast<size_t>(std::max(1, column) - 1)
    );
    if (target >= line.size() || std::isspace(static_cast<unsigned char>(line[target]))) {
        return insertionSpan(source, lineNumber, static_cast<int>(target + 1));
    }

    size_t index = 0;
    while (index < line.size()) {
        if (std::isspace(static_cast<unsigned char>(line[index]))) {
            ++index;
            continue;
        }

        const size_t start = index;
        const unsigned char current = static_cast<unsigned char>(line[index]);
        if (std::isalpha(current) || line[index] == '_') {
            ++index;
            while (index < line.size()) {
                const unsigned char ch = static_cast<unsigned char>(line[index]);
                if (!std::isalnum(ch) && line[index] != '_') {
                    break;
                }
                ++index;
            }
        } else if (std::isdigit(current)) {
            ++index;
            while (index < line.size()) {
                const unsigned char ch = static_cast<unsigned char>(line[index]);
                if (!std::isalnum(ch) && line[index] != '.' && line[index] != '_') {
                    break;
                }
                ++index;
            }
        } else if (line[index] == '"' || line[index] == '\'') {
            const char quote = line[index++];
            bool escaped = false;
            while (index < line.size()) {
                const char ch = line[index++];
                if (escaped) {
                    escaped = false;
                } else if (ch == '\\') {
                    escaped = true;
                } else if (ch == quote) {
                    break;
                }
            }
        } else if (line[index] == '/' && index + 1 < line.size() && line[index + 1] == '/') {
            index = line.size();
        } else {
            static const std::vector<std::string> operators = {
                "<<=", ">>=", "++", "--", "==", "!=", "<=", ">=", "&&", "||",
                "+=", "-=", "*=", "/=", "%=", "<<", ">>", "&=", "|=", "^="
            };
            size_t operatorLength = 0;
            for (const std::string& candidate : operators) {
                if (line.compare(index, candidate.size(), candidate) == 0) {
                    operatorLength = candidate.size();
                    break;
                }
            }
            index += std::max<size_t>(1, operatorLength);
        }

        if (target >= start && target < index) {
            const size_t lineStart = file->lineStarts[static_cast<size_t>(lineNumber - 1)];
            return {source, lineStart + start, lineStart + index};
        }
    }

    return insertionSpan(source, lineNumber, static_cast<int>(target + 1));
}

SourceLocation SourceManager::location(SourceId source, size_t offset) const {
    const SourceFile* file = find(source);
    if (file == nullptr || file->lineStarts.empty()) {
        return {};
    }

    const size_t boundedOffset = std::min(offset, file->contents.size());
    auto line = std::upper_bound(file->lineStarts.begin(), file->lineStarts.end(), boundedOffset);
    size_t lineIndex = line == file->lineStarts.begin()
        ? 0
        : static_cast<size_t>((line - file->lineStarts.begin()) - 1);
    if (lineIndex >= file->lines.size()) {
        lineIndex = file->lines.size() - 1;
    }
    const size_t byteColumn = std::min(
        boundedOffset - file->lineStarts[lineIndex],
        file->lines[lineIndex].size()
    );
    const int displayColumn = displayWidth(file->lines[lineIndex].substr(0, byteColumn)) + 1;
    return {
        static_cast<int>(lineIndex + 1),
        displayColumn,
        static_cast<int>(byteColumn + 1),
        boundedOffset
    };
}

std::string SourceManager::sourceFile(SourceId source) const {
    const SourceFile* file = find(source);
    return file == nullptr ? "<unknown>" : file->path;
}

std::string SourceManager::lineText(SourceId source, int lineNumber) const {
    const SourceFile* file = find(source);
    if (file == nullptr || lineNumber < 1 || static_cast<size_t>(lineNumber) > file->lines.size()) {
        return "";
    }
    return file->lines[static_cast<size_t>(lineNumber - 1)];
}

std::string SourceManager::text(SourceSpan span) const {
    const SourceFile* file = find(span.source);
    if (file == nullptr || !span.valid()) {
        return "";
    }
    const size_t start = std::min(span.startOffset, file->contents.size());
    const size_t end = std::min(std::max(span.startOffset, span.endOffset), file->contents.size());
    return file->contents.substr(start, end - start);
}

void SourceManager::clear() {
    sources.clear();
    nextSourceId = 1;
}

SourceSpan sourceSpanForColumns(
    const std::string& sourceFile,
    const std::map<int, std::string>& sourceLines,
    int lineNumber,
    int startColumn,
    int endColumn
) {
    SourceManager& sources = diagnosticSources();
    const SourceId source = sources.addSource(sourceFile, sourceLines);
    return sources.spanForColumns(source, lineNumber, startColumn, endColumn);
}

SourceSpan sourceSpanForRange(
    const std::string& sourceFile,
    const std::map<int, std::string>& sourceLines,
    int startLine,
    int startColumn,
    int endLine,
    int endColumn
) {
    SourceManager& sources = diagnosticSources();
    const SourceId source = sources.addSource(sourceFile, sourceLines);
    return sources.spanForRange(
        source,
        startLine,
        startColumn,
        endLine,
        endColumn
    );
}

SourceSpan sourceInsertionSpan(
    const std::string& sourceFile,
    const std::map<int, std::string>& sourceLines,
    int lineNumber,
    int column
) {
    SourceManager& sources = diagnosticSources();
    const SourceId source = sources.addSource(sourceFile, sourceLines);
    return sources.insertionSpan(source, lineNumber, column);
}

SourceSpan sourceTokenSpan(
    const std::string& sourceFile,
    const std::map<int, std::string>& sourceLines,
    int lineNumber,
    int column
) {
    SourceManager& sources = diagnosticSources();
    const SourceId source = sources.addSource(sourceFile, sourceLines);
    return sources.tokenSpanAt(source, lineNumber, column);
}

std::string closestDiagnosticCandidate(
    const std::string& input,
    const std::vector<std::string>& candidates
) {
    if (input.empty()) {
        return "";
    }

    const size_t maximumDistance =
        input.size() <= 3 ? 1 :
        input.size() <= 7 ? 2 : 3;
    size_t bestDistance = maximumDistance + 1;
    std::string best;
    bool tied = false;
    for (const std::string& candidate : candidates) {
        if (candidate.empty() || candidate == input) {
            continue;
        }
        const size_t distance = levenshteinDistance(input, candidate);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
            tied = false;
        } else if (distance == bestDistance && candidate != best) {
            tied = true;
        }
    }

    return bestDistance <= maximumDistance && !tied ? best : "";
}

void addAutomaticSyntaxSuggestion(
    Diagnostic& diagnostic,
    const std::string& sourceFile,
    int lineNumber,
    int column,
    const std::map<int, std::string>& sourceLines
) {
    const char closingDelimiter = automaticClosingDelimiter(diagnostic.message);
    if (closingDelimiter == '\0') {
        return;
    }

    const auto sourceLine = sourceLines.find(lineNumber);
    if (sourceLine == sourceLines.end()) {
        return;
    }
    const int insertionColumn = delimiterInsertionColumn(
        sourceLine->second,
        column,
        closingDelimiter
    );
    diagnostic.suggestions.push_back({
        sourceInsertionSpan(
            sourceFile,
            sourceLines,
            lineNumber,
            insertionColumn
        ),
        std::string(1, closingDelimiter),
        std::string("add `") + closingDelimiter + "` to close the delimiter",
        SuggestionApplicability::MachineApplicable
    });
}

void recordDiagnostic(Diagnostic diagnostic) {
    const bool hasPrimary = std::any_of(
        diagnostic.labels.begin(),
        diagnostic.labels.end(),
        [](const DiagnosticLabel& label) {
            return label.primary;
        }
    );
    if (!hasPrimary && !diagnostic.labels.empty()) {
        diagnostic.labels.front().primary = true;
    }
    diagnostics().push_back(std::move(diagnostic));
}

void printSourceError(
    const std::string& sourceFile,
    int lineNumber,
    int column,
    const std::string& message,
    const std::map<int, std::string>& sourceLines
) {
    recordSourceError(sourceFile, lineNumber, column, message, sourceLines);
    printRecordedSourceErrors();
    clearRecordedSourceErrors();
}

void recordSourceError(
    const std::string& sourceFile,
    int lineNumber,
    int column,
    const std::string& message,
    const std::map<int, std::string>& sourceLines
) {
    recordDiagnostic(pointDiagnostic(
        sourceFile,
        lineNumber,
        column,
        message,
        sourceLines
    ));
}

void recordSourceError(
    const std::string& sourceFile,
    int lineNumber,
    int startColumn,
    int endColumn,
    const std::string& message,
    const std::map<int, std::string>& sourceLines
) {
    Diagnostic diagnostic;
    diagnostic.message = message;
    diagnostic.labels.push_back({
        sourceSpanForColumns(
            sourceFile,
            sourceLines,
            lineNumber,
            startColumn,
            endColumn
        ),
        "",
        true
    });
    addAutomaticSyntaxSuggestion(
        diagnostic,
        sourceFile,
        lineNumber,
        startColumn,
        sourceLines
    );
    recordDiagnostic(std::move(diagnostic));
}

// hasRecordedSourceErrors returns whether the supplied input satisfies the relevant condition.
bool hasRecordedSourceErrors() {
    return !diagnostics().empty();
}

void printRecordedSourceErrors() {
    const std::vector<Diagnostic>& recorded = diagnostics();
    for (size_t index = 0; index < recorded.size(); ++index) {
        if (index > 0) {
            std::cerr << '\n';
        }
        printDiagnostic(std::cerr, recorded[index]);
    }
}

void clearRecordedSourceErrors() {
    diagnostics().clear();
    diagnosticSources().clear();
}

void printCompileErrors(
    const std::string& sourceFile,
    const std::string& logFile,
    const std::map<int, std::string>& sourceLines,
    const std::map<int, int>& cppToCpppLine,
    const std::map<int, std::vector<SourceRange>>& sourceRanges
) {
    std::ifstream log(logFile);
    if (!log) {
        std::cerr << "CP++ compile error: generated C++ failed to compile\n";
        return;
    }

    std::string line;
    std::regex errorPattern(R"(:([0-9]+):([0-9]+):\s+error:\s+(.*))");

    while (std::getline(log, line)) {
        std::smatch match;
        if (!std::regex_search(line, match, errorPattern)) {
            continue;
        }

        const int cppLine = std::stoi(match[1].str());
        const int cppColumn = std::stoi(match[2].str());
        const auto mappedLine = cppToCpppLine.find(cppLine);
        if (mappedLine != cppToCpppLine.end()) {
            int sourceColumn = 1;
            SourceSpan mappedSourceSpan;
            const auto ranges = sourceRanges.find(cppLine);
            if (ranges != sourceRanges.end()) {
                for (const SourceRange& range : ranges->second) {
                    if (cppColumn >= range.generatedStartColumn && cppColumn <= range.generatedEndColumn) {
                        sourceColumn = range.sourceColumn + (cppColumn - range.generatedStartColumn);
                        mappedSourceSpan = range.sourceSpan;
                        break;
                    }
                }
            }

            const std::string message = match[3].str();
            if (message.find("missing terminating") != std::string::npos) {
                const auto sourceLine = sourceLines.find(mappedLine->second);
                if (sourceLine != sourceLines.end() && !sourceLine->second.empty()) {
                    sourceColumn = static_cast<int>(sourceLine->second.size());
                }
            }

            if (mappedSourceSpan.valid()) {
                Diagnostic diagnostic;
                diagnostic.message = message;
                diagnostic.labels.push_back({mappedSourceSpan, "", true});
                recordDiagnostic(std::move(diagnostic));
                printRecordedSourceErrors();
                clearRecordedSourceErrors();
            } else {
                printSourceError(sourceFile, mappedLine->second, sourceColumn, message, sourceLines);
            }
            return;
        } else {
            std::cerr << "CP++ compile error: " << match[3].str() << '\n';
            return;
        }
    }

    std::cerr << "CP++ compile error: generated C++ failed to compile\n";
}

bool printRuntimeErrors(
    const std::string& sourceFile,
    const std::string& logFile,
    const std::map<int, std::string>& sourceLines,
    const std::map<int, int>& cppToCpppLine
) {
    std::ifstream log(logFile);
    if (!log) {
        return false;
    }

    std::string line;
    std::string logText;
    std::regex runtimePattern(R"(:([0-9]+):([0-9]+):\s+runtime error:\s+(.*))");
    while (std::getline(log, line)) {
        logText += line;
        logText += '\n';

        std::smatch match;
        if (!std::regex_search(line, match, runtimePattern)) {
            continue;
        }

        const int reportedLine = std::stoi(match[1].str());
        const int reportedColumn = std::stoi(match[2].str());
        const std::string message = match[3].str();
        const std::string readableMessage = readableRuntimeMessage(message);
        if (line.rfind("CP++:", 0) == 0) {
            printRuntimeDiagnostic(sourceFile, reportedLine, reportedColumn, readableMessage, sourceLines);
        } else if (const auto mappedLine = cppToCpppLine.find(reportedLine); mappedLine != cppToCpppLine.end()) {
            printRuntimeDiagnostic(sourceFile, mappedLine->second, reportedColumn, readableMessage, sourceLines);
        } else {
            std::cout << "CP++ " << readableMessage << '\n';
        }
        return true;
    }

    const std::string lowered = lowerText(logText);
    const RuntimeFailureLocation location = likelyRuntimeFailureLocation(sourceLines);
    std::string message = readableRuntimeMessageForLog(lowered, location);
    if (!logText.empty() && message == "runtime error: generated program stopped before finishing") {
        message = "runtime error: " + logText.substr(0, logText.find('\n'));
    }

    printRuntimeDiagnostic(sourceFile, location.lineNumber, location.column, message, sourceLines);
    return true;
}
