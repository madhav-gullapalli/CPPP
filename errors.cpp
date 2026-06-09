#include "errors.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>

namespace {
struct Diagnostic {
    std::string sourceFile;
    int lineNumber;
    int column;
    std::string message;
    std::string sourceLine;
};

std::vector<Diagnostic>& diagnostics() {
    static std::vector<Diagnostic> errors;
    return errors;
}

void printDiagnostic(const Diagnostic& diagnostic) {
    const int displayColumn = std::max(1, diagnostic.column);
    std::cerr << diagnostic.sourceFile << ':' << diagnostic.lineNumber << ':' << displayColumn
              << ": error: " << diagnostic.message << '\n';

    if (!diagnostic.sourceLine.empty()) {
        std::cerr << diagnostic.sourceLine << '\n';
    }
}
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
    const auto sourceLine = sourceLines.find(lineNumber);
    diagnostics().push_back({
        sourceFile,
        lineNumber,
        column,
        message,
        sourceLine == sourceLines.end() ? "" : sourceLine->second
    });
}

bool hasRecordedSourceErrors() {
    return !diagnostics().empty();
}

void printRecordedSourceErrors() {
    for (const Diagnostic& diagnostic : diagnostics()) {
        printDiagnostic(diagnostic);
    }
}

void clearRecordedSourceErrors() {
    diagnostics().clear();
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
            const auto ranges = sourceRanges.find(cppLine);
            if (ranges != sourceRanges.end()) {
                for (const SourceRange& range : ranges->second) {
                    if (cppColumn >= range.generatedStartColumn && cppColumn <= range.generatedEndColumn) {
                        sourceColumn = range.sourceColumn + (cppColumn - range.generatedStartColumn);
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

            printSourceError(sourceFile, mappedLine->second, sourceColumn, message, sourceLines);
            return;
        } else {
            std::cerr << "CP++ compile error: " << match[3].str() << '\n';
            return;
        }
    }

    std::cerr << "CP++ compile error: generated C++ failed to compile\n";
}
