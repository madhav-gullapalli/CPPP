#include <bits/stdc++.h>

#include "errors.h"
#include "printCppp.h"
#include "typesCppp.h"

static std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

static std::string quotePath(const std::string& path) {
    return "\"" + path + "\"";
}

static std::string executablePathFor(const std::string& inputFile, const std::string& extension) {
    const size_t slash = inputFile.find_last_of("\\/");
    const std::string directory = slash == std::string::npos ? "" : inputFile.substr(0, slash + 1);
    const std::string baseName = inputFile.substr(
        slash == std::string::npos ? 0 : slash + 1,
        inputFile.size() - (slash == std::string::npos ? 0 : slash + 1) - extension.size()
    );

    return directory + "build\\" + baseName + ".exe";
}

int main(int argc, char* argv[]) {
    if ((argc != 3 && argc != 4) || std::string(argv[1]) != "--cppp") {
        std::cerr << "Usage: " << argv[0] << " --cppp FILE_NAME.cppp [--compile]\n";
        return 1;
    }
    clearRecordedSourceErrors();

    const bool shouldCompile = argc == 4;
    if (shouldCompile && std::string(argv[3]) != "--compile") {
        std::cerr << "Error: unknown option " << argv[3] << '\n';
        return 1;
    }

    const std::string inputFile = argv[2];
    const std::string cpppExtension = ".cppp";

    if (inputFile.size() < cpppExtension.size() ||
        inputFile.substr(inputFile.size() - cpppExtension.size()) != cpppExtension) {
        std::cerr << "Error: input file must have a .cppp extension\n";
        return 1;
    }

    const std::string outputFile =
        inputFile.substr(0, inputFile.size() - cpppExtension.size()) + ".cpp";
    const std::string executableFile = executablePathFor(inputFile, cpppExtension);

    std::ifstream input(inputFile, std::ios::binary);
    if (!input) {
        std::cerr << "Error: could not open " << inputFile << '\n';
        return 1;
    }

    std::ofstream output(outputFile, std::ios::binary);
    if (!output) {
        std::cerr << "Error: could not write to " << outputFile << '\n';
        return 1;
    }

    std::map<int, int> cppToCpppLine;
    std::map<int, std::string> sourceLines;
    std::map<int, std::vector<SourceRange>> sourceRanges;
    std::set<std::string> declaredVariables;
    int generatedLine = 0;
    const auto emitLine = [&](const std::string& text, int sourceLine = 0) {
        output << text << '\n';
        ++generatedLine;
        if (sourceLine != 0) {
            cppToCpppLine[generatedLine] = sourceLine;
        }
    };

    emitLine("#include <bits/stdc++.h>");
    emitLine("using namespace std;");
    emitLine("");
    for (const std::string& preambleLine : typeSupportPreamble()) {
        emitLine(preambleLine);
    }
    emitLine("int main() {");
    emitLine("    ios::sync_with_stdio(false);");
    emitLine("    cin.tie(nullptr);");
    emitLine("");

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        sourceLines[lineNumber] = line;
        const std::string statement = trim(line);

        if (statement.empty()) {
            continue;
        }

        if (statement.back() != ';') {
            const int column = static_cast<int>(line.find_last_not_of(" \t\r\n")) + 1;
            recordSourceError(inputFile, lineNumber, column, "missing semicolon", sourceLines);
            continue;
        }

        const std::string statementBody = trim(statement.substr(0, statement.size() - 1));
        const TypeEmitResult typeResult = emitTypeDeclaration(inputFile, lineNumber, line, statementBody, sourceLines, declaredVariables);
        if (typeResult.matched) {
            if (!typeResult.ok) {
                continue;
            }

            sourceRanges[generatedLine + 1] = typeResult.sourceRanges;
            emitLine(typeResult.generatedStatement, lineNumber);
            continue;
        }

        const PrintEmitResult printResult = emitPrintStatement(inputFile, lineNumber, line, statementBody, sourceLines, declaredVariables);
        if (!printResult.ok) {
            continue;
        }

        sourceRanges[generatedLine + 1] = printResult.sourceRanges;
        emitLine(printResult.generatedStatement, lineNumber);
    }

    if (hasRecordedSourceErrors()) {
        printRecordedSourceErrors();
        clearRecordedSourceErrors();
        return 1;
    }

    emitLine("    return 0;");
    emitLine("}");
    output.close();

    if (shouldCompile) {
        std::filesystem::create_directories(std::filesystem::path(executableFile).parent_path());

        const std::string compileLogFile = outputFile + ".compile.log";
        const std::string command =
            "g++ " + quotePath(outputFile) + " -o " + quotePath(executableFile) +
            " > " + quotePath(compileLogFile) + " 2>&1";
        const int result = std::system(command.c_str());
        if (result != 0) {
            printCompileErrors(inputFile, compileLogFile, sourceLines, cppToCpppLine, sourceRanges);
            return 1;
        }

        std::cout << "Built " << executableFile << '\n';
    }

    return 0;
}
