/*
 * programEmitter.cpp
 *
 * Emits the translated C++ program, including generated headers, runtime support, and main function scaffolding.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "programEmitter.h"

#include "typesCppp.h"

#include <string>
#include <vector>

namespace {
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
    for (const GeneratedLine& line : lines) {
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
"#include <memory>\n"
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
    const std::vector<std::string> preambleLines = options.shouldSubmit
        ? typeSupportPreambleForSubmit(requiredRuntimeHelpers())
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
