/*
 * errors.h
 *
 * Defines diagnostic and source-range data structures used by the compiler and runtime layers.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#pragma once

#include <map>
#include <string>
#include <vector>

// SourceRange implements the SourceRange behavior for the errors.h module.
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
