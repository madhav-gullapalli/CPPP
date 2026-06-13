#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "assignmentCppp.h"
#include "controlFlow.h"
#include "errors.h"
#include "expressions.h"
#include "listsCppp.h"
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

static std::string indentForDepth(int depth) {
    return std::string(static_cast<size_t>((depth + 1) * 4), ' ');
}

static std::string indentGeneratedStatement(const std::string& generatedStatement, int depth) {
    return indentForDepth(depth) + trim(generatedStatement);
}

static std::string stripGeneratedStatement(const std::string& generatedStatement) {
    std::string text = trim(generatedStatement);
    if (!text.empty() && text.back() == ';') {
        text.pop_back();
    }

    return text;
}

struct SourceFragment {
    int lineNumber;
    std::string text;
};

struct GeneratedLine {
    std::string text;
    int sourceLine = 0;
    std::vector<SourceRange> sourceRanges;
};

static bool isRepCountType(Type type) {
    return type == PrimitiveType::Bool ||
        type == PrimitiveType::Char ||
        type == PrimitiveType::Int ||
        type == PrimitiveType::Float;
}

static size_t findLineCommentStart(const std::string& text) {
    bool inString = false;
    bool inChar = false;
    bool escaped = false;

    for (size_t i = 0; i + 1 < text.size(); ++i) {
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

        if (!inString && !inChar && ch == '/' && text[i + 1] == '/') {
            return i;
        }
    }

    return std::string::npos;
}

static std::vector<std::string> splitSemicolonStatements(const std::string& line) {
    const size_t commentStart = findLineCommentStart(line);
    const std::string codeText = commentStart == std::string::npos ? line : line.substr(0, commentStart);
    const std::string commentText = commentStart == std::string::npos ? "" : line.substr(commentStart);
    std::vector<std::string> fragments;
    bool inString = false;
    bool inChar = false;
    bool escaped = false;
    int parenDepth = 0;
    size_t start = 0;

    for (size_t i = 0; i < codeText.size(); ++i) {
        const char ch = codeText[i];
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
        if (!inString && !inChar && ch == '(') {
            ++parenDepth;
            continue;
        }
        if (!inString && !inChar && ch == ')' && parenDepth > 0) {
            --parenDepth;
            continue;
        }
        if (!inString && !inChar && ch == '{' && parenDepth == 0) {
            fragments.push_back(std::string(start, ' ') + codeText.substr(start, i - start + 1));
            start = i + 1;
            continue;
        }
        if (!inString && !inChar && ch == '}' && parenDepth == 0) {
            if (i > start) {
                fragments.push_back(std::string(start, ' ') + codeText.substr(start, i - start));
            }
            fragments.push_back(std::string(i, ' ') + codeText.substr(i, 1));
            start = i + 1;
            continue;
        }
        if (!inString && !inChar && ch == ';' && parenDepth == 0) {
            fragments.push_back(std::string(start, ' ') + codeText.substr(start, i - start + 1));
            start = i + 1;
        }
    }

    std::string remainder = start < codeText.size() ? std::string(start, ' ') + codeText.substr(start) : "";
    if (!commentText.empty()) {
        if (trim(remainder).empty()) {
            remainder = std::string(commentStart, ' ') + commentText;
        } else {
            remainder += commentText;
        }
    }
    if (!trim(remainder).empty() || fragments.empty()) {
        fragments.push_back(remainder);
    }
    return fragments;
}

static std::vector<SourceFragment> attachDetachedOpeningBraces(const std::vector<SourceFragment>& fragments) {
    std::vector<SourceFragment> attached;
    for (const SourceFragment& fragment : fragments) {
        if (trim(fragment.text) == "{" && !attached.empty()) {
            attached.back().text += " {";
            continue;
        }

        attached.push_back(fragment);
    }

    return attached;
}

static std::string quotePath(const std::string& path) {
    return "\"" + path + "\"";
}

static std::string commandPathFor(const std::string& path) {
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

static std::string executablePathFor(const std::string& inputFile, const std::string& extension) {
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

int main(int argc, char* argv[]) {
    if ((argc != 3 && argc != 4) || std::string(argv[1]) != "--cppp") {
        std::cerr << "Usage: " << argv[0] << " --cppp FILE_NAME.cppp [--compile|--run|--submit]\n";
        return 1;
    }
    clearRecordedSourceErrors();

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
    std::map<std::string, Type> declaredVariables;
    int generatedLine = 0;
    int blockDepth = 0;
    bool canAttachElse = false;
    std::vector<std::string> blockKinds;
    int repLoopIndex = 0;
    const auto emitLine = [&](const std::string& text, int sourceLine = 0) {
        output << text << '\n';
        ++generatedLine;
        if (sourceLine != 0) {
            cppToCpppLine[generatedLine] = sourceLine;
        }
    };
    std::vector<GeneratedLine> generatedBodyLines;
    const auto queueGeneratedLine = [&](const std::string& text, int sourceLine = 0, std::vector<SourceRange> ranges = {}) {
        generatedBodyLines.push_back({text, sourceLine, std::move(ranges)});
    };

    std::vector<SourceFragment> sourceFragments;
    std::string rawLine;
    int rawLineNumber = 0;
    while (std::getline(input, rawLine)) {
        ++rawLineNumber;
        sourceLines[rawLineNumber] = rawLine;
        for (const std::string& fragment : splitSemicolonStatements(rawLine)) {
            sourceFragments.push_back({rawLineNumber, fragment});
        }
    }
    sourceFragments = attachDetachedOpeningBraces(sourceFragments);

    int lineNumber = 0;
    for (const SourceFragment& fragment : sourceFragments) {
        lineNumber = fragment.lineNumber;
        const std::string& line = fragment.text;
        const size_t commentStart = findLineCommentStart(line);
        const std::string commentText = commentStart == std::string::npos ? "" : trim(line.substr(commentStart));
        const bool hasComment = !commentText.empty();
        const std::string codeText = commentStart == std::string::npos ? line : line.substr(0, commentStart);
        const std::string statement = trim(codeText);

        if (statement.empty()) {
            if (hasComment) {
                queueGeneratedLine(indentForDepth(blockDepth) + commentText, lineNumber);
            }
            continue;
        }

        const size_t statementStart = codeText.find(statement);
        const int statementStartColumn = static_cast<int>(statementStart == std::string::npos ? 1 : statementStart + 1);

        const auto emitConditionHeader = [&](const std::string& keyword, const ConditionHeader& header, size_t absoluteOffset = 0) {
            const size_t conditionOffset = absoluteOffset + header.conditionOffset;
            if (header.condition.empty()) {
                recordSourceError(inputFile, lineNumber, statementStartColumn + static_cast<int>(conditionOffset), "expected condition", sourceLines);
                ++blockDepth;
                blockKinds.push_back(keyword);
                canAttachElse = false;
                return false;
            }

            const ExpressionEmitResult condition = emitExpression(
                inputFile,
                lineNumber,
                header.condition,
                statementStartColumn + static_cast<int>(conditionOffset),
                sourceLines,
                declaredVariables
            );
            if (!condition.ok) {
                ++blockDepth;
                blockKinds.push_back(keyword);
                canAttachElse = false;
                return false;
            }

            if (!isImplicitlyConvertible(condition.type, PrimitiveType::Bool)) {
                recordSourceError(inputFile, lineNumber, statementStartColumn + static_cast<int>(conditionOffset), keyword + " condition must be bool", sourceLines);
                ++blockDepth;
                blockKinds.push_back(keyword);
                canAttachElse = false;
                return false;
            }

            std::string generatedCondition = condition.generatedExpression;
            if (condition.type != PrimitiveType::Bool) {
                generatedCondition = castExpressionTo(generatedCondition, condition.type, PrimitiveType::Bool);
            }

            queueGeneratedLine(indentForDepth(blockDepth) + keyword + " (" + generatedCondition + ") {" + (hasComment ? " " + commentText : ""), lineNumber);
            ++blockDepth;
            blockKinds.push_back(keyword);
            canAttachElse = false;
            return true;
        };

        const auto emitForPart = [&](const std::string& part, int partColumn, bool allowDeclaration, std::string& generatedPart) {
            generatedPart.clear();
            if (part.empty()) {
                return true;
            }

            if (allowDeclaration) {
                const TypeEmitResult typeResult = emitTypeDeclaration(inputFile, lineNumber, line, part, sourceLines, declaredVariables);
                if (typeResult.matched) {
                    if (!typeResult.ok) {
                        return false;
                    }

                    generatedPart = stripGeneratedStatement(typeResult.generatedStatement);
                    return true;
                }
            }

            const AssignmentEmitResult assignmentResult = emitAssignmentStatement(inputFile, lineNumber, part, sourceLines, declaredVariables);
            if (assignmentResult.matched) {
                if (!assignmentResult.ok) {
                    return false;
                }

                generatedPart = stripGeneratedStatement(assignmentResult.generatedStatement);
                return true;
            }

            const ExpressionEmitResult expression = emitExpression(
                inputFile,
                lineNumber,
                part,
                partColumn,
                sourceLines,
                declaredVariables
            );
            if (!expression.ok) {
                return false;
            }

            generatedPart = expression.generatedExpression;
            return true;
        };

        if (statement[0] == '}') {
            if (blockDepth == 0) {
                recordSourceError(inputFile, lineNumber, statementStartColumn, "unmatched closing brace", sourceLines);
                continue;
            }

            --blockDepth;
            const std::string closedBlock = blockKinds.empty() ? "" : blockKinds.back();
            if (!blockKinds.empty()) {
                blockKinds.pop_back();
            }
            canAttachElse = closedBlock == "if" || closedBlock == "else if";

            const std::string afterBrace = trim(statement.substr(1));
            if (afterBrace.empty()) {
                queueGeneratedLine(indentForDepth(blockDepth) + "}" + (hasComment ? " " + commentText : ""), lineNumber);
                continue;
            }

            if (!canAttachElse) {
                recordSourceError(inputFile, lineNumber, statementStartColumn + 1, "else without matching if", sourceLines);
                if (parseElseHeader(afterBrace)) {
                    ++blockDepth;
                    blockKinds.push_back("else");
                } else {
                    ConditionHeader recoveryHeader;
                    if (parseElseIfHeader(afterBrace, recoveryHeader)) {
                        ++blockDepth;
                        blockKinds.push_back("else if");
                    }
                }
                continue;
            }

            if (parseElseHeader(afterBrace)) {
                queueGeneratedLine(indentForDepth(blockDepth) + "} else {" + (hasComment ? " " + commentText : ""), lineNumber);
                ++blockDepth;
                blockKinds.push_back("else");
                canAttachElse = false;
                continue;
            }

            ConditionHeader header;
            if (parseElseIfHeader(afterBrace, header)) {
                if (header.condition.empty()) {
                    recordSourceError(inputFile, lineNumber, statementStartColumn + 1 + static_cast<int>(header.conditionOffset), "expected condition", sourceLines);
                    ++blockDepth;
                    blockKinds.push_back("else if");
                    canAttachElse = false;
                    continue;
                }

                const ExpressionEmitResult condition = emitExpression(
                    inputFile,
                    lineNumber,
                    header.condition,
                    statementStartColumn + 1 + static_cast<int>(header.conditionOffset),
                    sourceLines,
                    declaredVariables
                );
                if (!condition.ok) {
                    ++blockDepth;
                    blockKinds.push_back("else if");
                    canAttachElse = false;
                    continue;
                }

                if (!isImplicitlyConvertible(condition.type, PrimitiveType::Bool)) {
                    recordSourceError(inputFile, lineNumber, statementStartColumn + 1 + static_cast<int>(header.conditionOffset), "if condition must be bool", sourceLines);
                    ++blockDepth;
                    blockKinds.push_back("else if");
                    canAttachElse = false;
                    continue;
                }

                std::string generatedCondition = condition.generatedExpression;
                if (condition.type != PrimitiveType::Bool) {
                    generatedCondition = castExpressionTo(generatedCondition, condition.type, PrimitiveType::Bool);
                }

                queueGeneratedLine(indentForDepth(blockDepth) + "} else if (" + generatedCondition + ") {" + (hasComment ? " " + commentText : ""), lineNumber);
                ++blockDepth;
                blockKinds.push_back("else if");
                canAttachElse = false;
                continue;
            }

            queueGeneratedLine(indentForDepth(blockDepth) + "}", lineNumber);
            recordSourceError(inputFile, lineNumber, statementStartColumn + 1, "expected else, else if, or end of statement after '}'", sourceLines);
            continue;
        }

        if (parseElseHeader(statement)) {
            if (!canAttachElse) {
                recordSourceError(inputFile, lineNumber, statementStartColumn, "else without matching if", sourceLines);
                ++blockDepth;
                blockKinds.push_back("else");
                continue;
            }

            queueGeneratedLine(indentForDepth(blockDepth) + "else {" + (hasComment ? " " + commentText : ""), lineNumber);
            ++blockDepth;
            blockKinds.push_back("else");
            canAttachElse = false;
            continue;
        }

        ConditionHeader header;
        if (parseElseIfHeader(statement, header)) {
            if (!canAttachElse) {
                recordSourceError(inputFile, lineNumber, statementStartColumn, "else without matching if", sourceLines);
                ++blockDepth;
                blockKinds.push_back("else if");
                continue;
            }

            emitConditionHeader("else if", header);
            continue;
        }

        ForHeader forHeader;
        if (parseForHeader(statement, forHeader)) {
            std::string generatedInitializer;
            std::string generatedIteration;
            if (!emitForPart(
                    forHeader.initializer,
                    statementStartColumn + static_cast<int>(forHeader.initializerOffset),
                    true,
                    generatedInitializer)) {
                ++blockDepth;
                blockKinds.push_back("for");
                canAttachElse = false;
                continue;
            }

            std::string generatedCondition = "true";
            if (!forHeader.condition.empty()) {
                const ExpressionEmitResult condition = emitExpression(
                    inputFile,
                    lineNumber,
                    forHeader.condition,
                    statementStartColumn + static_cast<int>(forHeader.conditionOffset),
                    sourceLines,
                    declaredVariables
                );
                if (!condition.ok) {
                    ++blockDepth;
                    blockKinds.push_back("for");
                    canAttachElse = false;
                    continue;
                }

                if (!isImplicitlyConvertible(condition.type, PrimitiveType::Bool)) {
                    recordSourceError(inputFile, lineNumber, statementStartColumn + static_cast<int>(forHeader.conditionOffset), "for condition must be bool", sourceLines);
                    ++blockDepth;
                    blockKinds.push_back("for");
                    canAttachElse = false;
                    continue;
                }

                generatedCondition = condition.generatedExpression;
                if (condition.type != PrimitiveType::Bool) {
                    generatedCondition = castExpressionTo(generatedCondition, condition.type, PrimitiveType::Bool);
                }
            }

            if (!emitForPart(
                    forHeader.iteration,
                    statementStartColumn + static_cast<int>(forHeader.iterationOffset),
                    false,
                    generatedIteration)) {
                ++blockDepth;
                blockKinds.push_back("for");
                canAttachElse = false;
                continue;
            }

            queueGeneratedLine(indentForDepth(blockDepth) + "for (" + generatedInitializer + "; " + generatedCondition + "; " + generatedIteration + ") {" + (hasComment ? " " + commentText : ""), lineNumber);
            ++blockDepth;
            blockKinds.push_back("for");
            canAttachElse = false;
            continue;
        }

        if (parseConditionHeader(statement, "rep", header)) {
            if (header.condition.empty()) {
                recordSourceError(inputFile, lineNumber, statementStartColumn + static_cast<int>(header.conditionOffset), "expected rep count", sourceLines);
                ++blockDepth;
                blockKinds.push_back("rep");
                canAttachElse = false;
                continue;
            }

            const ExpressionEmitResult count = emitExpression(
                inputFile,
                lineNumber,
                header.condition,
                statementStartColumn + static_cast<int>(header.conditionOffset),
                sourceLines,
                declaredVariables
            );
            if (!count.ok) {
                ++blockDepth;
                blockKinds.push_back("rep");
                canAttachElse = false;
                continue;
            }

            if (!isRepCountType(count.type)) {
                recordSourceError(inputFile, lineNumber, statementStartColumn + static_cast<int>(header.conditionOffset), "rep count must be numeric", sourceLines);
                ++blockDepth;
                blockKinds.push_back("rep");
                canAttachElse = false;
                continue;
            }

            if (shouldSubmit) {
                const std::string indexName = "_" + std::to_string(repLoopIndex);
                ++repLoopIndex;
                queueGeneratedLine(
                    indentForDepth(blockDepth) +
                    "for (int " + indexName + " = 0; " + indexName + " < " + castExpressionTo(count.generatedExpression, count.type, PrimitiveType::Int) +
                    "; ++" + indexName + ") {" +
                    (hasComment ? " " + commentText : ""),
                    lineNumber
                );
            } else {
                const std::string indexName = "__cppp_rep_" + std::to_string(repLoopIndex);
                const std::string limitName = "__cppp_rep_limit_" + std::to_string(repLoopIndex);
                ++repLoopIndex;
                queueGeneratedLine(
                    indentForDepth(blockDepth) +
                    "for (long long " + indexName + " = 0, " + limitName + " = " + castExpressionTo(count.generatedExpression, count.type, PrimitiveType::Int) +
                    "; " + indexName + " < " + limitName + "; ++" + indexName + ") {" +
                    (hasComment ? " " + commentText : ""),
                    lineNumber
                );
            }
            ++blockDepth;
            blockKinds.push_back("rep");
            canAttachElse = false;
            continue;
        }

        if (parseConditionHeader(statement, "if", header)) {
            emitConditionHeader("if", header);
            continue;
        }

        if (parseConditionHeader(statement, "while", header)) {
            emitConditionHeader("while", header);
            continue;
        }

        canAttachElse = false;

        const bool hasSemicolon = statement.back() == ';';
        if (!hasSemicolon) {
            const int column = static_cast<int>(codeText.find_last_not_of(" \t\r\n")) + 1;
            recordSourceError(inputFile, lineNumber, column, "missing semicolon", sourceLines);
            continue;
        }

        const std::string statementBody = trim(statement.substr(0, statement.size() - 1));
        const TypeEmitResult typeResult = emitTypeDeclaration(inputFile, lineNumber, line, statementBody, sourceLines, declaredVariables);
        if (typeResult.matched) {
            if (!typeResult.ok) {
                continue;
            }

            queueGeneratedLine(
                indentGeneratedStatement(typeResult.generatedStatement, blockDepth) + (hasComment ? " " + commentText : ""),
                lineNumber,
                typeResult.sourceRanges
            );
            continue;
        }

        const AssignmentEmitResult assignmentResult = emitAssignmentStatement(inputFile, lineNumber, statementBody, sourceLines, declaredVariables);
        if (assignmentResult.matched) {
            if (!assignmentResult.ok) {
                continue;
            }

            queueGeneratedLine(
                indentGeneratedStatement(assignmentResult.generatedStatement, blockDepth) + (hasComment ? " " + commentText : ""),
                lineNumber,
                assignmentResult.sourceRanges
            );
            continue;
        }

        const ListEmitResult listResult = emitListStatement(
            inputFile,
            lineNumber,
            statementBody,
            sourceLines,
            declaredVariables,
            !shouldSubmit
        );
        if (listResult.matched) {
            if (!listResult.ok) {
                continue;
            }

            queueGeneratedLine(
                indentGeneratedStatement(listResult.generatedStatement, blockDepth) + (hasComment ? " " + commentText : ""),
                lineNumber,
                listResult.sourceRanges
            );
            continue;
        }

        if (statementBody.find("++") != std::string::npos || statementBody.find("--") != std::string::npos) {
            const ExpressionEmitResult expression = emitExpression(
                inputFile,
                lineNumber,
                statementBody,
                statementStartColumn,
                sourceLines,
                declaredVariables
            );
            if (!expression.ok) {
                continue;
            }

            queueGeneratedLine(indentForDepth(blockDepth) + expression.generatedExpression + ";" + (hasComment ? " " + commentText : ""), lineNumber);
            continue;
        }

        if (statementBody.rfind("describe", 0) == 0) {
            const PrintEmitResult describeResult = emitDescribeStatement(inputFile, lineNumber, line, statementBody, sourceLines, declaredVariables);
            if (!describeResult.ok) {
                continue;
            }

            queueGeneratedLine(
                indentGeneratedStatement(describeResult.generatedStatement, blockDepth) + (hasComment ? " " + commentText : ""),
                lineNumber,
                describeResult.sourceRanges
            );
            continue;
        }

        const PrintEmitResult printResult = emitPrintStatement(inputFile, lineNumber, line, statementBody, sourceLines, declaredVariables);
        if (!printResult.ok) {
            continue;
        }

        queueGeneratedLine(
            indentGeneratedStatement(printResult.generatedStatement, blockDepth) + (hasComment ? " " + commentText : ""),
            lineNumber,
            printResult.sourceRanges
        );
    }

    if (blockDepth > 0) {
        recordSourceError(inputFile, lineNumber, 1, "unclosed block", sourceLines);
    }

    if (hasRecordedSourceErrors()) {
        printRecordedSourceErrors();
        clearRecordedSourceErrors();
        return 1;
    }

    std::string generatedProgramText;
    for (const GeneratedLine& line : generatedBodyLines) {
        generatedProgramText += line.text;
        generatedProgramText.push_back('\n');
    }

    emitLine("#include <bits/stdc++.h>");
    emitLine("using namespace std;");
    emitLine("");
    const std::vector<std::string> preambleLines = shouldSubmit ?
        typeSupportPreambleForSubmit(generatedProgramText) :
        typeSupportPreamble();
    for (const std::string& preambleLine : preambleLines) {
        emitLine(preambleLine);
    }
    emitLine("int main() {");
    emitLine("    ios::sync_with_stdio(false);");
    emitLine("    cin.tie(nullptr);");
    emitLine("");
    if (shouldRun) {
        emitLine("    try {");
    }

    for (const GeneratedLine& line : generatedBodyLines) {
        if (!line.sourceRanges.empty()) {
            sourceRanges[generatedLine + 1] = line.sourceRanges;
        }
        emitLine(line.text, line.sourceLine);
    }

    emitLine("    return 0;");
    if (shouldRun) {
        emitLine("    } catch (const runtime_error& __cppp_error) {");
        emitLine("        string __cppp_message = __cppp_error.what();");
        emitLine("        size_t __cppp_first = __cppp_message.find(':');");
        emitLine("        size_t __cppp_second = __cppp_message.find(':', __cppp_first + 1);");
        emitLine("        if (__cppp_first != string::npos && __cppp_second != string::npos) {");
        emitLine("            cout << \"" + inputFile + ":\" << __cppp_message.substr(0, __cppp_first) << \":\" << __cppp_message.substr(__cppp_first + 1, __cppp_second - __cppp_first - 1) << \": error: runtime error: \" << __cppp_message.substr(__cppp_second + 1) << '\\n';");
        emitLine("        } else {");
        emitLine("            cout << \"CP++ runtime error: \" << __cppp_message << '\\n';");
        emitLine("        }");
        emitLine("        return 1;");
        emitLine("    }");
    }
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

        std::cout << (shouldSubmit ? "Built submit target " : "Built ") << executableFile << '\n' << std::flush;

        if (shouldRun) {
            const std::string runCommand = commandPathFor(executableFile);
            const int runResult = std::system(runCommand.c_str());
            if (runResult != 0) {
                return 1;
            }
        }
    }

    return 0;
}
