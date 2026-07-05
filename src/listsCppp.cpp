/*
 * listsCppp.cpp
 *
 * Implements list parsing, literal handling, and runtime helpers for CP++ list operations.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "listsCppp.h"

#include "tokenizer.h"

namespace {
// trim removes surrounding whitespace from a string.
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// ListArgument handles list-specific behavior for the compiler or runtime.
struct ListArgument {
    std::string text;
    int column;
};

// splitListArguments splits the input into smaller logical pieces.
std::vector<ListArgument> splitListArguments(const std::string& text, int startColumn) {
    std::vector<ListArgument> arguments;
    const std::vector<Token> tokens = tokenize(text);
    int parenDepth = 0;
    int bracketDepth = 0;
    size_t argumentStartIndex = 0;
    int argumentStartColumn = startColumn;

    for (const Token& token : tokens) {
        if (token.kind == TokenKind::EndOfFile) {
            break;
        }

        if (token.kind == TokenKind::LeftParen) {
            ++parenDepth;
        } else if (token.kind == TokenKind::RightParen && parenDepth > 0) {
            --parenDepth;
        } else if (token.kind == TokenKind::LeftBracket) {
            ++bracketDepth;
        } else if (token.kind == TokenKind::RightBracket && bracketDepth > 0) {
            --bracketDepth;
        } else if (token.kind == TokenKind::Comma && parenDepth == 0 && bracketDepth == 0) {
            const size_t endIndex = static_cast<size_t>(token.span.startColumn - 1);
            const std::string rawArgument = text.substr(argumentStartIndex, endIndex - argumentStartIndex);
            const std::string argumentText = trim(rawArgument);
            const size_t trimStart = rawArgument.find_first_not_of(" \t\r\n");
            arguments.push_back({
                argumentText,
                trimStart == std::string::npos ? argumentStartColumn : argumentStartColumn + static_cast<int>(trimStart)
            });
            argumentStartIndex = static_cast<size_t>(token.span.endColumn);
            argumentStartColumn = startColumn + token.span.endColumn;
        }
    }

    const std::string rawArgument = text.substr(argumentStartIndex);
    const std::string argumentText = trim(rawArgument);
    const size_t trimStart = rawArgument.find_first_not_of(" \t\r\n");
    arguments.push_back({
        argumentText,
        trimStart == std::string::npos ? argumentStartColumn : argumentStartColumn + static_cast<int>(trimStart)
    });
    return arguments;
}

std::string expressionSliceForTokens(
    const std::string& text,
    const std::vector<Token>& tokens,
    size_t startIndex,
    size_t endIndex
) {
    if (startIndex >= endIndex) {
        return "";
    }

    const int startColumn = tokens[startIndex].span.startColumn;
    const int endColumn = tokens[endIndex - 1].span.endColumn;
    return trim(text.substr(
        static_cast<size_t>(startColumn - 1),
        static_cast<size_t>(endColumn - startColumn + 1)
    ));
}

bool emitTypedListLiteralAt(
    const std::string& inputFile,
    int lineNumber,
    const std::string& valueText,
    int valueColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    const std::vector<Token>& tokens,
    size_t& tokenIndex,
    const Type& targetType,
    std::string& emittedValue
) {
    if (!isListType(targetType)) {
        return false;
    }

    if (tokenIndex >= tokens.size() || tokens[tokenIndex].kind != TokenKind::LeftBracket) {
        return false;
    }

    const std::string elementCppType = cppTypeForType(targetType.subtypes[0]);
    if (elementCppType.empty()) {
        return false;
    }

    const Token& leftBracket = tokens[tokenIndex];
    ++tokenIndex;
    if (tokenIndex < tokens.size() && tokens[tokenIndex].kind == TokenKind::RightBracket) {
        ++tokenIndex;
        emittedValue = "vector<" + elementCppType + ">{}";
        return true;
    }

    std::vector<std::string> elements;
    while (tokenIndex < tokens.size() && tokens[tokenIndex].kind != TokenKind::EndOfFile) {
        std::string emittedElement;
        const Type& elementType = targetType.subtypes[0];
        if (isListType(elementType) && tokens[tokenIndex].kind == TokenKind::LeftBracket) {
            if (!emitTypedListLiteralAt(
                    inputFile,
                    lineNumber,
                    valueText,
                    valueColumn,
                    sourceLines,
                    declaredVariables,
                    tokens,
                    tokenIndex,
                    elementType,
                    emittedElement)) {
                return false;
            }
        } else {
            const size_t elementStart = tokenIndex;
            int parenDepth = 0;
            int bracketDepth = 0;
            while (tokenIndex < tokens.size()) {
                const Token& token = tokens[tokenIndex];
                if (token.kind == TokenKind::EndOfFile) {
                    break;
                }
                if (token.kind == TokenKind::LeftParen) {
                    ++parenDepth;
                } else if (token.kind == TokenKind::RightParen) {
                    if (parenDepth > 0) {
                        --parenDepth;
                    }
                } else if (token.kind == TokenKind::LeftBracket) {
                    ++bracketDepth;
                } else if (token.kind == TokenKind::RightBracket) {
                    if (bracketDepth == 0 && parenDepth == 0) {
                        break;
                    }
                    if (bracketDepth > 0) {
                        --bracketDepth;
                    }
                } else if (token.kind == TokenKind::Comma && parenDepth == 0 && bracketDepth == 0) {
                    break;
                }
                ++tokenIndex;
            }

            const std::string elementText = expressionSliceForTokens(valueText, tokens, elementStart, tokenIndex);
            if (elementText.empty()) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    valueColumn + leftBracket.span.startColumn - 1,
                    "expected expression in list literal",
                    sourceLines
                );
                return false;
            }

            if (isListType(elementType) && tokens[elementStart].kind == TokenKind::LeftBracket) {
                const std::vector<Token> nestedTokens = tokenize(elementText);
                size_t nestedTokenIndex = 0;
                if (emitTypedListLiteralAt(
                        inputFile,
                        lineNumber,
                        elementText,
                        valueColumn + tokens[elementStart].span.startColumn - 1,
                        sourceLines,
                        declaredVariables,
                        nestedTokens,
                        nestedTokenIndex,
                        elementType,
                        emittedElement)) {
                    if (nestedTokens[nestedTokenIndex].kind != TokenKind::EndOfFile) {
                        recordSourceError(
                            inputFile,
                            lineNumber,
                            valueColumn + tokens[elementStart].span.startColumn - 1 + nestedTokens[nestedTokenIndex].span.startColumn - 1,
                            "unexpected token in list literal",
                            sourceLines
                        );
                        return false;
                    }
                } else {
                    return false;
                }
            } else {
            const ExpressionEmitResult expression = emitExpression(
                inputFile,
                lineNumber,
                elementText,
                valueColumn + tokens[elementStart].span.startColumn - 1,
                sourceLines,
                declaredVariables
            );
            if (!expression.ok) {
                return false;
            }

            if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, elementType)) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    valueColumn + tokens[elementStart].span.startColumn - 1,
                    "cannot implicitly convert " + cpppTypeName(expression.type) + " to " + cpppTypeName(elementType) + " in list literal",
                    sourceLines
                );
                return false;
            }

            emittedElement = expression.generatedExpression;
            if (!isImplicitlyConvertible(expression.type, elementType) || expression.type != elementType) {
                emittedElement = castExpressionTo(emittedElement, expression.type, elementType);
            }
            }
        }

        elements.push_back(emittedElement);

        if (tokenIndex >= tokens.size()) {
            break;
        }

        if (tokens[tokenIndex].kind == TokenKind::Comma) {
            const Token& comma = tokens[tokenIndex];
            ++tokenIndex;
            if (tokenIndex >= tokens.size() ||
                tokens[tokenIndex].kind == TokenKind::RightBracket ||
                tokens[tokenIndex].kind == TokenKind::EndOfFile) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    valueColumn + comma.span.startColumn - 1,
                    "expected expression after ',' in list literal",
                    sourceLines
                );
                return false;
            }
            continue;
        }

        if (tokens[tokenIndex].kind == TokenKind::RightBracket) {
            ++tokenIndex;
            emittedValue = "vector<" + elementCppType + ">{";
            for (size_t i = 0; i < elements.size(); ++i) {
                if (i > 0) {
                    emittedValue += ", ";
                }
                emittedValue += elements[i];
            }
            emittedValue += "}";
            return true;
        }

        break;
    }

    recordSourceError(
        inputFile,
        lineNumber,
        valueColumn + leftBracket.span.startColumn - 1,
        "unclosed bracket in list literal",
        sourceLines
    );
    return false;
}
}

// listRuntimeHelpers handles list-specific behavior for the compiler or runtime.
std::vector<RuntimeHelper> listRuntimeHelpers() {
    return {
        {
            "CPPPListInsert",
            {
                "template <typename T, typename U>",
                "void CPPPListInsert(vector<T>& list, const U& value, long long index, int line, int column) {",
                "    if (index < 0 || index > static_cast<long long>(list.size())) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid list index\");",
                "    }",
                "    list.insert(list.begin() + static_cast<typename vector<T>::difference_type>(index), value);",
                "}",
                ""
            },
            {},
            {"CPPPListInsert("}
        },
        {
            "CPPPListPop",
            {
                "template <typename T>",
                "T CPPPListPop(vector<T>& list, int line, int column) {",
                "    if (list.empty()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":cannot remove from empty list\");",
                "    }",
                "    T value = list.back();",
                "    list.pop_back();",
                "    return value;",
                "}",
                ""
            },
            {},
            {"CPPPListPop("}
        },
        {
            "CPPPListRemoveAt",
            {
                "template <typename T>",
                "T CPPPListRemoveAt(vector<T>& list, long long index, int line, int column) {",
                "    if (index < 0 || index >= static_cast<long long>(list.size())) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid list index\");",
                "    }",
                "    auto iterator = list.begin() + static_cast<typename vector<T>::difference_type>(index);",
                "    T value = *iterator;",
                "    list.erase(iterator);",
                "    return value;",
                "}",
                ""
            },
            {},
            {"CPPPListRemoveAt("}
        },
        {
            "CPPPListSet",
            {
                "template <typename T, typename U>",
                "void CPPPListSet(vector<T>& list, long long index, const U& value, int line, int column) {",
                "    if (index < 0 || index >= static_cast<long long>(list.size())) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid list index\");",
                "    }",
                "    list[static_cast<typename vector<T>::difference_type>(index)] = value;",
                "}",
                ""
            },
            {},
            {"CPPPListSet("}
        },
        {
            "CPPPListAt",
            {
                "template <typename T>",
                "long long CPPPNormalizeListIndex(const vector<T>& list, long long index, int line, int column) {",
                "    if (index < 0) {",
                "        index += static_cast<long long>(list.size());",
                "    }",
                "    if (index < 0 || index >= static_cast<long long>(list.size())) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid list index\");",
                "    }",
                "    return index;",
                "}",
                "",
                "template <typename T>",
                "typename vector<T>::const_reference CPPPListAt(const vector<T>& list, long long index, int line, int column) {",
                "    return list[static_cast<typename vector<T>::difference_type>(CPPPNormalizeListIndex(list, index, line, column))];",
                "}",
                ""
            },
            {},
            {"CPPPListAt("}
        },
        {
            "CPPPListRef",
            {
                "template <typename T>",
                "decltype(auto) CPPPListRef(vector<T>& list, long long index, int line, int column) {",
                "    return list[static_cast<typename vector<T>::difference_type>(CPPPNormalizeListIndex(list, index, line, column))];",
                "}",
                ""
            },
            {"CPPPListAt"},
            {"CPPPListRef("}
        },
        {
            "CPPPListSlice",
            {
                "template <typename T>",
                "vector<T> CPPPListSlice(const vector<T>& list, long long start, long long end) {",
                "    const long long size = static_cast<long long>(list.size());",
                "    if (start < 0) {",
                "        start += size;",
                "    }",
                "    if (end < 0) {",
                "        end += size;",
                "    }",
                "    start = max(0LL, min(start, size));",
                "    end = max(0LL, min(end, size));",
                "    if (start >= end) {",
                "        return {};",
                "    }",
                "    return vector<T>(",
                "        list.begin() + static_cast<typename vector<T>::difference_type>(start),",
                "        list.begin() + static_cast<typename vector<T>::difference_type>(end)",
                "    );",
                "}",
                ""
            },
            {},
            {"CPPPListSlice("}
        },
        {
            "CPPPListContainsSublist",
            {
                "template <typename T>",
                "bool CPPPListContainsSublist(const vector<T>& haystack, const vector<T>& needle) {",
                "    if (needle.empty()) {",
                "        return true;",
                "    }",
                "    if (needle.size() > haystack.size()) {",
                "        return false;",
                "    }",
                "    vector<size_t> prefix(needle.size(), 0);",
                "    for (size_t i = 1, matched = 0; i < needle.size(); ++i) {",
                "        while (matched > 0 && needle[i] != needle[matched]) {",
                "            matched = prefix[matched - 1];",
                "        }",
                "        if (needle[i] == needle[matched]) {",
                "            ++matched;",
                "        }",
                "        prefix[i] = matched;",
                "    }",
                "    for (size_t i = 0, matched = 0; i < haystack.size(); ++i) {",
                "        while (matched > 0 && haystack[i] != needle[matched]) {",
                "            matched = prefix[matched - 1];",
                "        }",
                "        if (haystack[i] == needle[matched]) {",
                "            ++matched;",
                "            if (matched == needle.size()) {",
                "                return true;",
                "            }",
                "        }",
                "    }",
                "    return false;",
                "}",
                ""
            },
            {},
            {"CPPPListContainsSublist("}
        },
        {
            "CPPPListFindValue",
            {
                "template <typename T, typename U>",
                "vector<long long> CPPPListFindValue(const vector<T>& haystack, const U& needle) {",
                "    vector<long long> matches;",
                "    for (size_t i = 0; i < haystack.size(); ++i) {",
                "        if (haystack[i] == needle) {",
                "            matches.push_back(static_cast<long long>(i));",
                "        }",
                "    }",
                "    return matches;",
                "}",
                ""
            },
            {},
            {"CPPPListFindValue("}
        },
        {
            "CPPPListFindSublist",
            {
                "template <typename T>",
                "vector<long long> CPPPListFindSublist(const vector<T>& haystack, const vector<T>& needle) {",
                "    vector<long long> matches;",
                "    if (needle.empty()) {",
                "        for (size_t i = 0; i <= haystack.size(); ++i) {",
                "            matches.push_back(static_cast<long long>(i));",
                "        }",
                "        return matches;",
                "    }",
                "    if (needle.size() > haystack.size()) {",
                "        return matches;",
                "    }",
                "    vector<size_t> prefix(needle.size(), 0);",
                "    for (size_t i = 1, matched = 0; i < needle.size(); ++i) {",
                "        while (matched > 0 && needle[i] != needle[matched]) {",
                "            matched = prefix[matched - 1];",
                "        }",
                "        if (needle[i] == needle[matched]) {",
                "            ++matched;",
                "        }",
                "        prefix[i] = matched;",
                "    }",
                "    for (size_t i = 0, matched = 0; i < haystack.size(); ++i) {",
                "        while (matched > 0 && haystack[i] != needle[matched]) {",
                "            matched = prefix[matched - 1];",
                "        }",
                "        if (haystack[i] == needle[matched]) {",
                "            ++matched;",
                "            if (matched == needle.size()) {",
                "                matches.push_back(static_cast<long long>(i + 1 - matched));",
                "                matched = prefix[matched - 1];",
                "            }",
                "        }",
                "    }",
                "    return matches;",
                "}",
                ""
            },
            {},
            {"CPPPListFindSublist("}
        },
        {
            "CPPPListSplitValue",
            {
                "template <typename T, typename U>",
                "vector<vector<T>> CPPPListSplitValue(const vector<T>& haystack, const U& needle) {",
                "    vector<vector<T>> parts;",
                "    vector<T> current;",
                "    bool matched = false;",
                "    for (const T& value : haystack) {",
                "        if (value == needle) {",
                "            matched = true;",
                "            if (!current.empty()) {",
                "                parts.push_back(current);",
                "                current.clear();",
                "            }",
                "        } else {",
                "            current.push_back(value);",
                "        }",
                "    }",
                "    if (!current.empty() || !matched) {",
                "        parts.push_back(current);",
                "    }",
                "    return parts;",
                "}",
                ""
            },
            {},
            {"CPPPListSplitValue("}
        },
        {
            "CPPPListSplitSublist",
            {
                "template <typename T>",
                "vector<vector<T>> CPPPListSplitSublist(const vector<T>& haystack, const vector<T>& needle) {",
                "    if (needle.empty()) {",
                "        return {haystack};",
                "    }",
                "    vector<vector<T>> parts;",
                "    auto start = haystack.begin();",
                "    bool matched = false;",
                "    while (true) {",
                "        auto found = search(start, haystack.end(), needle.begin(), needle.end());",
                "        if (found == haystack.end()) {",
                "            break;",
                "        }",
                "        matched = true;",
                "        if (start != found) {",
                "            parts.emplace_back(start, found);",
                "        }",
                "        start = found + static_cast<typename vector<T>::difference_type>(needle.size());",
                "    }",
                "    if (start != haystack.end() || !matched) {",
                "        parts.emplace_back(start, haystack.end());",
                "    }",
                "    return parts;",
                "}",
                ""
            },
            {},
            {"CPPPListSplitSublist("}
        },
        {
            "CPPPListMin",
            {
                "template <typename T>",
                "typename vector<T>::const_reference CPPPListMin(const vector<T>& list, int line, int column) {",
                "    if (list.empty()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":cannot take min of empty list\");",
                "    }",
                "    return *min_element(list.begin(), list.end());",
                "}",
                ""
            },
            {},
            {"CPPPListMin("}
        },
        {
            "CPPPListMax",
            {
                "template <typename T>",
                "typename vector<T>::const_reference CPPPListMax(const vector<T>& list, int line, int column) {",
                "    if (list.empty()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":cannot take max of empty list\");",
                "    }",
                "    return *max_element(list.begin(), list.end());",
                "}",
                ""
            },
            {},
            {"CPPPListMax("}
        },
        {
            "CPPPSetRemove",
            {
                "template <typename T, typename U>",
                "T CPPPSetRemove(set<T>& values, const U& key, int line, int column) {",
                "    T lookupKey = key;",
                "    auto iterator = values.find(lookupKey);",
                "    if (iterator == values.end()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":key not found in set\");",
                "    }",
                "    T value = *iterator;",
                "    values.erase(iterator);",
                "    return value;",
                "}",
                ""
            },
            {},
            {"CPPPSetRemove("}
        },
        {
            "CPPPMapRemove",
            {
                "template <typename K, typename V, typename U>",
                "V CPPPMapRemove(map<K, V>& values, const U& key, int line, int column) {",
                "    K lookupKey = key;",
                "    auto iterator = values.find(lookupKey);",
                "    if (iterator == values.end()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":key not found in map\");",
                "    }",
                "    V value = iterator->second;",
                "    values.erase(iterator);",
                "    return value;",
                "}",
                ""
            },
            {},
            {"CPPPMapRemove("}
        },
        {
            "CPPPMapAt",
            {
                "template <typename K, typename V, typename U>",
                "const V& CPPPMapAt(const map<K, V>& values, const U& key, int line, int column) {",
                "    K lookupKey = key;",
                "    auto iterator = values.find(lookupKey);",
                "    if (iterator == values.end()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":key not found in map\");",
                "    }",
                "    return iterator->second;",
                "}",
                ""
            },
            {},
            {"CPPPMapAt("}
        }
    };
}

ListEmitResult emitListStatement(
    const std::string& inputFile,
    int lineNumber,
    const std::string& statementBody,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks
) {
    const std::vector<Token> tokens = tokenize(statementBody);
    if (tokens.size() < 6 ||
        tokens[tokens.size() - 2].kind != TokenKind::RightParen ||
        tokens.back().kind != TokenKind::EndOfFile) {
        return {false, true, "", {}};
    }

    size_t leftParenIndex = tokens.size();
    int parenDepth = 0;
    for (size_t i = tokens.size() - 2; i > 0; --i) {
        if (tokens[i].kind == TokenKind::RightParen) {
            ++parenDepth;
        } else if (tokens[i].kind == TokenKind::LeftParen) {
            --parenDepth;
            if (parenDepth == 0) {
                leftParenIndex = i;
                break;
            }
        }
    }
    if (leftParenIndex == tokens.size() ||
        leftParenIndex < 2 ||
        tokens[leftParenIndex - 1].kind != TokenKind::Identifier ||
        tokens[leftParenIndex - 2].kind != TokenKind::Operator ||
        tokens[leftParenIndex - 2].text != ".") {
        return {false, true, "", {}};
    }

    const size_t methodIndex = leftParenIndex - 1;
    const size_t dotIndex = leftParenIndex - 2;
    const std::string actionName = tokens[methodIndex].text;
    if (actionName != "add" && actionName != "remove" && actionName != "sort" && actionName != "reverse") {
        return {false, true, "", {}};
    }

    if (dotIndex == 0) {
        return {false, true, "", {}};
    }

    const std::string receiverText = expressionSliceForTokens(statementBody, tokens, 0, dotIndex);
    if (receiverText.empty()) {
        return {false, true, "", {}};
    }

    const LvalueEmitResult receiver = emitLvalueExpression(
        inputFile,
        lineNumber,
        receiverText,
        tokens[0].span.startColumn,
        sourceLines,
        declaredVariables,
        emitRuntimeChecks
    );
    if (!receiver.ok) {
        return {true, false, "", {}};
    }

    const bool isAdd = actionName == "add";
    const bool isSort = actionName == "sort";
    const bool isReverse = actionName == "reverse";
    const bool receiverIsList = isListType(receiver.type);
    const bool receiverIsSet = isSetType(receiver.type);
    const bool receiverIsMap = isMapType(receiver.type);

    if (!receiverIsList && !receiverIsSet && !receiverIsMap) {
        recordSourceError(inputFile, lineNumber, tokens[methodIndex].span.startColumn, actionName + "() can only be used on collection values", sourceLines);
        return {true, false, "", {}};
    }

    const Token& leftParen = tokens[leftParenIndex];
    const Token& rightParen = tokens[tokens.size() - 2];
    const std::string argumentsText = statementBody.substr(
        static_cast<size_t>(leftParen.span.endColumn),
        static_cast<size_t>(rightParen.span.startColumn - leftParen.span.endColumn - 1)
    );
    const int argumentsStartColumn = leftParen.span.endColumn + 1;
    const std::vector<ListArgument> arguments = splitListArguments(argumentsText, argumentsStartColumn);

    const Type elementType = receiver.type.subtypes[0];

    if (isSort || isReverse) {
        if (!receiverIsList) {
            recordSourceError(inputFile, lineNumber, tokens[methodIndex].span.startColumn, actionName + "() can only be used on List values", sourceLines);
            return {true, false, "", {}};
        }

        if (arguments.size() != 1 || !arguments[0].text.empty()) {
            recordSourceError(
                inputFile,
                lineNumber,
                argumentsStartColumn,
                actionName + "() does not take arguments",
                sourceLines
            );
            return {true, false, "", {}};
        }

        const std::string generatedStatement = isSort
            ? "    sort((" + receiver.generatedExpression + ").begin(), (" + receiver.generatedExpression + ").end());"
            : "    reverse((" + receiver.generatedExpression + ").begin(), (" + receiver.generatedExpression + ").end());";

        return {
            true,
            true,
            generatedStatement,
            {{
                lineNumber,
                receiver.sourceColumn,
                5,
                5 + static_cast<int>(receiver.generatedExpression.size()) - 1
            }}
        };
    }

    if (isAdd) {
        if (receiverIsMap) {
            recordSourceError(inputFile, lineNumber, tokens[methodIndex].span.startColumn, "add() can only be used on List or Set values", sourceLines);
            return {true, false, "", {}};
        }

        if (arguments.size() == 1 && arguments[0].text.empty()) {
            recordSourceError(inputFile, lineNumber, argumentsStartColumn, "add() expects value or value, index", sourceLines);
            return {true, false, "", {}};
        }

        if (arguments.size() != 1 && arguments.size() != 2) {
            recordSourceError(inputFile, lineNumber, argumentsStartColumn, "add() expects value or value, index", sourceLines);
            return {true, false, "", {}};
        }

        for (const ListArgument& argument : arguments) {
            if (argument.text.empty()) {
                recordSourceError(inputFile, lineNumber, argument.column, "add() argument cannot be empty", sourceLines);
                return {true, false, "", {}};
            }
        }

        std::string emittedValue;
        const std::vector<Token> valueTokens = tokenize(arguments[0].text);
        size_t listTokenIndex = 0;
        if (valueTokens.size() > 1 &&
            valueTokens[0].kind == TokenKind::LeftBracket &&
            isListType(elementType)) {
            if (!emitTypedListLiteralAt(
                inputFile,
                lineNumber,
                arguments[0].text,
                arguments[0].column,
                sourceLines,
                declaredVariables,
                valueTokens,
                listTokenIndex,
                elementType,
                emittedValue)) {
                return {true, false, "", {}};
            }

            if (valueTokens[listTokenIndex].kind != TokenKind::EndOfFile) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    arguments[0].column + valueTokens[listTokenIndex].span.startColumn - 1,
                    "unexpected token in list literal",
                    sourceLines
                );
                return {true, false, "", {}};
            }
        } else {
            const ExpressionEmitResult value = emitExpression(
                inputFile,
                lineNumber,
                arguments[0].text,
                arguments[0].column,
                sourceLines,
                declaredVariables
            );
            if (!value.ok) {
                return {true, false, "", {}};
            }

            if (!value.explicitCast && !isImplicitlyConvertible(value.type, elementType)) {
                recordSourceError(
                    inputFile,
                    lineNumber,
                    arguments[0].column,
                    "cannot add " + cpppTypeName(value.type) + " to " + cpppTypeName(receiver.type),
                    sourceLines
                );
                return {true, false, "", {}};
            }

            emittedValue = value.generatedExpression;
            if (!isImplicitlyConvertible(value.type, elementType) || value.type != elementType) {
                emittedValue = castExpressionTo(emittedValue, value.type, elementType);
            }
        }

        if (arguments.size() == 1) {
            return {
                true,
                true,
                receiverIsList
                    ? "    (" + receiver.generatedExpression + ").push_back(" + emittedValue + ");"
                    : "    (" + receiver.generatedExpression + ").insert(" + emittedValue + ");",
                {{
                    lineNumber,
                    receiver.sourceColumn,
                    5,
                    5 + static_cast<int>(receiver.generatedExpression.size()) - 1
                }}
            };
        }

        if (!receiverIsList) {
            recordSourceError(inputFile, lineNumber, argumentsStartColumn, "Set.add() takes exactly one value", sourceLines);
            return {true, false, "", {}};
        }

        const ExpressionEmitResult index = emitExpression(
            inputFile,
            lineNumber,
            arguments[1].text,
            arguments[1].column,
            sourceLines,
            declaredVariables
        );
        if (!index.ok) {
            return {true, false, "", {}};
        }

        if (!index.explicitCast && !isImplicitlyConvertible(index.type, PrimitiveType::Int)) {
            recordSourceError(
                inputFile,
                lineNumber,
                arguments[1].column,
                "list index must be int",
                sourceLines
            );
            return {true, false, "", {}};
        }

        std::string emittedIndex = index.generatedExpression;
        if (!isImplicitlyConvertible(index.type, PrimitiveType::Int) || index.type != PrimitiveType::Int) {
            emittedIndex = castExpressionTo(emittedIndex, index.type, PrimitiveType::Int);
        }

        if (emitRuntimeChecks) {
            requireRuntimeHelper("CPPPListInsert");
        }
        const std::string generatedStatement = emitRuntimeChecks
            ? "    CPPPListInsert(" + receiver.generatedExpression + ", " + emittedValue + ", " + emittedIndex + ", " + std::to_string(lineNumber) + ", " + std::to_string(arguments[1].column) + ");"
            : "    (" + receiver.generatedExpression + ").insert((" + receiver.generatedExpression + ").begin() + " + emittedIndex + ", " + emittedValue + ");";

        return {
            true,
            true,
            generatedStatement,
            {{
                lineNumber,
                receiver.sourceColumn,
                5,
                5 + static_cast<int>(receiver.generatedExpression.size()) - 1
            }}
        };
    }

    if (receiverIsList && arguments.size() == 1 && arguments[0].text.empty()) {
        if (emitRuntimeChecks) {
            requireRuntimeHelper("CPPPListPop");
        }
        const std::string generatedStatement = emitRuntimeChecks
            ? "    CPPPListPop(" + receiver.generatedExpression + ", " + std::to_string(lineNumber) + ", " + std::to_string(tokens[methodIndex].span.startColumn) + ");"
            : "    (" + receiver.generatedExpression + ").pop_back();";

        return {
            true,
            true,
            generatedStatement,
            {{
                lineNumber,
                receiver.sourceColumn,
                5,
                5 + static_cast<int>(receiver.generatedExpression.size()) - 1
            }}
        };
    }

    if (arguments.size() != 1) {
        recordSourceError(
            inputFile,
            lineNumber,
            argumentsStartColumn,
            receiverIsList ? "remove() expects no arguments or index" : "remove() expects exactly one key or value",
            sourceLines
        );
        return {true, false, "", {}};
    }

    if (arguments[0].text.empty()) {
        recordSourceError(inputFile, lineNumber, arguments[0].column, "remove() argument cannot be empty", sourceLines);
        return {true, false, "", {}};
    }

    const ExpressionEmitResult index = emitExpression(
        inputFile,
        lineNumber,
        arguments[0].text,
        arguments[0].column,
        sourceLines,
        declaredVariables
    );
    if (!index.ok) {
        return {true, false, "", {}};
    }

    std::string emittedIndex = index.generatedExpression;
    std::string generatedStatement;
    if (receiverIsList) {
        if (!index.explicitCast && !isImplicitlyConvertible(index.type, PrimitiveType::Int)) {
            recordSourceError(
                inputFile,
                lineNumber,
                arguments[0].column,
                "list index must be int",
                sourceLines
            );
            return {true, false, "", {}};
        }

        if (!isImplicitlyConvertible(index.type, PrimitiveType::Int) || index.type != PrimitiveType::Int) {
            emittedIndex = castExpressionTo(emittedIndex, index.type, PrimitiveType::Int);
        }

        if (emitRuntimeChecks) {
            requireRuntimeHelper("CPPPListRemoveAt");
        }
        generatedStatement = emitRuntimeChecks
            ? "    CPPPListRemoveAt(" + receiver.generatedExpression + ", " + emittedIndex + ", " + std::to_string(lineNumber) + ", " + std::to_string(arguments[0].column) + ");"
            : "    (" + receiver.generatedExpression + ").erase((" + receiver.generatedExpression + ").begin() + " + emittedIndex + ");";
    } else {
        const Type expectedType = receiver.type.subtypes[0];
        if (!index.explicitCast && !isImplicitlyConvertible(index.type, expectedType)) {
            recordSourceError(
                inputFile,
                lineNumber,
                arguments[0].column,
                "cannot remove " + cpppTypeName(index.type) + " from " + cpppTypeName(receiver.type),
                sourceLines
            );
            return {true, false, "", {}};
        }

        if (!isImplicitlyConvertible(index.type, expectedType) || index.type != expectedType) {
            emittedIndex = castExpressionTo(emittedIndex, index.type, expectedType);
        }

        if (receiverIsSet) {
            requireRuntimeHelper("CPPPSetRemove");
            generatedStatement = "    CPPPSetRemove(" + receiver.generatedExpression + ", " + emittedIndex + ", " + std::to_string(lineNumber) + ", " + std::to_string(arguments[0].column) + ");";
        } else {
            requireRuntimeHelper("CPPPMapRemove");
            generatedStatement = "    CPPPMapRemove(" + receiver.generatedExpression + ", " + emittedIndex + ", " + std::to_string(lineNumber) + ", " + std::to_string(arguments[0].column) + ");";
        }
    }

    return {
        true,
        true,
        generatedStatement,
        {{
            lineNumber,
            receiver.sourceColumn,
            5,
            5 + static_cast<int>(receiver.generatedExpression.size()) - 1
        }}
    };
}
