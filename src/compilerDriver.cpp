/*
 * compilerDriver.cpp
 *
 * Implements the compiler driver entry points, file handling, and command-line orchestration for transpilation.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "compilerDriver.h"

#include "astParser.h"
#include "astPrinter.h"
#include "compileContext.h"
#include "errors.h"
#include "expressions.h"
#include "run/runCodegen.h"
#include "run/runProgramEmitter.h"
#include "semanticAnalyzer.h"
#include "semanticPrinter.h"
#include "sourceSplitter.h"
#include "submit/pruning/submitPruner.h"
#include "submit/submitCodegen.h"
#include "submit/submitProgramEmitter.h"
#include "typesCppp.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {
std::string readSourceText(
    std::istream& input,
    std::map<int, std::string>& sourceLines
) {
    std::string source;
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        sourceLines[lineNumber] = line;
        if (lineNumber > 1) {
            source += '\n';
        }
        source += line;
    }
    if (sourceLines.empty()) {
        sourceLines[1] = "";
    }
    return source;
}

std::string jsonString(const std::string& text) {
    std::ostringstream escaped;
    escaped << '"';
    for (unsigned char ch : text) {
        switch (ch) {
            case '"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (ch < 0x20) {
                    escaped << "\\u"
                            << std::hex << std::setw(4) << std::setfill('0')
                            << static_cast<int>(ch)
                            << std::dec << std::setfill(' ');
                } else {
                    escaped << static_cast<char>(ch);
                }
                break;
        }
    }
    escaped << '"';
    return escaped.str();
}

void printTokenStream(const TokenStream& stream) {
    for (const Token& token : stream.tokens) {
        std::cout
            << "{\"kind\":" << jsonString(tokenKindName(token.kind))
            << ",\"text\":" << jsonString(token.text)
            << ",\"line\":" << token.span.startLine
            << ",\"column\":" << token.span.startColumn
            << ",\"endLine\":" << token.span.endLine
            << ",\"endColumn\":" << token.span.endColumn
            << ",\"startOffset\":" << token.span.startOffset
            << ",\"endOffset\":" << token.span.endOffset
            << "}\n";
    }
}

// quotePath quotes a path string for safe output.
std::string quotePath(const std::string& path) {
    return "\"" + path + "\"";
}

// commandPathFor returns the command path used to run a file.
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

// executablePathFor builds the executable output path for an input source file.
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

}

// Keep the driver flow linear on purpose:
// raw file -> canonical TokenStream -> ProgramAst -> semantic lowering -> C++.
int runCompilerDriver(int argc, char* argv[]) {
    if ((argc < 3 || argc > 5) || std::string(argv[1]) != "--cppp") {
        std::cerr << "Usage: " << argv[0] << " --cppp FILE_NAME.cppp [--tokens|--ast|--semantic|--submit-ast|--compile|--run|--submit [--readable]]\n";
        return 1;
    }
    clearRecordedSourceErrors();
    clearRequiredRuntimeHelpers();

    const bool hasAction = argc >= 4;
    const std::string action = hasAction ? std::string(argv[3]) : "";
    const bool shouldPrintTokens = action == "--tokens";
    const bool shouldPrintAst = action == "--ast";
    const bool shouldPrintSemantic = action == "--semantic";
    const bool shouldPrintSubmitAst = action == "--submit-ast";
    const bool shouldCompile = action == "--compile" || action == "--run" || action == "--submit";
    const bool shouldRun = action == "--run";
    const bool shouldSubmit = action == "--submit";
    if (hasAction && !shouldCompile && !shouldPrintTokens && !shouldPrintAst && !shouldPrintSemantic && !shouldPrintSubmitAst) {
        std::cerr << "Error: unknown option " << argv[3] << '\n';
        return 1;
    }
    const bool readableSubmit = argc == 5 && std::string(argv[4]) == "--readable";
    if (argc == 5 && !readableSubmit) {
        std::cerr << "Error: unknown option " << argv[4] << '\n';
        return 1;
    }
    if (argc == 5 && !shouldSubmit) {
        std::cerr << "Error: --readable can only be used with --submit\n";
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
    options.readableSubmit = readableSubmit;

    std::ifstream input(options.inputFile, std::ios::binary);
    if (!input) {
        std::cerr << "Error: could not open " << options.inputFile << '\n';
        return 1;
    }

    // This is the shared state object for every later compiler stage.
    CompileContext context(options);
    const std::string source = readSourceText(input, context.sourceLines);
    const int lastLine = context.sourceLines.rbegin()->first;
    const int lastColumn = static_cast<int>(context.sourceLines.rbegin()->second.size());
    const SourceSpan sourceSpan = sourceSpanForRange(
        options.inputFile,
        context.sourceLines,
        1,
        1,
        lastLine,
        lastColumn
    );
    const TokenStream tokenStream = tokenizeSource(source, sourceSpan);
    if (shouldPrintTokens) {
        printTokenStream(tokenStream);
        return 0;
    }
    ProgramAst program = parseProgramAst(tokenStream);
    std::string astInvariantError;
    if (!validateProgramAst(program, astInvariantError)) {
        std::cerr << "Internal AST error: " << astInvariantError << '\n';
        return 1;
    }
    if (shouldPrintAst) {
        printProgramAst(std::cout, program);
        return 0;
    }
    AnalyzedProgramAst analyzed = analyzeProgramAst(context, program);
    if (hasRecordedSourceErrors()) {
        printRecordedSourceErrors();
        clearRecordedSourceErrors();
        clearRequiredRuntimeHelpers();
        setDeclaredFunctionsForExpressions(nullptr);
        return 1;
    }
    std::string semanticInvariantError;
    if (!validateAnalyzedProgramAst(analyzed, semanticInvariantError)) {
        std::cerr << "Internal semantic AST error: " << semanticInvariantError << '\n';
        return 1;
    }
    if (shouldPrintSemantic) {
        printAnalyzedProgramAst(std::cout, analyzed);
        return 0;
    }
    if (shouldPrintSubmitAst) {
        PrunedAnalyzedProgramAst pruned = pruneAnalyzedProgramForSubmit(analyzed);
        std::string pruningInvariantError;
        if (!validatePrunedAnalyzedProgramAst(pruned, analyzed, pruningInvariantError)) {
            std::cerr << "Internal submit-pruning AST error: " << pruningInvariantError << '\n';
            return 1;
        }
        printProgramAst(std::cout, *pruned.ownedProgram);
        return 0;
    }
    setDeclaredFunctionsForExpressions(&context.declaredFunctions);
    if (options.shouldSubmit) {
        PrunedAnalyzedProgramAst pruned = pruneAnalyzedProgramForSubmit(analyzed);
        std::string pruningInvariantError;
        if (!validatePrunedAnalyzedProgramAst(pruned, analyzed, pruningInvariantError)) {
            std::cerr << "Internal submit-pruning AST error: " << pruningInvariantError << '\n';
            return 1;
        }
        generateSubmitProgram(context, pruned);
    } else {
        generateRunProgram(context, analyzed);
    }

    if (context.blockDepth > 0) {
        recordSourceError(options.inputFile, context.sourceLines.empty() ? 1 : context.sourceLines.rbegin()->first, 1, "unclosed block", context.sourceLines);
    }

    if (hasRecordedSourceErrors()) {
        printRecordedSourceErrors();
        clearRecordedSourceErrors();
        clearRequiredRuntimeHelpers();
        setDeclaredFunctionsForExpressions(nullptr);
        return 1;
    }

    std::ofstream output(options.outputFile, std::ios::binary);
    if (!output) {
        std::cerr << "Error: could not write to " << options.outputFile << '\n';
        clearRequiredRuntimeHelpers();
        setDeclaredFunctionsForExpressions(nullptr);
        return 1;
    }

    if (options.shouldSubmit) emitSubmitProgram(output, context);
    else emitRunProgram(output, context);
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
            setDeclaredFunctionsForExpressions(nullptr);
            return 1;
        }

        std::cout << (options.shouldSubmit ? "Built submit target " : "Built ") << options.executableFile << '\n' << std::flush;

        if (options.shouldRun) {
            const std::string runLogFile = options.outputFile + ".run.log";
            const std::string runCommand =
                quotePath(commandPathFor(options.executableFile)) +
                " > " + quotePath(runLogFile) + " 2>&1";
            const int runResult = std::system(runCommand.c_str());
            clearRequiredRuntimeHelpers();
            if (runResult != 0) {
                printRuntimeErrors(
                    options.inputFile,
                    runLogFile,
                    context.sourceLines,
                    context.cppToCpppLine
                );
                return 1;
            }

            std::ifstream runOutput(runLogFile, std::ios::binary);
            if (runOutput) {
                std::cout << runOutput.rdbuf();
            }
        }
    }

    clearRequiredRuntimeHelpers();
    setDeclaredFunctionsForExpressions(nullptr);
    return 0;
}
