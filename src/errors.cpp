#include "errors.h"

#include <algorithm>
#include <cctype>
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

struct RuntimeFailureLocation {
    int lineNumber;
    int column;
    char operation;
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

void printRuntimeDiagnostic(
    const std::string& sourceFile,
    int lineNumber,
    int column,
    const std::string& message,
    const std::map<int, std::string>& sourceLines
) {
    const int displayColumn = std::max(1, column);
    std::cout << sourceFile << ':' << lineNumber << ':' << displayColumn
              << ": error: " << message << '\n';

    const auto sourceLine = sourceLines.find(lineNumber);
    if (sourceLine != sourceLines.end() && !sourceLine->second.empty()) {
        std::cout << sourceLine->second << '\n';
    }
}

std::string lowerText(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

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
