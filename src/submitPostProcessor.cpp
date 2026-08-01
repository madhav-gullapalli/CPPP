/*
 * submitPostProcessor.cpp
 *
 * Compacts a complete generated C++ translation unit after normal emission.
 */

#include "submitPostProcessor.h"

#include <cctype>
#include <set>

namespace {
bool isCppWordCharacter(char value) {
    return std::isalnum(static_cast<unsigned char>(value)) || value == '_';
}

bool whitespaceIsRequired(const std::string& output, char next) {
    if (output.empty() || output.back() == '\n') return false;
    const char previous = output.back();
    if (isCppWordCharacter(previous) && isCppWordCharacter(next)) return true;
    if ((std::isdigit(static_cast<unsigned char>(previous)) && next == '.') ||
        (previous == '.' && std::isdigit(static_cast<unsigned char>(next)))) return true;
    if ((previous == '\'' || previous == '"') && isCppWordCharacter(next)) return true;
    if (isCppWordCharacter(previous) && (next == '\'' || next == '"')) return true;
    const std::string merged = std::string(1, previous) + next;
    static const std::set<std::string> punctuators = {
        "++", "--", "->", ".*", "<<", ">>", "<=", ">=", "==", "!=",
        "&&", "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=",
        "::", "##", "/*", "//"
    };
    if (punctuators.count(merged) != 0) return true;
    if (output.size() >= 2) {
        const std::string mergedThree = output.substr(output.size() - 2) + next;
        if (mergedThree == "->*" || mergedThree == "<<=" || mergedThree == ">>=" ||
            mergedThree == "..." || mergedThree == "<=>") return true;
    }
    return false;
}
}

std::string compactSubmitCpp(const std::string& readableCpp) {
    std::string compact;
    compact.reserve(readableCpp.size());
    bool pendingWhitespace = false;
    bool lineStart = true;
    for (size_t index = 0; index < readableCpp.size();) {
        const char current = readableCpp[index];
        if (lineStart) {
            size_t directive = index;
            while (directive < readableCpp.size() &&
                (readableCpp[directive] == ' ' || readableCpp[directive] == '\t' || readableCpp[directive] == '\r')) ++directive;
            if (directive < readableCpp.size() && readableCpp[directive] == '#') {
                if (!compact.empty() && compact.back() != '\n') compact.push_back('\n');
                const size_t end = readableCpp.find('\n', directive);
                compact.append(readableCpp, directive, end == std::string::npos ? readableCpp.size() - directive : end - directive);
                compact.push_back('\n');
                if (end == std::string::npos) break;
                index = end + 1;
                pendingWhitespace = false;
                lineStart = true;
                continue;
            }
        }
        if (current == '\n') {
            pendingWhitespace = true;
            lineStart = true;
            ++index;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(current))) {
            pendingWhitespace = true;
            ++index;
            continue;
        }
        lineStart = false;
        if (current == '/' && index + 1 < readableCpp.size() && readableCpp[index + 1] == '/') {
            const size_t end = readableCpp.find('\n', index + 2);
            index = end == std::string::npos ? readableCpp.size() : end;
            pendingWhitespace = true;
            continue;
        }
        if (current == '/' && index + 1 < readableCpp.size() && readableCpp[index + 1] == '*') {
            const size_t end = readableCpp.find("*/", index + 2);
            index = end == std::string::npos ? readableCpp.size() : end + 2;
            pendingWhitespace = true;
            continue;
        }
        if (pendingWhitespace && whitespaceIsRequired(compact, current)) compact.push_back(' ');
        pendingWhitespace = false;
        if (current == '\'' || current == '"') {
            const char quote = current;
            compact.push_back(current);
            ++index;
            while (index < readableCpp.size()) {
                compact.push_back(readableCpp[index]);
                if (readableCpp[index] == '\\' && index + 1 < readableCpp.size()) {
                    compact.push_back(readableCpp[index + 1]);
                    index += 2;
                    continue;
                }
                if (readableCpp[index++] == quote) break;
            }
            continue;
        }
        compact.push_back(current);
        ++index;
    }
    while (!compact.empty() && std::isspace(static_cast<unsigned char>(compact.back()))) compact.pop_back();
    compact.push_back('\n');
    return compact;
}
