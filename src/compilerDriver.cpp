#include "compilerDriver.h"

#include "compileContext.h"
#include "errors.h"
#include "expressions.h"
#include "sourceSplitter.h"
#include "statementCompiler.h"
#include "typesCppp.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

bool startsWithTrimmed(const std::string& text, const std::string& prefix) {
    const std::string trimmed = trim(text);
    return trimmed.rfind(prefix, 0) == 0;
}

std::string loopBreakFlagNameFromDeclaration(const std::string& text) {
    const std::string trimmed = trim(text);
    const std::string prefix = "bool __cppp_loop_completed_";
    const std::string suffix = " = true;";
    if (trimmed.rfind(prefix, 0) != 0 || trimmed.size() <= prefix.size() + suffix.size()) {
        return "";
    }
    if (trimmed.substr(trimmed.size() - suffix.size()) != suffix) {
        return "";
    }
    return trimmed.substr(5, trimmed.size() - 5 - suffix.size());
}

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

std::string quotePath(const std::string& path) {
    return "\"" + path + "\"";
}

std::string commandPathFor(const std::string& path) {
#ifdef _WIN32
    if (path.size() >= 2 && path[1] == ':') {
        return path;
    }
    if (!path.empty() && (path[0] == '\\' || path[0] == '/')) {
        return path;
    }
    return ".\\" + path;
#else
    return path;
#endif
}

std::string executablePathFor(const std::string& inputFile, const std::string& extension) {
    const std::filesystem::path inputPath(inputFile);
    const std::filesystem::path directory = inputPath.parent_path();
    const std::string baseName = inputFile.substr(
        inputFile.find_last_of("\\/") == std::string::npos ? 0 : inputFile.find_last_of("\\/") + 1,
        inputFile.size() - (inputFile.find_last_of("\\/") == std::string::npos ? 0 : inputFile.find_last_of("\\/") + 1) - extension.size()
    );
#ifdef _WIN32
    const std::string executableName = baseName + ".exe";
#else
    const std::string executableName = baseName;
#endif

    return (directory / "build" / executableName).string();
}

void pruneSubmitLoopHelpers(CompileContext& context) {
    std::vector<std::string> usedLoopFlags;
    for (const GeneratedLine& line : context.generatedBodyLines) {
        const std::string trimmed = trim(line.text);
        if (!startsWithTrimmed(trimmed, "if (__cppp_loop_completed_")) {
            continue;
        }

        const size_t flagStart = trimmed.find("__cppp_loop_completed_");
        const size_t flagEnd = trimmed.find(')', flagStart);
        if (flagStart == std::string::npos || flagEnd == std::string::npos || flagEnd <= flagStart) {
            continue;
        }

        usedLoopFlags.push_back(trimmed.substr(flagStart, flagEnd - flagStart));
    }

    std::vector<GeneratedLine> cleanedLines;
    for (GeneratedLine line : context.generatedBodyLines) {
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
                    if (commentStart != std::string::npos) {
                        line.text += " " + trim(originalText.substr(commentStart));
                    }
                }
            }
        }

        cleanedLines.push_back(std::move(line));
    }
    context.generatedBodyLines = std::move(cleanedLines);
}
}

int runCompilerDriver(int argc, char* argv[]) {
    if ((argc != 3 && argc != 4) || std::string(argv[1]) != "--cppp") {
        std::cerr << "Usage: " << argv[0] << " --cppp FILE_NAME.cppp [--compile|--run|--submit]\n";
        return 1;
    }
    clearRecordedSourceErrors();
    clearRequiredRuntimeHelpers();

    const bool hasAction = argc == 4;
    const std::string action = hasAction ? std::string(argv[3]) : "";
    const bool shouldCompile = action == "--compile" || action == "--run" || action == "--submit";
    const bool shouldRun = action == "--run";
    const bool shouldSubmit = action == "--submit";
    if (hasAction && !shouldCompile) {
        std::cerr << "Error: unknown option " << argv[3] << '\n';
        return 1;
    }
    setExpressionRuntimeChecksEnabled(shouldRun);

    const std::string inputFile = argv[2];
    const std::string cpppExtension = ".cppp";
    if (inputFile.size() < cpppExtension.size() ||
        inputFile.substr(inputFile.size() - cpppExtension.size()) != cpppExtension) {
        std::cerr << "Error: input file must have a .cppp extension\n";
        return 1;
    }

    CompileOptions options;
    options.inputFile = inputFile;
    options.outputFile = inputFile.substr(0, inputFile.size() - cpppExtension.size()) + ".cpp";
    options.executableFile = executablePathFor(inputFile, cpppExtension);
    options.shouldCompile = shouldCompile;
    options.shouldRun = shouldRun;
    options.shouldSubmit = shouldSubmit;

    std::ifstream input(options.inputFile, std::ios::binary);
    if (!input) {
        std::cerr << "Error: could not open " << options.inputFile << '\n';
        return 1;
    }

    CompileContext context(options);
    compileSourceFragments(context, splitSourceFragments(input, context.sourceLines));

    if (context.blockDepth > 0) {
        recordSourceError(options.inputFile, context.sourceLines.empty() ? 1 : context.sourceLines.rbegin()->first, 1, "unclosed block", context.sourceLines);
    }

    if (hasRecordedSourceErrors()) {
        printRecordedSourceErrors();
        clearRecordedSourceErrors();
        clearRequiredRuntimeHelpers();
        return 1;
    }

    if (options.shouldSubmit) {
        pruneSubmitLoopHelpers(context);
    }

    std::ofstream output(options.outputFile, std::ios::binary);
    if (!output) {
        std::cerr << "Error: could not write to " << options.outputFile << '\n';
        clearRequiredRuntimeHelpers();
        return 1;
    }

    const auto emitLine = [&](const std::string& text, int sourceLine = 0) {
        output << text << '\n';
        ++context.generatedLine;
        if (sourceLine != 0) {
            context.cppToCpppLine[context.generatedLine] = sourceLine;
        }
    };

    emitLine("#include <bits/stdc++.h>");
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
    emitLine("int main() {");
    emitLine("    ios::sync_with_stdio(false);");
    emitLine("    cin.tie(nullptr);");
    emitLine("");
    if (options.shouldRun) {
        emitLine("    try {");
    }

    for (const GeneratedLine& line : context.generatedBodyLines) {
        if (!line.sourceRanges.empty()) {
            context.sourceRanges[context.generatedLine + 1] = line.sourceRanges;
        }
        emitLine(line.text, line.sourceLine);
    }

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
    output.close();

    if (options.shouldCompile) {
        std::filesystem::create_directories(std::filesystem::path(options.executableFile).parent_path());

        const std::string compileLogFile = options.outputFile + ".compile.log";
        const std::string command =
            "g++ " + quotePath(options.outputFile) + " -o " + quotePath(options.executableFile) +
            " > " + quotePath(compileLogFile) + " 2>&1";
        const int result = std::system(command.c_str());
        if (result != 0) {
            printCompileErrors(options.inputFile, compileLogFile, context.sourceLines, context.cppToCpppLine, context.sourceRanges);
            clearRequiredRuntimeHelpers();
            return 1;
        }

        std::cout << (options.shouldSubmit ? "Built submit target " : "Built ") << options.executableFile << '\n' << std::flush;

        if (options.shouldRun) {
            const std::string runCommand = commandPathFor(options.executableFile);
            const int runResult = std::system(runCommand.c_str());
            clearRequiredRuntimeHelpers();
            if (runResult != 0) {
                return 1;
            }
        }
    }

    clearRequiredRuntimeHelpers();
    return 0;
}
