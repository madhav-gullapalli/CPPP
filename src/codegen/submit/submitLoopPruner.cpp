#include "submitLoopPruner.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace {
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::string loopBreakFlagNameFromDeclaration(const std::string& text) {
    const std::string trimmed = trim(text);
    const std::string prefix = "bool __cppp_loop_completed_";
    const std::string suffix = " = true;";
    if (trimmed.rfind(prefix, 0) != 0 || trimmed.size() <= prefix.size() + suffix.size()) return "";
    if (trimmed.substr(trimmed.size() - suffix.size()) != suffix) return "";
    return trimmed.substr(5, trimmed.size() - 5 - suffix.size());
}
}

void pruneUnusedSubmitLoopHelpers(CompileContext& context) {
    std::vector<std::string> usedLoopFlags;
    for (const GeneratedLine& line : context.generatedMainLines) {
        const std::string text = trim(line.text);
        if (text.rfind("if (__cppp_loop_completed_", 0) != 0) continue;
        const size_t flagStart = text.find("__cppp_loop_completed_");
        const size_t flagEnd = text.find(')', flagStart);
        if (flagStart != std::string::npos && flagEnd != std::string::npos && flagEnd > flagStart) {
            usedLoopFlags.push_back(text.substr(flagStart, flagEnd - flagStart));
        }
    }

    std::vector<GeneratedLine> cleanedLines;
    for (GeneratedLine line : context.generatedMainLines) {
        const std::string flagName = loopBreakFlagNameFromDeclaration(line.text);
        if (!flagName.empty() &&
            std::find(usedLoopFlags.begin(), usedLoopFlags.end(), flagName) == usedLoopFlags.end()) {
            continue;
        }

        if (line.text.find("__cppp_loop_completed_") != std::string::npos &&
            line.text.find(" = false; break;") != std::string::npos) {
            const std::string originalText = line.text;
            const size_t indentEnd = line.text.find_first_not_of(' ');
            const std::string indent = indentEnd == std::string::npos ? "" : line.text.substr(0, indentEnd);
            const size_t assignEnd = line.text.find("break;");
            const size_t commentStart = originalText.find("//", assignEnd == std::string::npos ? 0 : assignEnd);
            const size_t flagStart = line.text.find("__cppp_loop_completed_");
            const size_t flagEnd = line.text.find(" = false; break;");
            if (flagStart != std::string::npos && flagEnd != std::string::npos) {
                const std::string flag = line.text.substr(flagStart, flagEnd - flagStart);
                if (std::find(usedLoopFlags.begin(), usedLoopFlags.end(), flag) == usedLoopFlags.end()) {
                    line.text = indent + "break;";
                    if (commentStart != std::string::npos) line.text += " " + trim(originalText.substr(commentStart));
                }
            }
        }
        cleanedLines.push_back(std::move(line));
    }
    context.generatedMainLines = std::move(cleanedLines);
}
