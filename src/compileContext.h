/*
 * compileContext.h
 *
 * Shared state for the whole transpiler pipeline.
 *
 * The current flow is:
 * source text
 *   -> TokenStream
 *   -> ProgramAst
 *   -> compileProgramAst(...)
 *   -> generatedTopLevelLines / generatedFunctionLines / generatedMainLines
 *   -> emitTranslatedProgram(...)
 *
 * If two compiler stages need to communicate, that data usually lives here.
 */

#pragma once

#include "errors.h"
#include "expressions.h"
#include "functions.h"

#include <map>
#include <string>
#include <vector>

// Per-invocation options chosen by the driver before source processing starts.
struct CompileOptions {
    std::string inputFile;
    std::string outputFile;
    std::string executableFile;
    bool shouldCompile = false;
    bool shouldRun = false;
    bool shouldSubmit = false;
    bool readableSubmit = false;
};

// One emitted C++ line plus the CP++ source metadata needed for diagnostics.
struct GeneratedLine {
    std::string text;
    int sourceLine = 0;
    std::vector<SourceRange> sourceRanges;
    std::string submitOwnerKey;
};

enum class OutputTarget {
    Main,
    Function,
    TopLevel
};

// Shared mutable compiler state: source maps, symbols, output buffers, and
// block/function lowering state.
struct CompileContext {
    explicit CompileContext(const CompileOptions& compileOptions) : options(compileOptions) {}

    const CompileOptions& options;
    std::map<int, int> cppToCpppLine;
    std::map<int, std::string> sourceLines;
    std::map<int, std::vector<SourceRange>> sourceRanges;
    std::map<std::string, Type> declaredVariables;
    std::map<std::string, FunctionSignature> declaredFunctions;
    std::map<std::string, std::map<std::string, Type>> declaredStructs;
    std::set<std::string> declaredClassNames;
    std::map<std::string, std::vector<std::string>> declaredStructFieldOrders;
    std::map<std::string, std::map<std::string, FunctionSignature>> declaredStructMethods;
    std::vector<GeneratedLine> generatedTopLevelLines;
    std::vector<GeneratedLine> generatedFunctionLines;
    std::vector<GeneratedLine> generatedMainLines;
    int generatedLine = 0;
    int blockDepth = 0;
    OutputTarget outputTarget = OutputTarget::Main;
    bool inFunction = false;
    bool currentStructIsClass = false;
    std::string currentStructName;
    std::string currentStructMethodName;
    std::string currentTopLevelFunctionName;
    std::vector<std::string> currentStructFields;
    FunctionSignature currentFunction;
    std::map<std::string, Type> savedDeclaredVariables;
    std::vector<std::string> blockKinds;
    std::vector<std::string> blockBreakFlags;
    std::vector<std::vector<std::string>> blockDeclaredNames;
    int repLoopIndex = 0;
    int loopControlIndex = 0;

    // Output that must live above generated functions and above main().
    void queueTopLevelLine(const std::string& text, int sourceLine = 0, std::vector<SourceRange> ranges = {}) {
        const std::string ownerKey = currentStructMethodName.empty()
            ? (currentStructName.empty() ? "" : "struct:" + currentStructName)
            : "method:" + currentStructName + "." + currentStructMethodName;
        generatedTopLevelLines.push_back({text, sourceLine, std::move(ranges), ownerKey});
    }

    // Output for emitted user-defined function bodies.
    void queueFunctionLine(const std::string& text, int sourceLine = 0, std::vector<SourceRange> ranges = {}) {
        const std::string ownerKey = currentTopLevelFunctionName.empty() ? "" : "function:" + currentTopLevelFunctionName;
        generatedFunctionLines.push_back({text, sourceLine, std::move(ranges), ownerKey});
    }

    // Default queue used during statement lowering. The current OutputTarget
    // decides whether the line lands in a function body or generated main().
    void queueGeneratedLine(const std::string& text, int sourceLine = 0, std::vector<SourceRange> ranges = {}) {
        if (outputTarget == OutputTarget::Function) {
            const std::string ownerKey = currentTopLevelFunctionName.empty() ? "" : "function:" + currentTopLevelFunctionName;
            generatedFunctionLines.push_back({text, sourceLine, std::move(ranges), ownerKey});
        } else if (outputTarget == OutputTarget::TopLevel) {
            const std::string ownerKey = currentStructMethodName.empty()
                ? (currentStructName.empty() ? "" : "struct:" + currentStructName)
                : "method:" + currentStructName + "." + currentStructMethodName;
            generatedTopLevelLines.push_back({text, sourceLine, std::move(ranges), ownerKey});
        } else {
            generatedMainLines.push_back({text, sourceLine, std::move(ranges), ""});
        }
    }

    // Mirrors lexical block nesting so close-brace handling can restore scope
    // and loop metadata correctly.
    void pushBlock(const std::string& kind, const std::string& breakFlag = std::string(), std::vector<std::string> declaredNames = {}) {
        blockKinds.push_back(kind);
        blockBreakFlags.push_back(breakFlag);
        blockDeclaredNames.push_back(std::move(declaredNames));
    }

    // Removes names whose scope ended when a block closed.
    void eraseDeclaredNames(const std::vector<std::string>& names) {
        for (const std::string& name : names) {
            declaredVariables.erase(name);
        }
    }

    // Finds the innermost surrounding loop helper flag for break/continue and
    // loop-nobreak lowering.
    std::string nearestLoopBreakFlag() const {
        for (int i = static_cast<int>(blockKinds.size()) - 1; i >= 0; --i) {
            const std::string& kind = blockKinds[static_cast<size_t>(i)];
            if (kind == "for" || kind == "while" || kind == "rep") {
                return blockBreakFlags[static_cast<size_t>(i)];
            }
        }
        return "";
    }
};
