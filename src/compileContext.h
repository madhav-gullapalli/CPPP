#pragma once

#include "errors.h"
#include "expressions.h"
#include "functions.h"

#include <map>
#include <string>
#include <vector>

struct CompileOptions {
    std::string inputFile;
    std::string outputFile;
    std::string executableFile;
    bool shouldCompile = false;
    bool shouldRun = false;
    bool shouldSubmit = false;
};

struct SourceFragment {
    int lineNumber = 0;
    int startColumn = 1;
    std::string text;
};

struct GeneratedLine {
    std::string text;
    int sourceLine = 0;
    std::vector<SourceRange> sourceRanges;
};

struct PendingLoopElse {
    bool active = false;
    std::string breakFlagName;
};

enum class OutputTarget {
    Main,
    Function
};

struct CompileContext {
    explicit CompileContext(const CompileOptions& compileOptions) : options(compileOptions) {}

    const CompileOptions& options;
    std::map<int, int> cppToCpppLine;
    std::map<int, std::string> sourceLines;
    std::map<int, std::vector<SourceRange>> sourceRanges;
    std::map<std::string, Type> declaredVariables;
    std::map<std::string, FunctionSignature> declaredFunctions;
    std::vector<GeneratedLine> generatedTopLevelLines;
    std::vector<GeneratedLine> generatedFunctionLines;
    std::vector<GeneratedLine> generatedMainLines;
    int generatedLine = 0;
    int blockDepth = 0;
    int suppressedBlockDepth = 0;
    bool canAttachElse = false;
    PendingLoopElse pendingLoopElse;
    OutputTarget outputTarget = OutputTarget::Main;
    bool inFunction = false;
    FunctionSignature currentFunction;
    std::map<std::string, Type> savedDeclaredVariables;
    std::vector<std::string> blockKinds;
    std::vector<std::string> blockBreakFlags;
    std::vector<std::vector<std::string>> blockDeclaredNames;
    int repLoopIndex = 0;
    int loopControlIndex = 0;

    void queueTopLevelLine(const std::string& text, int sourceLine = 0, std::vector<SourceRange> ranges = {}) {
        generatedTopLevelLines.push_back({text, sourceLine, std::move(ranges)});
    }

    void queueFunctionLine(const std::string& text, int sourceLine = 0, std::vector<SourceRange> ranges = {}) {
        generatedFunctionLines.push_back({text, sourceLine, std::move(ranges)});
    }

    void queueGeneratedLine(const std::string& text, int sourceLine = 0, std::vector<SourceRange> ranges = {}) {
        if (outputTarget == OutputTarget::Function) {
            generatedFunctionLines.push_back({text, sourceLine, std::move(ranges)});
        } else {
            generatedMainLines.push_back({text, sourceLine, std::move(ranges)});
        }
    }

    void pushBlock(const std::string& kind, const std::string& breakFlag = std::string(), std::vector<std::string> declaredNames = {}) {
        blockKinds.push_back(kind);
        blockBreakFlags.push_back(breakFlag);
        blockDeclaredNames.push_back(std::move(declaredNames));
    }

    void eraseDeclaredNames(const std::vector<std::string>& names) {
        for (const std::string& name : names) {
            declaredVariables.erase(name);
        }
    }

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
