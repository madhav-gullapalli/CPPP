#pragma once

#include <map>
#include <string>
#include <vector>

struct SourceRange {
    int sourceLine;
    int sourceColumn;
    int generatedStartColumn;
    int generatedEndColumn;
};

void printSourceError(
    const std::string& sourceFile,
    int lineNumber,
    int column,
    const std::string& message,
    const std::map<int, std::string>& sourceLines
);

void recordSourceError(
    const std::string& sourceFile,
    int lineNumber,
    int column,
    const std::string& message,
    const std::map<int, std::string>& sourceLines
);

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
