/*
 * programEmitter.cpp
 *
 * Emits the translated C++ program, including generated headers, runtime support, and main function scaffolding.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "programEmitter.h"

#include "typesCppp.h"

#include <cctype>
#include <string>
#include <set>
#include <vector>

namespace {
bool containsIdentifier(const std::string& text, const std::string& name) {
    size_t position = text.find(name);
    while (position != std::string::npos) {
        const bool leftBoundary = position == 0 || !(std::isalnum(static_cast<unsigned char>(text[position - 1])) || text[position - 1] == '_');
        const size_t end = position + name.size();
        const bool rightBoundary = end == text.size() || !(std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_');
        if (leftBoundary && rightBoundary) {
            return true;
        }
        position = text.find(name, position + 1);
    }
    return false;
}

std::set<std::string> reachableSubmitOwners(const CompileContext& context) {
    std::map<std::string, std::vector<std::string>> linesByOwner;
    for (const GeneratedLine& line : context.generatedTopLevelLines) {
        if (!line.submitOwnerKey.empty()) linesByOwner[line.submitOwnerKey].push_back(line.text);
    }
    for (const GeneratedLine& line : context.generatedFunctionLines) {
        if (!line.submitOwnerKey.empty()) linesByOwner[line.submitOwnerKey].push_back(line.text);
    }

    std::set<std::string> reachable;
    std::set<std::string> calledMethodNames;
    std::vector<std::string> pendingText;
    for (const GeneratedLine& line : context.generatedMainLines) pendingText.push_back(line.text);
    for (const GeneratedLine& line : context.generatedTopLevelLines) {
        if (line.submitOwnerKey.empty()) pendingText.push_back(line.text);
    }
    for (const GeneratedLine& line : context.generatedFunctionLines) {
        if (line.submitOwnerKey.empty()) pendingText.push_back(line.text);
    }

    const auto addOwner = [&](const std::string& owner, std::set<std::string>& found, std::vector<std::string>& work) {
        if (!found.insert(owner).second) return;
        const auto lines = linesByOwner.find(owner);
        if (lines != linesByOwner.end()) work.insert(work.end(), lines->second.begin(), lines->second.end());
    };

    for (size_t index = 0; index < pendingText.size(); ++index) {
        // addOwner() may append to pendingText and reallocate its storage.
        // Keep a value copy so the text being scanned cannot dangle.
        const std::string text = pendingText[index];
        for (const auto& function : context.declaredFunctions) {
            if (text.find(function.first + "(") != std::string::npos) {
                addOwner("function:" + function.first, reachable, pendingText);
            }
        }
        for (const auto& structure : context.declaredStructs) {
            if (containsIdentifier(text, structure.first)) {
                addOwner("struct:" + structure.first, reachable, pendingText);
            }
        }
        for (const auto& structure : context.declaredStructMethods) {
            for (const auto& method : structure.second) {
                if (text.find("->" + method.first + "(") != std::string::npos ||
                    text.find("." + method.first + "(") != std::string::npos) {
                    calledMethodNames.insert(method.first);
                }
            }
        }

        for (const auto& structure : context.declaredStructMethods) {
            if (reachable.count("struct:" + structure.first) == 0) continue;
            for (const std::string& methodName : calledMethodNames) {
                if (structure.second.count(methodName) != 0) {
                    addOwner("method:" + structure.first + "." + methodName, reachable, pendingText);
                }
            }
        }
    }
    return reachable;
}

std::set<std::string> requiredSubmitContainerTypes(
    const CompileContext& context,
    const std::set<std::string>& reachableOwners
) {
    std::set<std::string> types;
    const auto inspect = [&](const std::vector<GeneratedLine>& lines) {
        for (const GeneratedLine& line : lines) {
            if (!line.submitOwnerKey.empty() && reachableOwners.count(line.submitOwnerKey) == 0) continue;
            if (line.text.find("CPPPPair<") != std::string::npos) types.insert("CPPPPair");
            if (line.text.find("CPPPList<") != std::string::npos) types.insert("CPPPList");
            if (line.text.find("CPPPStack<") != std::string::npos) types.insert("CPPPStack");
            if (line.text.find("CPPPQueue<") != std::string::npos) types.insert("CPPPQueue");
            if (line.text.find("CPPPDeque<") != std::string::npos) types.insert("CPPPDeque");
            if (line.text.find("CPPPSet<") != std::string::npos) types.insert("CPPPSet");
            if (line.text.find("CPPPMap<") != std::string::npos) types.insert("CPPPMap");
        }
    };
    inspect(context.generatedTopLevelLines);
    inspect(context.generatedFunctionLines);
    inspect(context.generatedMainLines);
    return types;
}

std::set<std::string> requiredSubmitContainerMembers(
    const CompileContext& context,
    const std::set<std::string>& reachableOwners
) {
    std::set<std::string> members;
    const std::vector<std::string> names = {"first", "second", "begin", "end", "cbegin", "cend", "rbegin", "rend", "empty", "size", "reserve", "resize", "clear", "push_back", "pop_back", "emplace_back", "insert", "erase", "at", "front", "back", "find", "lower_bound", "upper_bound"};
    const auto inspect = [&](const std::vector<GeneratedLine>& lines) {
        for (const GeneratedLine& line : lines) {
            if (!line.submitOwnerKey.empty() && reachableOwners.count(line.submitOwnerKey) == 0) continue;
            for (const std::string& name : names) {
                if (line.text.find("." + name + "(") != std::string::npos) members.insert(name);
            }
            if (line.text.find('[') != std::string::npos) members.insert("index");
            if (line.text.find("CPPPList<") != std::string::npos && line.text.find('{') != std::string::npos) members.insert("ctor_init");
            if (line.text.find("CPPPList<") != std::string::npos && line.text.find("begin()") != std::string::npos && line.text.find("end()") != std::string::npos) members.insert("ctor_iterator");
            if (line.text.find(" : ") != std::string::npos) { members.insert("begin"); members.insert("end"); }
        }
    };
    inspect(context.generatedTopLevelLines);
    inspect(context.generatedFunctionLines);
    inspect(context.generatedMainLines);
    return members;
}

// cppStringLiteral implements the cppStringLiteral behavior for the programEmitter.cpp module.
std::string cppStringLiteral(const std::string& text) {
    std::string escaped = "\"";
    for (char ch : text) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }
    escaped += "\"";
    return escaped;
}

void emitGeneratedLines(
    std::ostream& output,
    CompileContext& context,
    const std::vector<GeneratedLine>& lines
) {
    const std::set<std::string> submitOwners = context.options.shouldSubmit
        ? reachableSubmitOwners(context)
        : std::set<std::string>{};
    for (const GeneratedLine& line : lines) {
        if (context.options.shouldSubmit &&
            !line.submitOwnerKey.empty() &&
            submitOwners.count(line.submitOwnerKey) == 0) {
            continue;
        }
        output << line.text << '\n';
        ++context.generatedLine;
        if (line.sourceLine != 0) {
            context.cppToCpppLine[context.generatedLine] = line.sourceLine;
        }
        if (!line.sourceRanges.empty()) {
            context.sourceRanges[context.generatedLine] = line.sourceRanges;
        }
    }
}
}

void emitTranslatedProgram(std::ostream& output, CompileContext& context) {
    const CompileOptions& options = context.options;
    const auto emitLine = [&](const std::string& text, int sourceLine = 0) {
        output << text << '\n';
        ++context.generatedLine;
        if (sourceLine != 0) {
            context.cppToCpppLine[context.generatedLine] = sourceLine;
        }
    };

    emitLine("#include <algorithm>\n"
"#include <array>\n"
"#include <bitset>\n"
"#include <cassert>\n"
"#include <climits>\n"
"#include <cmath>\n"
"#include <cstdint>\n"
"#include <cstdlib>\n"
"#include <cstring>\n"
"#include <deque>\n"
"#include <functional>\n"
"#include <iomanip>\n"
"#include <iostream>\n"
"#include <limits>\n"
"#include <map>\n"
"#include <numeric>\n"
"#include <queue>\n"
"#include <set>\n"
"#include <sstream>\n"
"#include <stdexcept>\n"
"#include <string>\n"
"#include <tuple>\n"
"#include <type_traits>\n"
"#include <unordered_map>\n"
"#include <unordered_set>\n"
"#include <utility>\n"
"#include <vector>\n"
"\n");
    emitLine("using namespace std;");
    emitLine("");
    const std::set<std::string> submitOwners = options.shouldSubmit
        ? reachableSubmitOwners(context)
        : std::set<std::string>{};
    const std::vector<std::string> preambleLines = options.shouldSubmit
        ? typeSupportPreambleForSubmit(
            requiredRuntimeHelpersForOwners(submitOwners),
            requiredSubmitContainerTypes(context, submitOwners),
            requiredSubmitContainerMembers(context, submitOwners)
        )
        : typeSupportPreamble();
    for (const std::string& preambleLine : preambleLines) {
        emitLine(preambleLine);
    }
    if (options.shouldRun) {
        emitLine("static const vector<string> __cppp_source_lines = {");
        emitLine("    \"\",");
        for (const auto& sourceLine : context.sourceLines) {
            emitLine("    " + cppStringLiteral(sourceLine.second) + ",");
        }
        emitLine("};");
        emitLine("");
    }

    emitGeneratedLines(output, context, context.generatedTopLevelLines);
    if (!context.generatedTopLevelLines.empty()) {
        emitLine("");
    }

    emitGeneratedLines(output, context, context.generatedFunctionLines);
    if (!context.generatedFunctionLines.empty()) {
        emitLine("");
    }

    emitLine("int main() {");
    emitLine("    ios::sync_with_stdio(false);");
    emitLine("    cin.tie(nullptr);");
    emitLine("");
    if (options.shouldRun) {
        emitLine("    try {");
    }

    emitGeneratedLines(output, context, context.generatedMainLines);

    emitLine("    return 0;");
    if (options.shouldRun) {
        emitLine("    } catch (const runtime_error& __cppp_error) {");
        emitLine("        string __cppp_message = __cppp_error.what();");
        emitLine("        size_t __cppp_first = __cppp_message.find(':');");
        emitLine("        size_t __cppp_second = __cppp_message.find(':', __cppp_first + 1);");
        emitLine("        if (__cppp_first != string::npos && __cppp_second != string::npos) {");
        emitLine("            int __cppp_line = stoi(__cppp_message.substr(0, __cppp_first));");
        emitLine("            int __cppp_column = stoi(__cppp_message.substr(__cppp_first + 1, __cppp_second - __cppp_first - 1));");
        emitLine("            cout << \"" + options.inputFile + ":\" << __cppp_line << ':' << __cppp_column << \": error: runtime error: \" << __cppp_message.substr(__cppp_second + 1) << '\\n';");
        emitLine("            if (__cppp_line >= 0 && __cppp_line < static_cast<int>(__cppp_source_lines.size())) {");
        emitLine("                cout << __cppp_source_lines[static_cast<size_t>(__cppp_line)] << '\\n';");
        emitLine("                cout << string(static_cast<size_t>(max(0, __cppp_column - 1)), ' ') << '^' << '\\n';");
        emitLine("            }");
        emitLine("        } else {");
        emitLine("            cout << \"CP++ runtime error: \" << __cppp_message << '\\n';");
        emitLine("        }");
        emitLine("        return 1;");
        emitLine("    }");
    }
    emitLine("}");
}
