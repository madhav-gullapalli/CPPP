#include "expressions.h"

#include "expressionParser.h"

namespace {
bool& expressionRuntimeChecksEnabled() {
    static bool enabled = false;
    return enabled;
}

std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

int listDepth(const Type& type) {
    int depth = 0;
    Type current = type;
    while (current.primitive == PrimitiveType::List && current.subtypes.size() == 1) {
        ++depth;
        current = current.subtypes[0];
    }
    return depth;
}

std::string emitListInputExpression(
    const Type& currentType,
    const std::vector<std::string>& dimensions,
    size_t dimensionIndex
) {
    if (currentType.primitive != PrimitiveType::List || currentType.subtypes.size() != 1) {
        return inputFunctionForType(currentType);
    }

    const std::string elementExpression = emitListInputExpression(currentType.subtypes[0], dimensions, dimensionIndex + 1);
    return "CPPPInputList(" + dimensions[dimensionIndex] + ", [&]() { return " + elementExpression + "; })";
}
}

int primitiveArity(PrimitiveType primitive) {
    switch (primitive) {
        case PrimitiveType::Bool:
        case PrimitiveType::Char:
        case PrimitiveType::Int:
        case PrimitiveType::Float:
            return 0;
        case PrimitiveType::List:
            return 1;
        case PrimitiveType::Unknown:
            return 0;
    }

    return 0;
}

std::string cpppTypeName(const Type& type) {
    if (isStringType(type)) {
        return "string";
    }

    switch (type.primitive) {
        case PrimitiveType::Bool:
            return "bool";
        case PrimitiveType::Char:
            return "char";
        case PrimitiveType::Int:
            return "int";
        case PrimitiveType::Float:
            return "float";
        case PrimitiveType::List:
            if (type.subtypes.size() == 1) {
                return "List<" + cpppTypeName(type.subtypes[0]) + ">";
            }
            return "List";
        case PrimitiveType::Unknown:
            return "unknown";
    }

    return "unknown";
}

bool isStringType(const Type& type) {
    return type.primitive == PrimitiveType::List &&
        type.subtypes.size() == 1 &&
        type.subtypes[0] == PrimitiveType::Char;
}

bool isImplicitlyConvertible(const Type& from, const Type& to) {
    if (to == PrimitiveType::Bool && from.primitive == PrimitiveType::List && from.subtypes.size() == 1) {
        return true;
    }

    if (!from.subtypes.empty() || !to.subtypes.empty()) {
        return from == to;
    }

    if (from == to) {
        return true;
    }

    if (from == PrimitiveType::Bool) {
        return to == PrimitiveType::Char || to == PrimitiveType::Int || to == PrimitiveType::Float;
    }

    if (from == PrimitiveType::Char) {
        return to == PrimitiveType::Bool || to == PrimitiveType::Int || to == PrimitiveType::Float;
    }

    if (from == PrimitiveType::Int) {
        return to == PrimitiveType::Bool || to == PrimitiveType::Float;
    }

    if (from == PrimitiveType::Float) {
        return to == PrimitiveType::Bool;
    }

    if (from.primitive == PrimitiveType::List || to.primitive == PrimitiveType::List) {
        return from == to;
    }

    return false;
}

std::string castExpressionTo(const std::string& expression, const Type& to) {
    return castExpressionTo(expression, PrimitiveType::Unknown, to);
}

std::string castExpressionTo(const std::string& expression, const Type& from, const Type& to) {
    switch (to.primitive) {
        case PrimitiveType::Bool:
            switch (from.primitive) {
                case PrimitiveType::Bool:
                    return "CPPPToBoolBool(" + expression + ")";
                case PrimitiveType::Char:
                    return "CPPPToBoolChar(" + expression + ")";
                case PrimitiveType::Int:
                    return "CPPPToBoolInt(" + expression + ")";
                case PrimitiveType::Float:
                    return "CPPPToBoolFloat(" + expression + ")";
                case PrimitiveType::List:
                    return "(!(" + expression + ").empty())";
                case PrimitiveType::Unknown:
                    return "CPPPToBool(" + expression + ")";
            }
            return "CPPPToBool(" + expression + ")";
        case PrimitiveType::Char:
            return "CPPPChar(static_cast<char>(" + expression + "))";
        case PrimitiveType::Int:
            return "static_cast<long long>(" + expression + ")";
        case PrimitiveType::Float:
            return "static_cast<long double>(" + expression + ")";
        case PrimitiveType::List:
        case PrimitiveType::Unknown:
            return expression;
    }

    return expression;
}

Type declaredTypeForName(const std::string& name) {
    if (name == "bool") {
        return PrimitiveType::Bool;
    }
    if (name == "char") {
        return PrimitiveType::Char;
    }
    if (name == "int") {
        return PrimitiveType::Int;
    }
    if (name == "float") {
        return PrimitiveType::Float;
    }
    if (name == "List") {
        return PrimitiveType::List;
    }
    if (name == "string") {
        return Type(PrimitiveType::List, {Type(PrimitiveType::Char)});
    }

    return PrimitiveType::Unknown;
}

std::string cppTypeForInput(const Type& type) {
    if (type == PrimitiveType::Bool) {
        return "bool";
    }
    if (type == PrimitiveType::Char) {
        return "CPPPChar";
    }
    if (type == PrimitiveType::Int) {
        return "long long";
    }
    if (type == PrimitiveType::Float) {
        return "long double";
    }
    if (type.primitive == PrimitiveType::List && type.subtypes.size() == 1) {
        return "vector<" + cppTypeForInput(type.subtypes[0]) + ">";
    }
    return "";
}

bool isInputCall(const std::vector<Token>& tokens) {
    return tokens.size() == 4 &&
        tokens[0].kind == TokenKind::Identifier &&
        tokens[0].text == "input" &&
        tokens[1].kind == TokenKind::LeftParen &&
        tokens[2].kind == TokenKind::RightParen &&
        tokens[3].kind == TokenKind::EndOfFile;
}

bool parseInputCall(const std::string& text, int startColumn, std::vector<InputArgument>& arguments) {
    const std::vector<Token> tokens = tokenize(text);
    if (tokens.size() < 4 ||
        tokens[0].kind != TokenKind::Identifier ||
        tokens[0].text != "input" ||
        tokens[1].kind != TokenKind::LeftParen ||
        tokens.back().kind != TokenKind::EndOfFile ||
        tokens[tokens.size() - 2].kind != TokenKind::RightParen) {
        return false;
    }

    const int argumentsStartColumn = startColumn + tokens[1].span.endColumn;
    const size_t contentStart = static_cast<size_t>(tokens[1].span.endColumn);
    const size_t contentLength = static_cast<size_t>(tokens[tokens.size() - 2].span.startColumn - tokens[1].span.endColumn - 1);
    const std::string content = text.substr(contentStart, contentLength);
    const std::vector<Token> contentTokens = tokenize(content);

    int parenDepth = 0;
    int bracketDepth = 0;
    size_t argumentStartIndex = 0;
    int argumentColumn = argumentsStartColumn;

    for (const Token& token : contentTokens) {
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
            const std::string rawArgument = content.substr(argumentStartIndex, static_cast<size_t>(token.span.startColumn - 1) - argumentStartIndex);
            const std::string argumentText = trim(rawArgument);
            const size_t trimStart = rawArgument.find_first_not_of(" \t\r\n");
            arguments.push_back({
                argumentText,
                trimStart == std::string::npos ? argumentColumn : argumentColumn + static_cast<int>(trimStart)
            });
            argumentStartIndex = static_cast<size_t>(token.span.endColumn);
            argumentColumn = argumentsStartColumn + token.span.endColumn;
        }
    }

    const std::string rawArgument = content.substr(argumentStartIndex);
    const std::string argumentText = trim(rawArgument);
    if (!argumentText.empty() || !content.empty()) {
        const size_t trimStart = rawArgument.find_first_not_of(" \t\r\n");
        arguments.push_back({
            argumentText,
            trimStart == std::string::npos ? argumentColumn : argumentColumn + static_cast<int>(trimStart)
        });
    }

    return true;
}

std::string inputFunctionForType(const Type& type) {
    if (isStringType(type)) {
        return "CPPPInputString()";
    }

    switch (type.primitive) {
        case PrimitiveType::Bool:
            return "CPPPInputBool()";
        case PrimitiveType::Char:
            return "CPPPInputChar()";
        case PrimitiveType::Int:
            return "CPPPInputInt()";
        case PrimitiveType::Float:
            return "CPPPInputFloat()";
        case PrimitiveType::List:
        case PrimitiveType::Unknown:
            return "";
    }

    return "";
}

bool emitInputCallForType(
    const std::string& inputFile,
    int lineNumber,
    const std::string& inputText,
    int inputColumn,
    const Type& targetType,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    std::string& emittedExpression
) {
    std::vector<InputArgument> arguments;
    if (!parseInputCall(inputText, inputColumn, arguments)) {
        return false;
    }

    if (isStringType(targetType)) {
        if (arguments.empty()) {
            emittedExpression = inputFunctionForType(targetType);
            return true;
        }
        if (arguments.size() != 1) {
            recordSourceError(inputFile, lineNumber, arguments[0].column, "string input needs exactly 1 size argument", sourceLines);
            return false;
        }

        const InputArgument& argument = arguments[0];
        if (argument.text.empty()) {
            recordSourceError(inputFile, lineNumber, argument.column, "input() size argument cannot be empty", sourceLines);
            return false;
        }

        const ExpressionEmitResult expression = emitExpression(
            inputFile,
            lineNumber,
            argument.text,
            argument.column,
            sourceLines,
            declaredVariables
        );
        if (!expression.ok) {
            return false;
        }

        if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, PrimitiveType::Int)) {
            recordSourceError(inputFile, lineNumber, argument.column, "input() size must be int", sourceLines);
            return false;
        }

        std::string emittedSize = expression.generatedExpression;
        if (!isImplicitlyConvertible(expression.type, PrimitiveType::Int) || expression.type != PrimitiveType::Int) {
            emittedSize = castExpressionTo(emittedSize, expression.type, PrimitiveType::Int);
        }
        emittedExpression = "CPPPInputString(" + emittedSize + ")";
        return true;
    }

    if (targetType.primitive != PrimitiveType::List) {
        if (!arguments.empty()) {
            recordSourceError(inputFile, lineNumber, arguments[0].column, "input(count) is only supported for List targets", sourceLines);
            return false;
        }

        emittedExpression = inputFunctionForType(targetType);
        return true;
    }

    const int depth = listDepth(targetType);
    if (arguments.empty()) {
        if (depth == 1) {
            const std::string elementCppType = cppTypeForInput(targetType.subtypes[0]);
            if (elementCppType.empty()) {
                recordSourceError(inputFile, lineNumber, inputColumn, "unsupported List input element type", sourceLines);
                return false;
            }
            emittedExpression = "CPPPInputListLine<" + elementCppType + ">()";
            return true;
        }

        recordSourceError(inputFile, lineNumber, inputColumn, "List input() without sizes only supports one-dimensional Lists; use input(n) or one size per List dimension", sourceLines);
        return false;
    }

    if (static_cast<int>(arguments.size()) != depth) {
        recordSourceError(
            inputFile,
            lineNumber,
            arguments[0].column,
            "List input needs exactly " + std::to_string(depth) + " size argument" + (depth == 1 ? "" : "s"),
            sourceLines
        );
        return false;
    }

    std::vector<std::string> dimensions;
    for (const InputArgument& argument : arguments) {
        if (argument.text.empty()) {
            recordSourceError(inputFile, lineNumber, argument.column, "input() size argument cannot be empty", sourceLines);
            return false;
        }

        const ExpressionEmitResult expression = emitExpression(
            inputFile,
            lineNumber,
            argument.text,
            argument.column,
            sourceLines,
            declaredVariables
        );
        if (!expression.ok) {
            return false;
        }

        if (!expression.explicitCast && !isImplicitlyConvertible(expression.type, PrimitiveType::Int)) {
            recordSourceError(inputFile, lineNumber, argument.column, "input() size must be int", sourceLines);
            return false;
        }

        std::string emittedSize = expression.generatedExpression;
        if (!isImplicitlyConvertible(expression.type, PrimitiveType::Int) || expression.type != PrimitiveType::Int) {
            emittedSize = castExpressionTo(emittedSize, expression.type, PrimitiveType::Int);
        }
        dimensions.push_back(emittedSize);
    }

    emittedExpression = emitListInputExpression(targetType, dimensions, 0);
    return true;
}

ExpressionEmitResult emitExpression(
    const std::string& inputFile,
    int lineNumber,
    const std::string& expressionText,
    int expressionColumn,
    const std::map<int, std::string>& sourceLines,
    const std::map<std::string, Type>& declaredVariables,
    bool emitRuntimeChecks
) {
    ExpressionParser parser(
        inputFile,
        lineNumber,
        expressionText,
        expressionColumn,
        sourceLines,
        declaredVariables,
        emitRuntimeChecks || expressionRuntimeChecksEnabled()
    );
    return parser.parse();
}

void setExpressionRuntimeChecksEnabled(bool enabled) {
    expressionRuntimeChecksEnabled() = enabled;
}

bool hasArithmeticOperator(const std::vector<Token>& tokens) {
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::Operator &&
            (token.text == "+" || token.text == "-" || token.text == "*" || token.text == "/" || token.text == "%" ||
             token.text == "<<" || token.text == ">>" ||
             token.text == "^" || token.text == "&" || token.text == "|" ||
             token.text == "&&" || token.text == "||" || token.text == "!" ||
             token.text == "<" || token.text == "<=" || token.text == ">" || token.text == ">=" ||
             token.text == "==" || token.text == "!=" ||
             token.text == "++" || token.text == "--")) {
            return true;
        }
        if (token.kind == TokenKind::LeftBracket || token.kind == TokenKind::RightBracket) {
            return true;
        }
    }

    return false;
}
