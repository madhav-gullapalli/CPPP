/*
 * statementCompiler.cpp
 *
 * Emits C++ by walking a semantically analyzed ProgramAst. Statement kinds
 * and block relationships are never rediscovered from source text.
 */

#include "statementCompiler.h"

#include "assignmentCppp.h"
#include "errors.h"
#include "expressionCodegen.h"
#include "functions.h"
#include "listsCppp.h"
#include "printCppp.h"
#include "typesCppp.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {
std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::string indentForDepth(int depth) {
    return std::string(static_cast<size_t>((depth + 1) * 4), ' ');
}

std::string indentGeneratedStatement(const std::string& generatedStatement, int depth) {
    return indentForDepth(depth) + trim(generatedStatement);
}

std::string stripGeneratedStatement(const std::string& generatedStatement) {
    std::string text = trim(generatedStatement);
    if (!text.empty() && text.back() == ';') text.pop_back();
    return text;
}

std::vector<Token> withoutTrailingSemicolon(const std::vector<Token>& tokens) {
    std::vector<Token> result;
    result.reserve(tokens.size());
    for (const Token& token : tokens) {
        if (token.kind == TokenKind::EndOfFile) break;
        result.push_back(token);
    }
    if (!result.empty() && result.back().kind == TokenKind::Semicolon) result.pop_back();

    Token eof = tokens.empty() ? Token{} : tokens.back();
    eof.kind = TokenKind::EndOfFile;
    eof.text.clear();
    if (!result.empty()) {
        const Token& last = result.back();
        eof.span.startLine = last.span.endLine;
        eof.span.endLine = last.span.endLine;
        eof.span.startColumn = last.span.endColumn + 1;
        eof.span.endColumn = eof.span.startColumn;
        eof.span.startOffset = last.span.endOffset;
        eof.span.endOffset = last.span.endOffset;
        eof.sourceSpan = last.sourceSpan.valid()
            ? SourceSpan{last.sourceSpan.source, last.sourceSpan.endOffset, last.sourceSpan.endOffset}
            : SourceSpan{};
    }
    result.push_back(std::move(eof));
    return result;
}

bool isBuiltinCallName(const std::string& name) {
    return name == "print" || name == "describe" || name == "input" ||
        name == "len" || name == "min" || name == "max" || name == "sum" ||
        name == "abs" || name == "split" || name == "copy" || name == "range";
}

bool isNamedCall(const Expr* expression, const std::string& name) {
    const auto* call = dynamic_cast<const CallExpr*>(expression);
    return call && !call->receiver && call->callee == name;
}

bool isUnsupportedBareCall(
    const Expr* expression,
    const CompileContext& context
) {
    const auto* call = dynamic_cast<const CallExpr*>(expression);
    if (!call || call->receiver) return false;
    const auto variable = context.declaredVariables.find(call->callee);
    return (variable == context.declaredVariables.end() || !isFunctionType(variable->second)) &&
        context.declaredFunctions.count(call->callee) == 0 &&
        context.declaredStructs.count(call->callee) == 0 &&
        !isBuiltinCallName(call->callee);
}

bool containsIncrementOrDecrement(const Expr* expression) {
    if (!expression) return false;
    if (const auto* unary = dynamic_cast<const UnaryExpr*>(expression)) {
        return unary->op == "++" || unary->op == "--" || containsIncrementOrDecrement(unary->operand.get());
    }
    if (const auto* field = dynamic_cast<const FieldExpr*>(expression)) return containsIncrementOrDecrement(field->base.get());
    if (const auto* binary = dynamic_cast<const BinaryExpr*>(expression)) {
        return containsIncrementOrDecrement(binary->left.get()) || containsIncrementOrDecrement(binary->right.get());
    }
    if (const auto* cast = dynamic_cast<const CastExpr*>(expression)) return containsIncrementOrDecrement(cast->operand.get());
    if (const auto* call = dynamic_cast<const CallExpr*>(expression)) {
        if (containsIncrementOrDecrement(call->receiver.get())) return true;
        for (const auto& argument : call->arguments) {
            if (containsIncrementOrDecrement(argument.get())) return true;
        }
    }
    if (const auto* index = dynamic_cast<const IndexExpr*>(expression)) {
        return containsIncrementOrDecrement(index->base.get()) || containsIncrementOrDecrement(index->index.get());
    }
    if (const auto* slice = dynamic_cast<const SliceExpr*>(expression)) {
        return containsIncrementOrDecrement(slice->base.get()) ||
            containsIncrementOrDecrement(slice->start.get()) ||
            containsIncrementOrDecrement(slice->end.get());
    }
    return false;
}

bool containsContextualEmptyLiteral(const Expr* expression) {
    if (!expression) return false;
    if (const auto* list = dynamic_cast<const ListLiteralExpr*>(expression)) {
        if (list->elements.empty()) return true;
        for (const auto& item : list->elements)
            if (containsContextualEmptyLiteral(item.get())) return true;
    }
    if (const auto* set = dynamic_cast<const SetLiteralExpr*>(expression)) {
        if (set->elements.empty()) return true;
        for (const auto& item : set->elements)
            if (containsContextualEmptyLiteral(item.get())) return true;
    }
    if (const auto* map = dynamic_cast<const MapLiteralExpr*>(expression)) {
        if (map->entries.empty()) return true;
        for (const auto& item : map->entries) {
            if (containsContextualEmptyLiteral(item.key.get()) ||
                containsContextualEmptyLiteral(item.value.get())) return true;
        }
    }
    if (const auto* pair = dynamic_cast<const PairLiteralExpr*>(expression)) {
        return containsContextualEmptyLiteral(pair->first.get()) ||
            containsContextualEmptyLiteral(pair->second.get());
    }
    if (const auto* field = dynamic_cast<const FieldExpr*>(expression))
        return containsContextualEmptyLiteral(field->base.get());
    if (const auto* unary = dynamic_cast<const UnaryExpr*>(expression))
        return containsContextualEmptyLiteral(unary->operand.get());
    if (const auto* binary = dynamic_cast<const BinaryExpr*>(expression))
        return containsContextualEmptyLiteral(binary->left.get()) ||
            containsContextualEmptyLiteral(binary->right.get());
    if (const auto* cast = dynamic_cast<const CastExpr*>(expression))
        return containsContextualEmptyLiteral(cast->operand.get());
    if (const auto* call = dynamic_cast<const CallExpr*>(expression)) {
        if (containsContextualEmptyLiteral(call->receiver.get())) return true;
        for (const auto& argument : call->arguments)
            if (containsContextualEmptyLiteral(argument.get())) return true;
    }
    if (const auto* index = dynamic_cast<const IndexExpr*>(expression))
        return containsContextualEmptyLiteral(index->base.get()) ||
            containsContextualEmptyLiteral(index->index.get());
    if (const auto* slice = dynamic_cast<const SliceExpr*>(expression))
        return containsContextualEmptyLiteral(slice->base.get()) ||
            containsContextualEmptyLiteral(slice->start.get()) ||
            containsContextualEmptyLiteral(slice->end.get());
    return false;
}

void collectCustomTypeNames(const Type& type, std::set<std::string>& names) {
    if (isStructType(type)) names.insert(type.name);
    for (const Type& subtype : type.subtypes) collectCustomTypeNames(subtype, names);
}

bool needsCharRuntimeHelperForType(const Type& type) {
    if (type == PrimitiveType::Char) return true;
    for (const Type& subtype : type.subtypes) {
        if (needsCharRuntimeHelperForType(subtype)) return true;
    }
    return false;
}

bool needsRangeRuntimeHelperForType(const Type& type) {
    if (type == PrimitiveType::Range) return true;
    for (const Type& subtype : type.subtypes) {
        if (needsRangeRuntimeHelperForType(subtype)) return true;
    }
    return false;
}

bool copyParameterEligible(const Type& type) {
    return isStringType(type) || isCollectionType(type) || isClassType(type);
}

std::string typeSyntaxDisplay(const TypeSyntax& syntax) {
    std::string result = syntax.name;
    if (!syntax.arguments.empty()) {
        result += "<";
        for (size_t index = 0; index < syntax.arguments.size(); ++index) {
            if (index > 0) result += ", ";
            result += typeSyntaxDisplay(syntax.arguments[index]);
        }
        result += ">";
    }
    return result;
}

int columnForSpan(const ProgramStatement& statement, SourceSpan span, int fallback = 1) {
    for (const Token& token : statement.syntax.tokens) {
        if (token.sourceSpan.source == span.source &&
            token.sourceSpan.startOffset == span.startOffset &&
            token.sourceSpan.endOffset == span.endOffset) {
            return statement.syntax.startColumn + token.span.startColumn - 1;
        }
    }
    return fallback;
}

int relativeColumnForSpan(const std::vector<Token>& tokens, SourceSpan span, int fallback = 1) {
    for (const Token& token : tokens) {
        if (token.sourceSpan.source == span.source &&
            token.sourceSpan.startOffset == span.startOffset &&
            token.sourceSpan.endOffset == span.endOffset) {
            return token.span.startColumn;
        }
    }
    return fallback;
}

std::string withComment(const std::string& text, const SyntaxSite& syntax) {
    return syntax.commentText.empty() ? text : text + " " + syntax.commentText;
}

bool recordContextualSuggestion(
    CompileContext& context,
    const ProgramStatement& statement,
    bool blockHeaderOnly
) {
    const std::vector<Token>& tokens = statement.syntax.tokens;
    if (tokens.empty() || tokens[0].kind != TokenKind::Identifier || !tokens[0].sourceSpan.valid()) return false;

    const auto functionVariable = context.declaredVariables.find(tokens[0].text);
    if (functionVariable != context.declaredVariables.end() && isFunctionType(functionVariable->second)) return false;

    std::vector<std::string> candidates;
    if (blockHeaderOnly) {
        candidates = {"if", "while", "for", "rep", "class", "struct"};
    } else if (tokens.size() >= 2 && tokens[1].kind == TokenKind::LeftParen) {
        candidates = {"print", "describe", "input", "len", "min", "max", "sum", "abs", "copy", "range"};
        for (const auto& function : context.declaredFunctions) candidates.push_back(function.first);
        for (const auto& structure : context.declaredStructs) candidates.push_back(structure.first);
    } else if (tokens.size() >= 2 &&
               (tokens[1].kind == TokenKind::Identifier ||
                (tokens[1].kind == TokenKind::Operator && tokens[1].text == "<"))) {
        candidates = {"bool", "char", "int", "float", "string", "List", "Set", "Map", "Pair", "Stack", "Queue", "Deque", "return"};
        for (const auto& structure : context.declaredStructs) candidates.push_back(structure.first);
    } else {
        return false;
    }

    const std::string closest = closestDiagnosticCandidate(tokens[0].text, candidates);
    if (closest.empty()) return false;
    Diagnostic diagnostic;
    diagnostic.message = "unsupported statement";
    diagnostic.labels.push_back({tokens[0].sourceSpan, "", true});
    diagnostic.suggestions.push_back({
        tokens[0].sourceSpan,
        closest,
        "did you mean '" + closest + "'?",
        SuggestionApplicability::MaybeIncorrect
    });
    recordDiagnostic(std::move(diagnostic));
    return true;
}

class AstLowerer {
public:
    AstLowerer(CompileContext& context, const AnalyzedProgramAst& analyzed) :
        context(context), analyzed(analyzed) {}

    void compile(const ProgramAst& program) {
        std::map<std::string, const AggregateDeclarationAst*> aggregates;
        for (const auto& statement : program.body.statements) {
            if (const auto* aggregate = dynamic_cast<const AggregateDeclarationAst*>(statement.get())) {
                aggregates[aggregate->name] = aggregate;
            }
        }
        // Semantic analysis registers the complete aggregate namespace before
        // member resolution. Mirror that capability in C++ with lightweight
        // declarations; the dependency order below still ensures inline
        // struct fields see complete definitions.
        std::map<std::string, size_t> emissionIndex;
        for (size_t i = 0; i < analyzed.aggregateEmissionOrder.size(); ++i) {
            emissionIndex[analyzed.aggregateEmissionOrder[i]] = i;
        }
        std::set<std::string> forwardDeclarations;
        for (const auto& aggregate : analyzed.aggregateFields) {
            for (const auto& field : aggregate.second) {
                std::set<std::string> referenced;
                collectCustomTypeNames(field.second, referenced);
                for (const std::string& name : referenced) {
                    if (name != aggregate.first && emissionIndex[name] > emissionIndex[aggregate.first]) {
                        forwardDeclarations.insert(name);
                    }
                }
            }
        }
        for (const std::string& name : forwardDeclarations) {
            context.queueTopLevelLine("struct " + name + ";");
        }
        for (const auto& aggregate : analyzed.aggregateFields) {
            for (const auto& field : aggregate.second) {
                if (isClassType(field.second) && field.second.name != aggregate.first &&
                    emissionIndex[field.second.name] > emissionIndex[aggregate.first]) {
                    deferredClassEqualityFields.insert(aggregate.first + "." + field.first);
                }
            }
        }
        if (!deferredClassEqualityFields.empty()) {
            context.queueTopLevelLine(
                "template <typename T> bool CPPPClassEqual(const cppp_smart_pointer<T>& left, "
                "const cppp_smart_pointer<T>& right) { return (left && right) ? (*left == *right) : (!left && !right); }");
        }
        for (const std::string& name : analyzed.aggregateEmissionOrder) {
            const auto aggregate = aggregates.find(name);
            if (aggregate != aggregates.end()) compileAggregate(*aggregate->second);
        }
        for (const auto& statement : program.body.statements) {
            if (!dynamic_cast<const AggregateDeclarationAst*>(statement.get())) compileStatement(*statement);
        }
        if (sawUnclosedBlock) context.blockDepth = std::max(context.blockDepth, 1);
    }

private:
    CompileContext& context;
    const AnalyzedProgramAst& analyzed;
    std::set<std::string> deferredClassEqualityFields;
    bool sawUnclosedBlock = false;

    void setRequirementOwner() {
        const std::string owner = !context.currentStructMethodName.empty()
            ? "method:" + context.currentStructName + "." + context.currentStructMethodName
            : (!context.currentStructName.empty()
                ? "struct:" + context.currentStructName
                : (context.currentTopLevelFunctionName.empty()
                    ? ""
                    : "function:" + context.currentTopLevelFunctionName));
        setRuntimeRequirementOwner(owner);
    }

    bool checkLexicalErrors(const ProgramStatement& statement) {
        const std::vector<Token>& tokens = statement.syntax.tokens;
        for (const Token& token : tokens) {
            if (token.kind == TokenKind::Unknown) {
                Diagnostic diagnostic;
                diagnostic.message = "unrecognized token '" + token.text + "'";
                diagnostic.labels.push_back({token.sourceSpan, "", true});
                diagnostic.helps.push_back("remove it or replace it with a valid CP++ token");
                recordDiagnostic(std::move(diagnostic));
                return false;
            }
            if ((token.kind != TokenKind::String && token.kind != TokenKind::Char) ||
                (token.text.size() >= 2 && token.text.front() == token.text.back())) continue;

            const bool printString = token.kind == TokenKind::String &&
                !tokens.empty() && tokens[0].kind == TokenKind::Identifier && tokens[0].text == "print";
            const char quote = token.kind == TokenKind::Char ? '\'' : '"';
            Diagnostic diagnostic;
            diagnostic.message = printString
                ? "unterminated string literal in print"
                : (token.kind == TokenKind::Char ? "unterminated char literal" : "unterminated string literal");
            diagnostic.labels.push_back({token.sourceSpan, "", true});
            const SourceSpan insertion = {token.sourceSpan.source, token.sourceSpan.endOffset, token.sourceSpan.endOffset};
            diagnostic.suggestions.push_back({
                insertion,
                std::string(1, quote),
                std::string("add a closing `") + quote + "`",
                SuggestionApplicability::MachineApplicable
            });
            recordDiagnostic(std::move(diagnostic));
            return false;
        }
        return true;
    }

    bool requireSemicolon(const ProgramStatement& node) {
        if (node.syntax.terminated) return true;
        const int insertionColumn = node.syntax.startColumn + static_cast<int>(node.syntax.codeLength);
        const SourceSpan insertion = sourceInsertionSpan(
            context.options.inputFile,
            context.sourceLines,
            node.syntax.lineNumber,
            insertionColumn
        );
        Diagnostic diagnostic;
        diagnostic.message = "missing semicolon";
        diagnostic.labels.push_back({insertion, "statement ends here", true});
        diagnostic.suggestions.push_back({
            insertion,
            ";",
            "add `;` to terminate the statement",
            SuggestionApplicability::MachineApplicable
        });
        recordDiagnostic(std::move(diagnostic));
        return false;
    }

    void enterBlock(
        const std::string& kind,
        const std::string& breakFlag = "",
        std::vector<std::string> declaredNames = {}
    ) {
        ++context.blockDepth;
        context.pushBlock(kind, breakFlag, std::move(declaredNames));
    }

    void leaveBlock(const BlockAst& block, bool emitClosingBrace = true) {
        const std::vector<std::string> names = context.blockDeclaredNames.empty()
            ? std::vector<std::string>{}
            : context.blockDeclaredNames.back();
        context.eraseDeclaredNames(names);
        if (!context.blockShadowedVariables.empty()) {
            for (const auto& shadowed : context.blockShadowedVariables.back()) {
                context.declaredVariables[shadowed.first] = shadowed.second;
            }
        }
        if (!context.blockKinds.empty()) {
            context.blockKinds.pop_back();
            context.blockBreakFlags.pop_back();
            context.blockDeclaredNames.pop_back();
            context.blockShadowedVariables.pop_back();
        }
        --context.blockDepth;
        if (!block.hasClosingSyntax) {
            sawUnclosedBlock = true;
            return;
        }
        if (emitClosingBrace) {
            context.queueGeneratedLine(
                withComment(indentForDepth(context.blockDepth) + "}", block.closingSyntax),
                block.closingSyntax.lineNumber
            );
        }
    }

    void prepareShadowedDeclaration(const std::vector<std::string>& names) {
        if (context.blockShadowedVariables.empty()) return;
        for (const std::string& name : names) {
            const auto existing = context.declaredVariables.find(name);
            if (existing == context.declaredVariables.end()) continue;
            context.blockShadowedVariables.back().emplace(name, existing->second);
            context.declaredVariables.erase(existing);
        }
    }

    void compileOwnedBlock(
        const BlockAst& block,
        const std::string& kind,
        const std::string& breakFlag = "",
        std::vector<std::string> declaredNames = {}
    ) {
        enterBlock(kind, breakFlag, std::move(declaredNames));
        for (const auto& statement : block.statements) compileStatement(*statement);
        leaveBlock(block);
    }

    void compileStatement(const ProgramStatement& statement) {
        setRequirementOwner();
        if (const auto* comment = dynamic_cast<const CommentStatementAst*>(&statement)) {
            if (!comment->syntax.commentText.empty()) {
                context.queueGeneratedLine(
                    indentForDepth(context.blockDepth) + comment->syntax.commentText,
                    comment->syntax.lineNumber
                );
            }
            return;
        }
        if (!checkLexicalErrors(statement)) return;

        if (const auto* node = dynamic_cast<const ErrorStatementAst*>(&statement)) return compileError(*node);
        if (const auto* node = dynamic_cast<const AggregateDeclarationAst*>(&statement)) return compileAggregate(*node);
        if (const auto* node = dynamic_cast<const FunctionDeclarationAst*>(&statement)) return compileFunction(*node, false);
        if (const auto* node = dynamic_cast<const IfStatementAst*>(&statement)) return compileIf(*node);
        if (const auto* node = dynamic_cast<const WhileStatementAst*>(&statement)) return compileWhile(*node);
        if (const auto* node = dynamic_cast<const ForStatementAst*>(&statement)) return compileFor(*node);
        if (const auto* node = dynamic_cast<const ForEachStatementAst*>(&statement)) return compileForEach(*node);
        if (const auto* node = dynamic_cast<const RepStatementAst*>(&statement)) return compileRep(*node);
        if (const auto* node = dynamic_cast<const VariableDeclarationAst*>(&statement)) return compileVariable(*node);
        if (const auto* node = dynamic_cast<const AssignmentStatementAst*>(&statement)) return compileAssignment(*node);
        if (const auto* node = dynamic_cast<const ReturnStatementAst*>(&statement)) return compileReturn(*node);
        if (statement.kind == ProgramStatementKind::Break) return compileBreak(statement);
        if (statement.kind == ProgramStatementKind::Continue) return compileContinue(statement);
        if (const auto* node = dynamic_cast<const ExpressionStatementAst*>(&statement)) return compileExpression(*node);
    }

    void compileError(const ErrorStatementAst& node) {
        const std::vector<Token>& tokens = node.syntax.tokens;
        if (!tokens.empty() && tokens[0].kind == TokenKind::RightBrace) {
            recordSourceError(
                context.options.inputFile,
                node.syntax.lineNumber,
                node.syntax.startColumn,
                "unmatched closing brace",
                context.sourceLines
            );
            return;
        }
        if (!tokens.empty() && tokens[0].kind == TokenKind::Identifier) {
            const std::string& word = tokens[0].text;
            if (word == "else") {
                recordSourceError(context.options.inputFile, node.syntax.lineNumber, node.syntax.startColumn,
                    "else without matching if", context.sourceLines);
                return;
            }
            if (word == "nobreak") {
                recordSourceError(context.options.inputFile, node.syntax.lineNumber, node.syntax.startColumn,
                    "nobreak without matching loop", context.sourceLines);
                return;
            }
        }
        if (recordContextualSuggestion(context, node, node.syntax.opensBlock)) return;
        if (!requireSemicolon(node)) return;
        recordSourceError(context.options.inputFile, node.syntax.lineNumber, node.syntax.startColumn,
            "unsupported statement", context.sourceLines);
    }

    bool resolveTypeSyntax(const TypeSyntax& syntax, bool allowVoid, Type& result) {
        if (!syntax.syntaxOk) {
            Diagnostic diagnostic;
            diagnostic.message = syntax.syntaxError;
            diagnostic.labels.push_back({syntax.errorSpan.valid() ? syntax.errorSpan : syntax.nameSpan, "", true});
            recordDiagnostic(std::move(diagnostic));
            return false;
        }
        if (syntax.name == "bigint" || syntax.name == "Bigint" ||
            syntax.name == "bigfloat" || syntax.name == "BigFloat") {
            Diagnostic diagnostic;
            diagnostic.message = syntax.name + " has been removed from CP++; use int or float instead";
            diagnostic.labels.push_back({syntax.nameSpan, "", true});
            recordDiagnostic(std::move(diagnostic));
            return false;
        }
        Type base = declaredTypeForName(syntax.name);
        if (base == PrimitiveType::Unknown) {
            Diagnostic diagnostic;
            diagnostic.message = "unsupported type " + syntax.spelling;
            diagnostic.labels.push_back({syntax.nameSpan.valid() ? syntax.nameSpan : syntax.sourceSpan, "", true});
            recordDiagnostic(std::move(diagnostic));
            return false;
        }
        if (base == PrimitiveType::Void && !allowVoid) {
            Diagnostic diagnostic;
            diagnostic.message = "unsupported type void";
            diagnostic.labels.push_back({syntax.nameSpan.valid() ? syntax.nameSpan : syntax.sourceSpan, "", true});
            recordDiagnostic(std::move(diagnostic));
            return false;
        }

        const int expected = syntax.name == "string" || isStructType(base)
            ? 0
            : primitiveArity(base.primitive);
        if (static_cast<int>(syntax.arguments.size()) != expected) {
            std::string example;
            if (expected == 1) example = " like " + syntax.name + "<int>";
            if (expected == 2) example = " like " + syntax.name + "<int, int>";
            Diagnostic diagnostic;
            diagnostic.message = syntax.name + " expects " + std::to_string(expected) +
                " subtype" + (expected == 1 ? "" : "s") + example;
            diagnostic.labels.push_back({syntax.nameSpan.valid() ? syntax.nameSpan : syntax.sourceSpan, "", true});
            recordDiagnostic(std::move(diagnostic));
            return false;
        }

        for (const TypeSyntax& argument : syntax.arguments) {
            if (argument.name == "void") {
                Diagnostic diagnostic;
                diagnostic.message = "unsupported type " + typeSyntaxDisplay(syntax);
                diagnostic.labels.push_back({syntax.nameSpan.valid() ? syntax.nameSpan : syntax.sourceSpan, "", true});
                recordDiagnostic(std::move(diagnostic));
                return false;
            }
        }

        std::vector<Type> subtypes;
        for (const TypeSyntax& argument : syntax.arguments) {
            Type subtype;
            if (!resolveTypeSyntax(argument, false, subtype)) return false;
            subtypes.push_back(std::move(subtype));
        }
        if (!subtypes.empty()) base.subtypes = std::move(subtypes);

        if (!syntax.functionType) {
            result = std::move(base);
            return true;
        }

        std::vector<Type> parts = {base};
        for (const TypeSyntax& parameter : syntax.functionParameters) {
            Type parameterType;
            if (!resolveTypeSyntax(parameter, false, parameterType)) return false;
            parts.push_back(std::move(parameterType));
        }
        result = Type(PrimitiveType::Function, std::move(parts));
        result.functionParameterCopy = syntax.functionParameterCopy;
        return true;
    }

    bool buildResolvedDeclaration(
        const TypeSyntax& typeSyntax,
        bool inferred,
        const std::vector<std::string>& names,
        const std::vector<SourceSpan>& nameSpans,
        size_t continuationTokenIndex,
        const std::vector<Token>& tokens,
        ResolvedDeclarationSyntax& declaration
    ) {
        declaration.inferred = inferred;
        declaration.continuationTokenIndex = continuationTokenIndex;
        for (size_t index = 0; index < names.size(); ++index) {
            const SourceSpan span = index < nameSpans.size() ? nameSpans[index] : SourceSpan{};
            declaration.names.push_back({names[index], relativeColumnForSpan(tokens, span)});
        }
        return inferred || resolveTypeSyntax(typeSyntax, true, declaration.type);
    }

    bool buildFunctionHeader(
        const FunctionDeclarationAst& node,
        FunctionSignature& signature,
        std::string& generated,
        int& nameColumn
    ) {
        if (!resolveTypeSyntax(node.returnType, true, signature.returnType)) return false;
        signature.returnsVoid = signature.returnType == PrimitiveType::Void;
        signature.name = node.name;
        nameColumn = columnForSpan(node, node.nameSpan, node.syntax.startColumn);

        for (const ParameterSyntax& syntax : node.parameters) {
            Type parameterType;
            if (!resolveTypeSyntax(syntax.type, false, parameterType)) return false;
            const int parameterColumn = columnForSpan(node, syntax.sourceSpan, node.syntax.startColumn);
            if (syntax.modifier == "deep") {
                Diagnostic diagnostic;
                diagnostic.message = "deep parameter modifier has been replaced by copy";
                diagnostic.labels.push_back({syntax.modifierSpan, "", true});
                diagnostic.helps.push_back("replace `deep` with `copy`");
                recordDiagnostic(std::move(diagnostic));
                return false;
            }
            if (syntax.copyParameter && !copyParameterEligible(parameterType)) {
                Diagnostic diagnostic;
                diagnostic.message = "copy must precede a collection, string, or class parameter type";
                diagnostic.labels.push_back({syntax.modifierSpan, "", true});
                diagnostic.helps.push_back("remove `copy` to pass " + cpppTypeName(parameterType) + " normally");
                recordDiagnostic(std::move(diagnostic));
                return false;
            }
            signature.parameters.push_back({syntax.name, parameterType, syntax.copyParameter, parameterColumn});
        }

        const std::string returnType = signature.returnsVoid ? "void" : cppTypeForType(signature.returnType);
        if (returnType.empty()) {
            recordSourceError(context.options.inputFile, node.syntax.lineNumber, node.syntax.startColumn,
                "unsupported function return type " + cpppTypeName(signature.returnType), context.sourceLines);
            return false;
        }
        generated = returnType + " " + signature.name + "(";
        for (size_t index = 0; index < signature.parameters.size(); ++index) {
            if (index > 0) generated += ", ";
            const FunctionParameter& parameter = signature.parameters[index];
            const std::string parameterType = cppTypeForType(parameter.type);
            if (parameterType.empty()) {
                recordSourceError(context.options.inputFile, node.syntax.lineNumber, parameter.column,
                    "unsupported parameter type " + cpppTypeName(parameter.type), context.sourceLines);
                return false;
            }
            generated += parameterType + " " + parameter.name;
        }
        generated += ") {";
        return true;
    }

    void compileIf(const IfStatementAst& node) {
        if (!node.syntaxOk && !node.thenBody.hasClosingSyntax) sawUnclosedBlock = true;
        if (!node.condition) return;
        std::string condition = generateAnalyzedExpression(
            *node.condition, node.syntax.lineNumber, context.options.shouldRun, context.declaredFunctions);
        if (node.condition->inferredType != PrimitiveType::Bool) {
            condition = castExpressionTo(condition, node.condition->inferredType, PrimitiveType::Bool);
        }
        context.queueGeneratedLine(
            withComment(indentForDepth(context.blockDepth) + "if (" + condition + ") {", node.syntax),
            node.syntax.lineNumber
        );
        compileOwnedBlock(node.thenBody, "if");
        for (const ConditionalBranchAst& branch : node.elseIfBranches) {
            if (!branch.condition) return;
            std::string branchCondition = generateAnalyzedExpression(
                *branch.condition, branch.headerSyntax.lineNumber,
                context.options.shouldRun, context.declaredFunctions);
            if (branch.condition->inferredType != PrimitiveType::Bool) {
                branchCondition = castExpressionTo(
                    branchCondition, branch.condition->inferredType, PrimitiveType::Bool);
            }
            context.queueGeneratedLine(
                withComment(indentForDepth(context.blockDepth) + "else if (" + branchCondition + ") {", branch.headerSyntax),
                branch.headerSyntax.lineNumber
            );
            compileOwnedBlock(branch.body, "else if");
        }
        if (node.elseBranch) {
            context.queueGeneratedLine(
                withComment(indentForDepth(context.blockDepth) + "else {", node.elseBranch->headerSyntax),
                node.elseBranch->headerSyntax.lineNumber
            );
            compileOwnedBlock(node.elseBranch->body, "else");
        }
    }

    void compileWhile(const WhileStatementAst& node) {
        if (!node.syntaxOk && !node.body.hasClosingSyntax) sawUnclosedBlock = true;
        if (!node.condition) return;
        std::string condition = generateAnalyzedExpression(
            *node.condition, node.syntax.lineNumber, context.options.shouldRun, context.declaredFunctions);
        if (node.condition->inferredType != PrimitiveType::Bool) {
            condition = castExpressionTo(condition, node.condition->inferredType, PrimitiveType::Bool);
        }
        const std::string breakFlag = "__cppp_loop_completed_" + std::to_string(context.loopControlIndex++);
        context.queueGeneratedLine(indentForDepth(context.blockDepth) + "bool " + breakFlag + " = true;", node.syntax.lineNumber);
        context.queueGeneratedLine(
            withComment(indentForDepth(context.blockDepth) + "while (" + condition + ") {", node.syntax),
            node.syntax.lineNumber
        );
        compileOwnedBlock(node.body, "while", breakFlag);
        compileCompletion(node.nobreakBranch.get(), breakFlag);
    }

    bool emitForClause(
        const ForClauseAst& clause,
        int lineNumber,
        int statementColumn,
        bool allowDeclaration,
        std::string& generated
    ) {
        generated.clear();
        if (clause.kind == ForClauseKind::Empty || clause.tokens.empty()) return true;
        if (allowDeclaration && clause.kind == ForClauseKind::VariableDeclaration) {
            ResolvedDeclarationSyntax declaration;
            if (!buildResolvedDeclaration(
                    clause.type, clause.inferredType, clause.names, clause.nameSpans,
                    clause.continuationTokenIndex, clause.tokens, declaration)) return false;
            const TypeEmitResult result = emitResolvedTypeDeclaration(
                context.options.inputFile, lineNumber, statementColumn,
                context.sourceLines, context.declaredVariables, declaration, clause.tokens);
            if (!result.ok) return false;
            generated = stripGeneratedStatement(result.generatedStatement);
            return true;
        }
        if (clause.kind == ForClauseKind::Assignment) {
            const AssignmentEmitResult result = emitParsedAssignment(
                context.options.inputFile, lineNumber, statementColumn,
                context.sourceLines, context.declaredVariables, context.declaredFunctions,
                !context.options.shouldSubmit, clause.operation, clause.operationToken,
                clause.targetTokens, clause.targetOffsets,
                clause.valueTokens, clause.valueOffsets);
            if (!result.ok) return false;
            generated = stripGeneratedStatement(result.generatedStatement);
            return true;
        }
        const ExpressionEmitResult result = emitExpression(
            context.options.inputFile, lineNumber, clause.tokens, statementColumn,
            context.sourceLines, context.declaredVariables);
        if (!result.ok) return false;
        generated = result.generatedExpression;
        return true;
    }

    void compileFor(const ForStatementAst& node) {
        if (!node.syntaxOk) {
            if (!node.body.hasClosingSyntax) sawUnclosedBlock = true;
            recordSourceError(context.options.inputFile, node.syntax.lineNumber,
                node.syntax.startColumn + static_cast<int>(node.syntaxErrorOffset),
                node.syntaxError, context.sourceLines);
            return;
        }
        const std::string breakFlag = "__cppp_loop_completed_" + std::to_string(context.loopControlIndex++);
        std::set<std::string> declarationsBefore;
        for (const auto& variable : context.declaredVariables) declarationsBefore.insert(variable.first);

        std::string initializer;
        if (!emitForClause(node.initializer, node.syntax.lineNumber,
                node.syntax.startColumn + static_cast<int>(node.initializer.offset), true, initializer)) return;
        std::vector<std::string> loopNames;
        for (const auto& variable : context.declaredVariables) {
            if (declarationsBefore.count(variable.first) == 0) loopNames.push_back(variable.first);
        }

        std::string condition = "true";
        if (!node.conditionTokens.empty()) {
            const ExpressionEmitResult result = emitExpression(
                context.options.inputFile, node.syntax.lineNumber, node.conditionTokens,
                node.syntax.startColumn + static_cast<int>(node.conditionOffset),
                context.sourceLines, context.declaredVariables);
            if (!result.ok) {
                context.eraseDeclaredNames(loopNames);
                return;
            }
            if (!isImplicitlyConvertible(result.type, PrimitiveType::Bool)) {
                recordSourceError(context.options.inputFile, node.syntax.lineNumber,
                    node.syntax.startColumn + static_cast<int>(node.conditionOffset),
                    "for condition must be bool", context.sourceLines);
                context.eraseDeclaredNames(loopNames);
                return;
            }
            condition = result.type == PrimitiveType::Bool
                ? result.generatedExpression
                : castExpressionTo(result.generatedExpression, result.type, PrimitiveType::Bool);
        }

        std::string iteration;
        if (!emitForClause(node.iteration, node.syntax.lineNumber,
                node.syntax.startColumn + static_cast<int>(node.iteration.offset), false, iteration)) {
            context.eraseDeclaredNames(loopNames);
            return;
        }

        context.queueGeneratedLine(indentForDepth(context.blockDepth) + "bool " + breakFlag + " = true;", node.syntax.lineNumber);
        context.queueGeneratedLine(
            withComment(indentForDepth(context.blockDepth) + "for (" + initializer + "; " + condition + "; " + iteration + ") {", node.syntax),
            node.syntax.lineNumber
        );
        compileOwnedBlock(node.body, "for", breakFlag, loopNames);
        compileCompletion(node.nobreakBranch.get(), breakFlag);
    }

    void compileForEach(const ForEachStatementAst& node) {
        if (!node.syntaxOk) {
            if (!node.body.hasClosingSyntax) sawUnclosedBlock = true;
            recordSourceError(context.options.inputFile, node.syntax.lineNumber,
                node.syntax.startColumn + static_cast<int>(node.syntaxErrorOffset),
                node.syntaxError, context.sourceLines);
            return;
        }
        const std::string breakFlag = "__cppp_loop_completed_" + std::to_string(context.loopControlIndex++);
        const ExpressionEmitResult iterable = emitExpression(
            context.options.inputFile, node.syntax.lineNumber, node.iterableTokens,
            node.syntax.startColumn + static_cast<int>(node.iterableOffset),
            context.sourceLines, context.declaredVariables);
        if (!iterable.ok) return;

        std::string declaration;
        Type variableType;
        if (node.inferredVariable) {
            if (context.declaredVariables.count(node.variableName) != 0) {
                recordSourceError(context.options.inputFile, node.syntax.lineNumber,
                    node.syntax.startColumn + static_cast<int>(node.variableOffset),
                    "variable '" + node.variableName + "' is already declared", context.sourceLines);
                return;
            }
        } else {
            if (!resolveTypeSyntax(node.variableType, true, variableType)) return;
            if (variableType == PrimitiveType::Void) {
                recordSourceError(context.options.inputFile, node.syntax.lineNumber,
                    node.syntax.startColumn + static_cast<int>(node.variableOffset),
                    "variables cannot have void type", context.sourceLines);
                return;
            }
            if (context.declaredVariables.count(node.variableName) != 0) {
                recordSourceError(context.options.inputFile, node.syntax.lineNumber,
                    node.syntax.startColumn + static_cast<int>(node.variableOffset),
                    "variable '" + node.variableName + "' is already declared", context.sourceLines);
                return;
            }
            context.declaredVariables[node.variableName] = variableType;
            declaration = cppTypeForType(variableType) + " " + node.variableName;
            if (needsCharRuntimeHelperForType(variableType)) requireRuntimeHelper("CPPPCharType");
            if (needsRangeRuntimeHelperForType(variableType)) requireRuntimeHelper("CPPPRangeType");
        }

        if (!isListType(iterable.type) && !isSetType(iterable.type) &&
            !isMapType(iterable.type) && !isRangeType(iterable.type)) {
            recordSourceError(context.options.inputFile, node.syntax.lineNumber,
                node.syntax.startColumn + static_cast<int>(node.iterableOffset),
                "for-in expects a List value", context.sourceLines);
            if (!node.inferredVariable) context.declaredVariables.erase(node.variableName);
            return;
        }

        const Type elementType = isRangeType(iterable.type)
            ? Type(PrimitiveType::Int)
            : (isMapType(iterable.type)
                ? Type(PrimitiveType::Pair, {iterable.type.subtypes[0], iterable.type.subtypes[1]})
                : iterable.type.subtypes[0]);
        if (!isRangeType(iterable.type)) {
            requireContainerMember(iterable.type, "begin_mut");
            requireContainerMember(iterable.type, "end_mut");
        }
        if (isMapType(iterable.type)) {
            requireContainerMember(elementType, "ctor_std");
        }
        if (node.inferredVariable) {
            variableType = elementType;
            declaration = cppTypeForType(variableType) + " " + node.variableName;
            context.declaredVariables[node.variableName] = variableType;
            if (needsCharRuntimeHelperForType(variableType)) requireRuntimeHelper("CPPPCharType");
            if (needsRangeRuntimeHelperForType(variableType)) requireRuntimeHelper("CPPPRangeType");
        }
        if (!isImplicitlyConvertible(elementType, variableType)) {
            recordSourceError(context.options.inputFile, node.syntax.lineNumber,
                node.syntax.startColumn + static_cast<int>(node.variableOffset),
                "cannot implicitly convert " + cpppTypeName(elementType) + " to " + cpppTypeName(variableType),
                context.sourceLines);
            context.declaredVariables.erase(node.variableName);
            return;
        }

        context.queueGeneratedLine(indentForDepth(context.blockDepth) + "bool " + breakFlag + " = true;", node.syntax.lineNumber);
        context.queueGeneratedLine(
            withComment(indentForDepth(context.blockDepth) + "for (" + declaration + " : " + iterable.generatedExpression + ") {", node.syntax),
            node.syntax.lineNumber
        );
        compileOwnedBlock(node.body, "for", breakFlag, {node.variableName});
        compileCompletion(node.nobreakBranch.get(), breakFlag);
    }

    void compileRep(const RepStatementAst& node) {
        if (!node.syntaxOk && !node.body.hasClosingSyntax) sawUnclosedBlock = true;
        if (!node.count || !node.semanticValid) return;
        std::string count = generateAnalyzedExpression(
            *node.count,
            node.syntax.lineNumber,
            context.options.shouldRun,
            context.declaredFunctions
        );
        if (node.count->inferredType != PrimitiveType::Int) {
            count = castExpressionTo(count, node.count->inferredType, PrimitiveType::Int);
        }
        const std::string breakFlag = "__cppp_loop_completed_" + std::to_string(context.loopControlIndex++);
        context.queueGeneratedLine(indentForDepth(context.blockDepth) + "bool " + breakFlag + " = true;", node.syntax.lineNumber);
        const std::string suffix = std::to_string(context.repLoopIndex++);
        const std::string index = context.options.shouldSubmit ? "_" + suffix : "__cppp_rep_" + suffix;
        const std::string limit = context.options.shouldSubmit ? "_n" + suffix : "__cppp_rep_limit_" + suffix;
        context.queueGeneratedLine(
            indentForDepth(context.blockDepth) + "long long " + limit + " = " + count + ";",
            node.syntax.lineNumber
        );
        context.queueGeneratedLine(
            indentForDepth(context.blockDepth) + "if (" + limit + " < 0) throw runtime_error(\"" +
                std::to_string(node.syntax.lineNumber) + ":" +
                std::to_string(node.count->sourceColumn) + ":rep count cannot be negative\");",
            node.syntax.lineNumber
        );
        context.queueGeneratedLine(
            withComment(indentForDepth(context.blockDepth) + "for (long long " + index + " = 0; " +
                index + " < " + limit + "; ++" + index + ") {", node.syntax),
            node.syntax.lineNumber
        );
        compileOwnedBlock(node.body, "rep", breakFlag);
        compileCompletion(node.nobreakBranch.get(), breakFlag);
    }

    void compileCompletion(const CompletionBranchAst* branch, const std::string& breakFlag) {
        if (!branch) return;
        context.queueGeneratedLine(
            withComment(indentForDepth(context.blockDepth) + "if (" + breakFlag + ") {", branch->headerSyntax),
            branch->headerSyntax.lineNumber
        );
        compileOwnedBlock(branch->body, "loop nobreak");
    }

    void compileBreak(const ProgramStatement& node) {
        if (!requireSemicolon(node)) return;
        const std::string breakFlag = context.nearestLoopBreakFlag();
        if (breakFlag.empty()) {
            recordSourceError(context.options.inputFile, node.syntax.lineNumber, node.syntax.startColumn,
                "break can only be used inside a loop", context.sourceLines);
            return;
        }
        context.queueGeneratedLine(
            withComment(indentForDepth(context.blockDepth) + breakFlag + " = false; break;", node.syntax),
            node.syntax.lineNumber
        );
    }

    void compileContinue(const ProgramStatement& node) {
        if (!requireSemicolon(node)) return;
        if (context.nearestLoopBreakFlag().empty()) {
            recordSourceError(context.options.inputFile, node.syntax.lineNumber, node.syntax.startColumn,
                "continue can only be used inside a loop", context.sourceLines);
            return;
        }
        context.queueGeneratedLine(
            withComment(indentForDepth(context.blockDepth) + "continue;", node.syntax),
            node.syntax.lineNumber
        );
    }

    void compileReturn(const ReturnStatementAst& node) {
        if (!requireSemicolon(node)) return;
        const int line = node.syntax.lineNumber;
        if (!node.value) {
            context.queueGeneratedLine(withComment(indentForDepth(context.blockDepth) + "return;", node.syntax), line);
            return;
        }
        std::string generated = generateAnalyzedExpression(
            *node.value,
            line,
            context.options.shouldRun,
            context.declaredFunctions
        );
        if (node.hasValueConversion) {
            generated = castExpressionTo(generated, node.value->inferredType, node.expectedType);
        }
        context.queueGeneratedLine(
            withComment(indentForDepth(context.blockDepth) + "return " + generated + ";", node.syntax),
            line
        );
    }

    void compileVariable(const VariableDeclarationAst& node) {
        if (!requireSemicolon(node)) return;
        if (!node.syntaxOk) {
            recordSourceError(context.options.inputFile, node.syntax.lineNumber,
                node.syntax.startColumn + static_cast<int>(node.syntaxErrorOffset),
                node.syntaxError, context.sourceLines);
            return;
        }
        if (!node.inferredType && declaredTypeForName(node.type.name) == PrimitiveType::Unknown &&
            node.type.name != "bigint" && node.type.name != "Bigint" &&
            node.type.name != "bigfloat" && node.type.name != "BigFloat" &&
            recordContextualSuggestion(context, node, false)) return;
        prepareShadowedDeclaration(node.names);
        const bool directAnalyzedInitializer =
            node.semanticValid && !node.initializers.empty() &&
            node.initializerKind == VariableDeclarationAst::InitializerKind::Assignment &&
            node.initializers.size() == node.names.size() &&
            std::any_of(node.initializers.begin(), node.initializers.end(), [](const auto& value) {
                return containsContextualEmptyLiteral(value.get());
            }) &&
            std::none_of(node.initializers.begin(), node.initializers.end(), [](const auto& value) {
                const auto* call = dynamic_cast<const CallExpr*>(value.get());
                return call && !call->receiver && call->callee == "input";
            });
        if (directAnalyzedInitializer) {
            std::string generated;
            for (size_t i = 0; i < node.names.size(); ++i) {
                const Expr& initializer = *node.initializers[i];
                std::string value = generateAnalyzedExpression(
                    initializer,
                    node.syntax.lineNumber,
                    context.options.shouldRun,
                    context.declaredFunctions
                );
                const auto* nullLiteral = dynamic_cast<const LiteralExpr*>(&initializer);
                const bool nullValue = nullLiteral && nullLiteral->kind == LiteralExpr::Kind::Null;
                if (!nullValue && initializer.inferredType != node.resolvedType) {
                    value = castExpressionTo(value, initializer.inferredType, node.resolvedType);
                }
                if (i > 0) generated += " ";
                generated += cppTypeForType(node.resolvedType) + " " + node.names[i] + " = " + value + ";";
                context.declaredVariables[node.names[i]] = node.resolvedType;
                if (!context.blockDeclaredNames.empty()) {
                    context.blockDeclaredNames.back().push_back(node.names[i]);
                }
            }
            context.queueGeneratedLine(
                withComment(indentGeneratedStatement(generated, context.blockDepth), node.syntax),
                node.syntax.lineNumber
            );
            return;
        }

        const std::map<std::string, Type> before = context.declaredVariables;
        const std::vector<Token> tokens = withoutTrailingSemicolon(node.syntax.tokens);
        ResolvedDeclarationSyntax declaration;
        if (!buildResolvedDeclaration(
                node.type, node.inferredType, node.names, node.nameSpans,
                node.continuationTokenIndex, tokens, declaration)) return;
        const TypeEmitResult result = emitResolvedTypeDeclaration(
            context.options.inputFile,
            node.syntax.lineNumber,
            node.syntax.startColumn,
            context.sourceLines,
            context.declaredVariables,
            declaration,
            tokens
        );
        if (!context.blockDeclaredNames.empty()) {
            for (const auto& variable : context.declaredVariables) {
                if (before.count(variable.first) == 0) context.blockDeclaredNames.back().push_back(variable.first);
            }
        }
        if (!result.ok) return;
        context.queueGeneratedLine(
            withComment(indentGeneratedStatement(result.generatedStatement, context.blockDepth), node.syntax),
            node.syntax.lineNumber,
            result.sourceRanges
        );
    }

    void compileAssignment(const AssignmentStatementAst& node) {
        if (!requireSemicolon(node)) return;
        const AssignmentEmitResult result = emitParsedAssignment(
            context.options.inputFile,
            node.syntax.lineNumber,
            node.syntax.startColumn,
            context.sourceLines,
            context.declaredVariables,
            context.declaredFunctions,
            !context.options.shouldSubmit,
            node.operation,
            node.operationToken,
            node.targetTokens,
            node.targetOffsets,
            node.valueTokens,
            node.valueOffsets
        );
        if (!result.ok) return;
        context.queueGeneratedLine(
            withComment(indentGeneratedStatement(result.generatedStatement, context.blockDepth), node.syntax),
            node.syntax.lineNumber,
            result.sourceRanges
        );
    }

    void compileExpression(const ExpressionStatementAst& node) {
        if (!requireSemicolon(node)) return;
        const std::vector<Token> tokens = withoutTrailingSemicolon(node.syntax.tokens);
        const int line = node.syntax.lineNumber;
        const int column = node.syntax.startColumn;
        const bool printCall = isNamedCall(node.expression.get(), "print") ||
            (!tokens.empty() && tokens[0].kind == TokenKind::Identifier && tokens[0].text == "print");
        const bool describeCall = isNamedCall(node.expression.get(), "describe") ||
            (!tokens.empty() && tokens[0].kind == TokenKind::Identifier && tokens[0].text == "describe");

        const ListEmitResult list = emitListStatement(
            context.options.inputFile, line, context.sourceLines, context.declaredVariables,
            !context.options.shouldSubmit, tokens);
        if (list.matched) {
            if (list.ok) context.queueGeneratedLine(
                withComment(indentGeneratedStatement(list.generatedStatement, context.blockDepth), node.syntax),
                line, list.sourceRanges);
            return;
        }

        if (describeCall) {
            const PrintEmitResult result = emitDescribeStatement(
                context.options.inputFile, line, column, context.sourceLines,
                context.declaredVariables, tokens);
            if (result.ok) context.queueGeneratedLine(
                withComment(indentGeneratedStatement(result.generatedStatement, context.blockDepth), node.syntax),
                line, result.sourceRanges);
            return;
        }

        if (containsIncrementOrDecrement(node.expression.get()) &&
            !printCall && !describeCall) {
            const ExpressionEmitResult result = emitExpression(
                context.options.inputFile, line, tokens, column,
                context.sourceLines, context.declaredVariables);
            if (result.ok) context.queueGeneratedLine(
                withComment(indentForDepth(context.blockDepth) + result.generatedExpression + ";", node.syntax), line);
            return;
        }

        if (printCall) {
            const PrintEmitResult result = emitPrintStatement(
                context.options.inputFile, line, column, context.sourceLines,
                context.declaredVariables, tokens);
            if (result.ok) context.queueGeneratedLine(
                withComment(indentGeneratedStatement(result.generatedStatement, context.blockDepth), node.syntax),
                line, result.sourceRanges);
            return;
        }

        if (recordContextualSuggestion(context, node, false)) return;
        if (isUnsupportedBareCall(node.expression.get(), context)) {
            recordSourceError(context.options.inputFile, line, column, "unsupported statement", context.sourceLines);
            return;
        }

        const PrintEmitResult print = emitPrintStatement(
            context.options.inputFile, line, column, context.sourceLines,
            context.declaredVariables, tokens);
        if (print.ok) {
            context.queueGeneratedLine(
                withComment(indentGeneratedStatement(print.generatedStatement, context.blockDepth), node.syntax),
                line, print.sourceRanges);
            return;
        }

        const ExpressionEmitResult result = emitExpression(
            context.options.inputFile, line, tokens, column,
            context.sourceLines, context.declaredVariables);
        if (result.ok) context.queueGeneratedLine(
            withComment(indentForDepth(context.blockDepth) + result.generatedExpression + ";", node.syntax), line);
    }

    void compileFunction(const FunctionDeclarationAst& node, bool method) {
        if (!node.syntaxOk) {
            recordSourceError(context.options.inputFile, node.syntax.lineNumber,
                node.syntax.startColumn + static_cast<int>(node.syntaxErrorOffset),
                node.syntaxError, context.sourceLines);
            if (!node.body.hasClosingSyntax) sawUnclosedBlock = true;
            return;
        }
        FunctionSignature signature;
        std::string generatedSignature;
        int nameColumn = node.syntax.startColumn;
        if (!buildFunctionHeader(node, signature, generatedSignature, nameColumn)) return;

        if (method) {
            // Member signatures were registered and checked by semantic
            // analysis. Lowering only consumes that resolved table.
            const auto resolved = analyzed.aggregateMethods.find(context.currentStructName);
            if (resolved != analyzed.aggregateMethods.end()) {
                const auto found = resolved->second.find(signature.name);
                if (found != resolved->second.end()) signature = found->second;
            }
            context.currentStructMethodName = signature.name;
            context.queueTopLevelLine("    " + generatedSignature, node.syntax.lineNumber);
            context.savedDeclaredVariables = context.declaredVariables;
            context.declaredVariables = context.declaredStructs[context.currentStructName];
            context.declaredVariables["self"] = Type(PrimitiveType::Struct, context.currentStructName);
            for (const FunctionParameter& parameter : signature.parameters) {
                context.declaredVariables[parameter.name] = parameter.type;
            }
            context.currentFunction = signature;
            context.inFunction = true;
            context.outputTarget = OutputTarget::TopLevel;
            compileOwnedBlock(node.body, "function");
            context.inFunction = false;
            context.outputTarget = OutputTarget::TopLevel;
            context.declaredVariables = context.savedDeclaredVariables;
            context.savedDeclaredVariables.clear();
            context.currentFunction = FunctionSignature{};
            context.currentStructMethodName.clear();
            return;
        }

        if (context.declaredFunctions.count(signature.name) != 0) {
            recordSourceError(context.options.inputFile, node.syntax.lineNumber, nameColumn,
                "duplicate function '" + signature.name + "'", context.sourceLines);
            return;
        }
        context.declaredFunctions[signature.name] = signature;
        context.currentTopLevelFunctionName = signature.name;
        context.queueFunctionLine(
            withComment(generatedSignature, node.syntax),
            node.syntax.lineNumber
        );
        context.savedDeclaredVariables = context.declaredVariables;
        context.declaredVariables.clear();
        for (const FunctionParameter& parameter : signature.parameters) {
            context.declaredVariables[parameter.name] = parameter.type;
        }
        context.currentFunction = signature;
        context.inFunction = true;
        context.outputTarget = OutputTarget::Function;
        compileOwnedBlock(node.body, "function");
        context.inFunction = false;
        context.outputTarget = OutputTarget::Main;
        context.declaredVariables = context.savedDeclaredVariables;
        context.savedDeclaredVariables.clear();
        context.currentFunction = FunctionSignature{};
        context.currentTopLevelFunctionName.clear();
    }

    void compileField(const VariableDeclarationAst& node) {
        std::map<std::string, Type> fields;
        const std::vector<Token> tokens = withoutTrailingSemicolon(node.syntax.tokens);
        ResolvedDeclarationSyntax declaration;
        if (!buildResolvedDeclaration(
                node.type, node.inferredType, node.names, node.nameSpans,
                node.continuationTokenIndex, tokens, declaration)) return;
        const TypeEmitResult result = emitResolvedTypeDeclaration(
            context.options.inputFile,
            node.syntax.lineNumber,
            node.syntax.startColumn,
            context.sourceLines,
            fields,
            declaration,
            tokens
        );
        if (!result.ok) return;
        bool accepted = true;
        for (const auto& field : fields) {
            const auto resolvedAggregate = analyzed.aggregateFields.find(context.currentStructName);
            const auto resolvedField = resolvedAggregate == analyzed.aggregateFields.end()
                ? std::map<std::string, Type>::const_iterator{}
                : resolvedAggregate->second.find(field.first);
            if (resolvedAggregate == analyzed.aggregateFields.end() ||
                resolvedField == resolvedAggregate->second.end()) {
                accepted = false;
                continue;
            }
            context.currentStructFields.push_back(field.first);
        }
        if (accepted) context.queueTopLevelLine("    " + trim(result.generatedStatement), node.syntax.lineNumber);
    }

    void compileAggregate(const AggregateDeclarationAst& node) {
        context.currentStructIsClass = node.isClass;
        context.currentStructName = node.name;
        context.currentStructFields.clear();
        enterBlock("struct");
        context.queueTopLevelLine("struct " + node.name + " {", node.syntax.lineNumber);

        for (const auto& member : node.body.statements) {
            setRequirementOwner();
            if (const auto* method = dynamic_cast<const FunctionDeclarationAst*>(member.get())) {
                compileFunction(*method, true);
            } else if (const auto* field = dynamic_cast<const VariableDeclarationAst*>(member.get())) {
                if (checkLexicalErrors(*field)) compileField(*field);
            } else if (const auto* comment = dynamic_cast<const CommentStatementAst*>(member.get())) {
                if (!comment->syntax.commentText.empty()) {
                    context.queueGeneratedLine(indentForDepth(context.blockDepth) + comment->syntax.commentText,
                        comment->syntax.lineNumber);
                }
            } else {
                recordSourceError(context.options.inputFile, member->syntax.lineNumber, member->syntax.startColumn,
                    "struct bodies currently require typed fields", context.sourceLines);
            }
        }

        finishAggregate(node.body);
    }

    void finishAggregate(const BlockAst& body) {
        const int line = body.hasClosingSyntax ? body.closingSyntax.lineNumber : 0;
        if (!body.hasClosingSyntax) sawUnclosedBlock = true;
        if (!context.currentStructFields.empty()) {
            std::string constructor = "    " + context.currentStructName + "(";
            size_t index = 0;
            for (const std::string& field : context.declaredStructFieldOrders[context.currentStructName]) {
                if (index++ > 0) constructor += ", ";
                constructor += cppTypeForType(context.declaredStructs[context.currentStructName][field]) + " value_" + field;
            }
            constructor += ") : ";
            index = 0;
            for (const std::string& field : context.declaredStructFieldOrders[context.currentStructName]) {
                if (index++ > 0) constructor += ", ";
                constructor += field + "(std::move(value_" + field + "))";
            }
            constructor += " {}";
            context.queueTopLevelLine(constructor, line);
        }

        const auto& fields = context.declaredStructs[context.currentStructName];
        const auto& order = context.declaredStructFieldOrders[context.currentStructName];
        if (context.currentStructIsClass) {
            context.queueTopLevelLine("    " + context.currentStructName + "(const " + context.currentStructName + "& other) {", line);
            for (const std::string& field : order) {
                requireCopyHelpersForType(fields.at(field));
                context.queueTopLevelLine("        " + field + " = CPPPCopy(other." + field + ");", line);
            }
            context.queueTopLevelLine("    }", line);
            context.queueTopLevelLine("    " + context.currentStructName + "& operator=(const " + context.currentStructName + "& other) {", line);
            context.queueTopLevelLine("        if (this == &other) return *this;", line);
            for (const std::string& field : order) context.queueTopLevelLine("        " + field + " = other." + field + ";", line);
            context.queueTopLevelLine("        return *this;", line);
            context.queueTopLevelLine("    }", line);
        } else {
            context.queueTopLevelLine("    " + context.currentStructName + "() = default;", line);
        }
        std::string equal = "    bool operator==(const " + context.currentStructName + "& other) const { return ";
        if (fields.empty()) equal += "true";
        size_t fieldIndex = 0;
        for (const std::string& field : order) {
            const Type& type = fields.at(field);
            if (fieldIndex++ > 0) equal += " && ";
            if (isClassType(type)) {
                if (deferredClassEqualityFields.count(context.currentStructName + "." + field) != 0) {
                    equal += "CPPPClassEqual(" + field + ", other." + field + ")";
                } else {
                    equal += "((" + field + " && other." + field + ") ? (*" + field +
                        " == *other." + field + ") : (!" + field + " && !other." + field + "))";
                }
            } else {
                if (isCollectionType(type) || isPairType(type)) requireContainerMember(type, "compare_eq");
                if (isHeapType(type)) {
                    requireContainerMember(type, "to_list");
                    requireContainerMember(Type(PrimitiveType::List, type.subtypes), "compare_eq");
                }
                equal += field + " == other." + field;
            }
        }
        equal += "; }";
        context.queueTopLevelLine(equal, line);
        for (const std::string& field : order) requirePrintHelpersForType(fields.at(field));
        context.queueTopLevelLine("    friend ostream& operator<<(ostream& output, const " + context.currentStructName + "& value) {", line);
        context.queueTopLevelLine("        output << '{';", line);
        fieldIndex = 0;
        for (const std::string& field : order) {
            if (fieldIndex++ > 0) context.queueTopLevelLine("        output << \", \";", line);
            context.queueTopLevelLine("        output << \"" + field + ": \"; CPPPPrintValue(output, value." + field + ");", line);
        }
        context.queueTopLevelLine("        return output << '}';", line);
        context.queueTopLevelLine("    }", line);
        context.queueTopLevelLine("};", line);
        if (!context.currentStructIsClass) {
            for (const std::string& field : order) requireCopyHelpersForType(fields.at(field));
            std::string copier = "template <> struct CPPPDeepCopier<" + context.currentStructName + "> { static " + context.currentStructName + " run(const " + context.currentStructName + "& value) { return " + context.currentStructName + "(";
            for (size_t index = 0; index < order.size(); ++index) {
                if (index > 0) copier += ", ";
                copier += "CPPPCopy(value." + order[index] + ")";
            }
            copier += "); } };";
            context.queueTopLevelLine(copier, line);
        }

        const std::vector<std::string> names = context.blockDeclaredNames.back();
        context.eraseDeclaredNames(names);
        context.blockKinds.pop_back();
        context.blockBreakFlags.pop_back();
        context.blockDeclaredNames.pop_back();
        context.blockShadowedVariables.pop_back();
        --context.blockDepth;
        context.currentStructIsClass = false;
        context.currentStructName.clear();
        context.currentStructFields.clear();
        context.outputTarget = OutputTarget::Main;
    }
};
}

void compileProgramAst(CompileContext& context, const AnalyzedProgramAst& analyzed) {
    if (!analyzed.program || !analyzed.valid) return;
    context.declaredStructs = analyzed.aggregateFields;
    context.declaredStructFieldOrders = analyzed.aggregateFieldOrder;
    context.declaredStructMethods = analyzed.aggregateMethods;
    context.declaredClassNames = analyzed.classNames;
    setDeclaredStructsForExpressions(&context.declaredStructs);
    setDeclaredClassNamesForExpressions(&context.declaredClassNames);
    setDeclaredStructFieldOrdersForExpressions(&context.declaredStructFieldOrders);
    setDeclaredStructMethodsForExpressions(&context.declaredStructMethods);
    AstLowerer(context, analyzed).compile(*analyzed.program);
}
