/*
 * errors.h
 *
 * Defines diagnostic and source-range data structures used by the compiler and runtime layers.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

// SourceId identifies one immutable source file registered with the diagnostic
// source manager.
struct SourceId {
    size_t value = 0;

    bool operator==(const SourceId& other) const {
        return value == other.value;
    }

    bool operator!=(const SourceId& other) const {
        return !(*this == other);
    }
};

// SourceSpan is a canonical, end-exclusive byte range in one source file.
struct SourceSpan {
    SourceId source;
    size_t startOffset = 0;
    size_t endOffset = 0;

    bool valid() const {
        return source.value != 0 && startOffset <= endOffset;
    }
};

struct SourceLocation {
    int line = 1;
    int column = 1;
    int byteColumn = 1;
    size_t offset = 0;
};

// SourceManager owns immutable source text and is the only component that
// converts canonical byte offsets to user-facing lines and columns.
class SourceManager {
public:
    SourceId addSource(
        const std::string& sourceFile,
        const std::map<int, std::string>& sourceLines
    );
    SourceSpan spanForColumns(
        SourceId source,
        int lineNumber,
        int startColumn,
        int endColumn
    ) const;
    SourceSpan spanForRange(
        SourceId source,
        int startLine,
        int startColumn,
        int endLine,
        int endColumn
    ) const;
    SourceSpan insertionSpan(SourceId source, int lineNumber, int column) const;
    SourceSpan tokenSpanAt(SourceId source, int lineNumber, int column) const;
    SourceLocation location(SourceId source, size_t offset) const;
    std::string sourceFile(SourceId source) const;
    std::string lineText(SourceId source, int lineNumber) const;
    std::string text(SourceSpan span) const;
    void clear();

private:
    struct SourceFile {
        SourceId id;
        std::string path;
        std::string contents;
        std::vector<size_t> lineStarts;
        std::vector<std::string> lines;
    };

    const SourceFile* find(SourceId source) const;
    SourceFile* findByPath(const std::string& sourceFile);

    std::vector<SourceFile> sources;
    size_t nextSourceId = 1;
};

enum class DiagnosticSeverity {
    Error,
    Warning
};

struct DiagnosticLabel {
    SourceSpan span;
    std::string message;
    bool primary = false;
};

enum class SuggestionApplicability {
    MachineApplicable,
    MaybeIncorrect,
    HasPlaceholders,
    Unspecified
};

struct DiagnosticSuggestion {
    SourceSpan span;
    std::string replacement;
    std::string message;
    SuggestionApplicability applicability = SuggestionApplicability::Unspecified;
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string code;
    std::string message;
    std::vector<DiagnosticLabel> labels;
    std::vector<std::string> notes;
    std::vector<std::string> helps;
    std::vector<DiagnosticSuggestion> suggestions;
};

// SourceRange implements the SourceRange behavior for the errors.h module.
struct SourceRange {
    int sourceLine = 0;
    int sourceColumn = 1;
    int generatedStartColumn = 0;
    int generatedEndColumn = 0;
    SourceSpan sourceSpan;

    SourceRange() = default;
    SourceRange(
        int sourceLine,
        int sourceColumn,
        int generatedStartColumn,
        int generatedEndColumn,
        SourceSpan sourceSpan = {}
    ) :
        sourceLine(sourceLine),
        sourceColumn(sourceColumn),
        generatedStartColumn(generatedStartColumn),
        generatedEndColumn(generatedEndColumn),
        sourceSpan(sourceSpan) {}
};

void printSourceError(
    const std::string& sourceFile,
    int lineNumber,
    int column,
    const std::string& message,
    const std::map<int, std::string>& sourceLines
);

// sourceSpanForColumns returns an exact end-exclusive source span. Columns are
// one-based and endColumn is inclusive to keep call sites readable.
SourceSpan sourceSpanForColumns(
    const std::string& sourceFile,
    const std::map<int, std::string>& sourceLines,
    int lineNumber,
    int startColumn,
    int endColumn
);

SourceSpan sourceSpanForRange(
    const std::string& sourceFile,
    const std::map<int, std::string>& sourceLines,
    int startLine,
    int startColumn,
    int endLine,
    int endColumn
);

SourceSpan sourceInsertionSpan(
    const std::string& sourceFile,
    const std::map<int, std::string>& sourceLines,
    int lineNumber,
    int column
);

SourceSpan sourceTokenSpan(
    const std::string& sourceFile,
    const std::map<int, std::string>& sourceLines,
    int lineNumber,
    int column
);

// Returns the unique closest context-valid spelling when it is sufficiently
// similar to input. An empty result means no suggestion is reliable.
std::string closestDiagnosticCandidate(
    const std::string& input,
    const std::vector<std::string>& candidates
);

// Adds safe insertion suggestions for locally recoverable delimiters. Block
// braces are intentionally excluded because their intended location is often
// ambiguous.
void addAutomaticSyntaxSuggestion(
    Diagnostic& diagnostic,
    const std::string& sourceFile,
    int lineNumber,
    int column,
    const std::map<int, std::string>& sourceLines
);

void recordDiagnostic(Diagnostic diagnostic);

void recordSourceError(
    const std::string& sourceFile,
    int lineNumber,
    int column,
    const std::string& message,
    const std::map<int, std::string>& sourceLines
);

void recordSourceError(
    const std::string& sourceFile,
    int lineNumber,
    int startColumn,
    int endColumn,
    const std::string& message,
    const std::map<int, std::string>& sourceLines
);

// hasRecordedSourceErrors returns whether the supplied input satisfies the relevant condition.
bool hasRecordedSourceErrors();
void printRecordedSourceErrors();
void clearRecordedSourceErrors();

void printCompileErrors(
    const std::string& sourceFile,
    const std::string& logFile,
    const std::map<int, std::string>& sourceLines,
    const std::map<int, int>& cppToCpppLine,
    const std::map<int, std::vector<SourceRange>>& sourceRanges
);

bool printRuntimeErrors(
    const std::string& sourceFile,
    const std::string& logFile,
    const std::map<int, std::string>& sourceLines,
    const std::map<int, int>& cppToCpppLine
);
