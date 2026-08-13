/*
 * Resolves names and types on ProgramAst before C++ lowering. The AST remains
 * the ownership tree; semantic fields on its nodes are the analyzed view.
 */

#include "semanticAnalyzer.h"

#include "errors.h"
#include "expressionAnalyzer.h"
#include "expressions.h"
#include "keywords.h"
#include "tokenizer.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {
struct Symbol {
    Type type;
    std::string kind;
    SourceSpan span;
};

bool copyEligible(const Type& type) {
    return isStringType(type) || isCollectionType(type) || isClassType(type);
}

int sizedListDepth(const Type& type) {
    if (!isListType(type)) return 0;
    return 1 + sizedListDepth(type.subtypes[0]);
}

class SemanticAnalyzer {
public:
    SemanticAnalyzer(CompileContext& context, ProgramAst& program) :
        context(context), program(program) {}

    AnalyzedProgramAst run() {
        result.program = &program;
        registerAggregateNames();
        setSemanticTables();
        resolveAggregateMembers();
        detectStructCyclesAndOrder();
        pushScope();
        analyzeBlock(program.body, false);
        popScope();
        result.valid = !hasRecordedSourceErrors();
        return result;
    }

private:
    CompileContext& context;
    ProgramAst& program;
    AnalyzedProgramAst result;
    std::vector<std::map<std::string, Symbol>> scopes;
    std::map<std::string, SourceSpan> typeNameSpans;
    std::map<std::string, std::pair<int, int>> typeDeclarationSites;
    std::map<std::string, int> futureFunctionLines;
    std::map<std::string, int> futureVariableLines;
    FunctionSignature currentFunction;
    bool inFunction = false;
    int loopDepth = 0;
    std::string currentAggregate;
    std::set<std::string> poisonedNames;

    void setSemanticTables() {
        setDeclaredStructsForExpressions(&result.aggregateFields);
        setDeclaredClassNamesForExpressions(&result.classNames);
        setDeclaredStructFieldOrdersForExpressions(&result.aggregateFieldOrder);
        setDeclaredStructMethodsForExpressions(&result.aggregateMethods);
        setDeclaredStructConstructorsForExpressions(&result.aggregateConstructors);
    }

    void error(int line, int column, const std::string& message) {
        recordSourceError(
            context.options.inputFile,
            line,
            std::max(1, column),
            message,
            context.sourceLines
        );
    }

    void errorWithNameSuggestion(
        int line,
        int column,
        SourceSpan span,
        const std::string& message,
        const std::string& misspelled,
        const std::vector<std::string>& candidates
    ) {
        Diagnostic diagnostic;
        diagnostic.message = message;
        diagnostic.labels.push_back({
            span.valid()
                ? span
                : sourceTokenSpan(context.options.inputFile, context.sourceLines, line, column),
            "",
            true
        });
        const std::string closest = closestDiagnosticCandidate(misspelled, candidates);
        if (!closest.empty()) {
            diagnostic.suggestions.push_back({
                diagnostic.labels.front().span,
                closest,
                "did you mean '" + closest + "'?",
                SuggestionApplicability::MaybeIncorrect
            });
        }
        recordDiagnostic(std::move(diagnostic));
    }

    int columnForOffset(size_t offset, int fallback) const {
        size_t lineStart = 0;
        for (const auto& sourceLine : context.sourceLines) {
            const size_t lineEnd = lineStart + sourceLine.second.size();
            if (offset <= lineEnd) {
                return static_cast<int>(offset - lineStart + 1);
            }
            lineStart = lineEnd + 1;
        }
        return fallback;
    }

    int columnForSpan(SourceSpan span, int fallback) const {
        return span.valid() ? columnForOffset(span.startOffset, fallback) : fallback;
    }

    void pushScope() { scopes.emplace_back(); }
    void popScope() { scopes.pop_back(); }

    std::map<std::string, Type> visibleVariables() const {
        std::map<std::string, Type> visible;
        for (const auto& scope : scopes) {
            for (const auto& item : scope) visible[item.first] = item.second.type;
        }
        return visible;
    }

    const Symbol* lookup(const std::string& name) const {
        for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
            const auto found = scope->find(name);
            if (found != scope->end()) return &found->second;
        }
        return nullptr;
    }

    bool declareValue(
        const std::string& name,
        Type type,
        const std::string& kind,
        SourceSpan span,
        int line,
        int column
    ) {
        if (isReservedKeyword(name)) {
            error(line, columnForSpan(span, column),
                "reserved keyword '" + name + "' cannot be used as a " + kind + " name");
            poisonedNames.insert(name);
            return false;
        }
        if (scopes.back().count(name) != 0 || result.functions.count(name) != 0 ||
            result.aggregateFields.count(name) != 0) {
            error(line, column, kind + " '" + name + "' conflicts with an existing symbol in this scope");
            poisonedNames.insert(name);
            return false;
        }
        scopes.back()[name] = {std::move(type), kind, span};
        return true;
    }

    Type resolveType(TypeSyntax& syntax, bool allowVoid, int line, int column) {
        if (syntax.hasResolvedType) return syntax.resolvedType;
        if (!syntax.syntaxOk) {
            error(line, column, syntax.syntaxError.empty() ? "invalid type syntax" : syntax.syntaxError);
            return PrimitiveType::Unknown;
        }
        if (syntax.name == "bigint" || syntax.name == "Bigint" ||
            syntax.name == "bigfloat" || syntax.name == "BigFloat") {
            error(line, column, syntax.name + " has been removed from CP++; use int or float instead");
            return PrimitiveType::Unknown;
        }
        Type base = declaredTypeForName(syntax.name);
        if (base == PrimitiveType::Unknown) {
            std::vector<std::string> candidates = {
                "bool", "char", "int", "float", "string", "range",
                "List", "Stack", "Queue", "Deque", "Heap", "Set", "Map", "Pair"
            };
            if (allowVoid) candidates.push_back("void");
            for (const auto& aggregate : result.aggregateFields) {
                candidates.push_back(aggregate.first);
            }
            errorWithNameSuggestion(
                line,
                columnForSpan(syntax.nameSpan, column),
                syntax.nameSpan,
                "unknown type '" + syntax.name + "'",
                syntax.name,
                candidates
            );
            return PrimitiveType::Unknown;
        }
        if (base == PrimitiveType::Void && !allowVoid && !syntax.functionType) {
            error(line, column, "void is not valid in this type position");
            return PrimitiveType::Unknown;
        }
        const int expected = syntax.name == "string" || isStructType(base)
            ? 0 : primitiveArity(base.primitive);
        if (static_cast<int>(syntax.arguments.size()) != expected) {
            error(line, column, syntax.name + " expects " + std::to_string(expected) +
                " subtype" + (expected == 1 ? "" : "s"));
            return PrimitiveType::Unknown;
        }
        std::vector<Type> subtypes;
        bool valid = true;
        for (TypeSyntax& argument : syntax.arguments) {
            Type subtype = resolveType(argument, false, line, column);
            valid = valid && subtype != PrimitiveType::Unknown;
            subtypes.push_back(std::move(subtype));
        }
        if (!valid) return PrimitiveType::Unknown;
        if (!subtypes.empty()) base.subtypes = std::move(subtypes);
        if (syntax.functionType) {
            std::vector<Type> parts = {base};
            for (TypeSyntax& parameter : syntax.functionParameters) {
                Type parameterType = resolveType(parameter, false, line, column);
                if (parameterType == PrimitiveType::Unknown) return PrimitiveType::Unknown;
                parts.push_back(std::move(parameterType));
            }
            base = Type(PrimitiveType::Function, std::move(parts));
            base.functionParameterCopy = syntax.functionParameterCopy;
        }
        syntax.resolvedType = base;
        syntax.hasResolvedType = true;
        return base;
    }

    void registerAggregateNames() {
        for (const auto& statement : program.body.statements) {
            const auto* aggregate = dynamic_cast<const AggregateDeclarationAst*>(statement.get());
            const auto* function = dynamic_cast<const FunctionDeclarationAst*>(statement.get());
            const auto* variable = dynamic_cast<const VariableDeclarationAst*>(statement.get());
            if (function) futureFunctionLines.emplace(function->name, function->syntax.lineNumber);
            if (variable) {
                for (const std::string& name : variable->names) {
                    futureVariableLines.emplace(name, variable->syntax.lineNumber);
                }
            }
            if (!aggregate) continue;
            const std::string& name = aggregate->name;
            if (isReservedKeyword(name)) {
                error(aggregate->syntax.lineNumber,
                    columnForSpan(aggregate->nameSpan, aggregate->syntax.startColumn),
                    std::string(aggregate->isClass ? "class" : "struct") +
                    " name '" + name + "' cannot be a reserved keyword");
                continue;
            }
            if (declaredTypeForName(name) != PrimitiveType::Unknown ||
                result.aggregateFields.count(name) != 0) {
                error(aggregate->syntax.lineNumber, aggregate->syntax.startColumn,
                    std::string(aggregate->isClass ? "class" : "struct") +
                    " '" + name + "' is already declared");
                continue;
            }
            result.aggregateFields[name] = {};
            result.aggregateFieldOrder[name] = {};
            result.aggregateMethods[name] = {};
            if (aggregate->isClass) result.classNames.insert(name);
            typeNameSpans[name] = aggregate->nameSpan;
            typeDeclarationSites[name] = {
                aggregate->syntax.lineNumber,
                aggregate->syntax.startColumn
            };
        }
    }

    bool buildSignature(FunctionDeclarationAst& function, FunctionSignature& signature) {
        const int line = function.syntax.lineNumber;
        const int column = function.syntax.startColumn;
        signature.name = function.name;
        signature.returnType = resolveType(function.returnType, true, line, column);
        if (signature.returnType == PrimitiveType::Unknown) return false;
        signature.returnsVoid = signature.returnType == PrimitiveType::Void;
        std::set<std::string> parameters;
        bool valid = true;
        std::vector<Type> functionParts = {signature.returnType};
        std::vector<bool> copyModes;
        for (ParameterSyntax& parameter : function.parameters) {
            Type type = resolveType(parameter.type, false, line, column);
            if (type == PrimitiveType::Unknown) { valid = false; continue; }
            if (isReservedKeyword(parameter.name)) {
                error(line, columnForOffset(parameter.sourceSpan.endOffset - parameter.name.size(), column),
                    "reserved keyword '" + parameter.name + "' cannot be used as a parameter name");
                valid = false;
                continue;
            }
            if (!parameters.insert(parameter.name).second) {
                error(line, column, "duplicate parameter '" + parameter.name + "' in " + function.name + "()");
                valid = false;
                continue;
            }
            if (parameter.modifier == "deep") {
                error(line, column, "deep parameter modifier has been replaced by copy");
                valid = false;
            }
            if (parameter.copyParameter && !copyEligible(type)) {
                error(line, column, "copy parameter '" + parameter.name +
                    "' must have a collection, string, or class type; got " + cpppTypeName(type));
                valid = false;
            }
            signature.parameters.push_back({parameter.name, type, parameter.copyParameter, column});
            functionParts.push_back(type);
            copyModes.push_back(parameter.copyParameter);
        }
        function.resolvedFunctionType = Type(PrimitiveType::Function, std::move(functionParts));
        function.resolvedFunctionType.functionParameterCopy = std::move(copyModes);
        return valid;
    }

    bool buildConstructorSignature(
        ConstructorDeclarationAst& constructor,
        const std::string& aggregateName,
        FunctionSignature& signature
    ) {
        const int line = constructor.syntax.lineNumber;
        const int column = constructor.syntax.startColumn;
        signature.name = aggregateName;
        signature.returnType = PrimitiveType::Void;
        signature.returnsVoid = true;
        std::set<std::string> parameters;
        bool valid = true;
        std::vector<Type> functionParts = {Type(PrimitiveType::Void)};
        std::vector<bool> copyModes;
        for (ParameterSyntax& parameter : constructor.parameters) {
            Type type = resolveType(parameter.type, false, line, column);
            if (type == PrimitiveType::Unknown) { valid = false; continue; }
            if (isReservedKeyword(parameter.name)) {
                error(line, columnForOffset(parameter.sourceSpan.endOffset - parameter.name.size(), column),
                    "reserved keyword '" + parameter.name + "' cannot be used as a parameter name");
                valid = false;
                continue;
            }
            if (!parameters.insert(parameter.name).second) {
                error(line, column, "duplicate parameter '" + parameter.name + "' in " + aggregateName + "()");
                valid = false;
                continue;
            }
            if (parameter.modifier == "deep") {
                error(line, column, "deep parameter modifier has been replaced by copy");
                valid = false;
            }
            if (parameter.copyParameter && !copyEligible(type)) {
                error(line, column, "copy parameter '" + parameter.name +
                    "' must have a collection, string, or class type; got " + cpppTypeName(type));
                valid = false;
            }
            signature.parameters.push_back({parameter.name, type, parameter.copyParameter, column});
            functionParts.push_back(type);
            copyModes.push_back(parameter.copyParameter);
        }
        constructor.resolvedFunctionType = Type(PrimitiveType::Function, std::move(functionParts));
        constructor.resolvedFunctionType.functionParameterCopy = std::move(copyModes);
        return valid;
    }

    void resolveAggregateMembers() {
        for (const auto& statement : program.body.statements) {
            auto* aggregate = dynamic_cast<AggregateDeclarationAst*>(statement.get());
            if (!aggregate || result.aggregateFields.count(aggregate->name) == 0) continue;
            aggregate->resolvedType = Type(
                aggregate->isClass ? PrimitiveType::Class : PrimitiveType::Struct,
                aggregate->name
            );
            std::set<std::string> memberNames;
            for (const auto& member : aggregate->body.statements) {
                if (auto* field = dynamic_cast<VariableDeclarationAst*>(member.get())) {
                    if (field->inferredType) {
                        error(field->syntax.lineNumber, field->syntax.startColumn,
                            "aggregate fields require an explicit type");
                        continue;
                    }
                    Type type = resolveType(field->type, false, field->syntax.lineNumber,
                        field->syntax.startColumn);
                    field->resolvedType = type;
                    for (size_t index = 0; index < field->names.size(); ++index) {
                        const std::string& name = field->names[index];
                        if (isReservedKeyword(name)) {
                            const SourceSpan span = index < field->nameSpans.size()
                                ? field->nameSpans[index] : SourceSpan{};
                            error(field->syntax.lineNumber,
                                columnForSpan(span, field->syntax.startColumn),
                                "reserved keyword '" + name + "' cannot be used as a field name");
                            continue;
                        }
                        if (!memberNames.insert(name).second) {
                            error(field->syntax.lineNumber, field->syntax.startColumn,
                                "duplicate aggregate member '" + name + "'");
                            continue;
                        }
                        result.aggregateFields[aggregate->name][name] = type;
                        result.aggregateFieldOrder[aggregate->name].push_back(name);
                    }
                } else if (auto* method = dynamic_cast<FunctionDeclarationAst*>(member.get())) {
                    if (isReservedKeyword(method->name)) {
                        error(method->syntax.lineNumber,
                            columnForSpan(method->nameSpan, method->syntax.startColumn),
                            "reserved keyword '" + method->name + "' cannot be used as a method name");
                        continue;
                    }
                    if (!memberNames.insert(method->name).second) {
                        error(method->syntax.lineNumber, method->syntax.startColumn,
                            "aggregate member '" + method->name + "' conflicts with an existing field or method");
                        continue;
                    }
                    FunctionSignature signature;
                    if (buildSignature(*method, signature)) {
                        result.aggregateMethods[aggregate->name][method->name] = std::move(signature);
                    }
                } else if (auto* constructor = dynamic_cast<ConstructorDeclarationAst*>(member.get())) {
                    if (result.aggregateConstructors.count(aggregate->name) != 0) {
                        error(constructor->syntax.lineNumber, constructor->syntax.startColumn,
                            "class '" + aggregate->name + "' has more than one constructor");
                        continue;
                    }
                    FunctionSignature signature;
                    if (buildConstructorSignature(*constructor, aggregate->name, signature)) {
                        result.aggregateConstructors[aggregate->name] = std::move(signature);
                    }
                } else if (!dynamic_cast<CommentStatementAst*>(member.get()) &&
                           !dynamic_cast<ErrorStatementAst*>(member.get())) {
                    error(member->syntax.lineNumber, member->syntax.startColumn,
                        "aggregate bodies may contain only typed fields and methods");
                }
            }
        }
    }

    void detectStructCyclesAndOrder() {
        std::map<std::string, std::set<std::string>> dependencies;
        for (const auto& aggregate : result.aggregateFields) {
            for (const auto& field : aggregate.second) {
                // Only a directly embedded struct needs a complete definition.
                // Reference-backed containers (including List<Self>) and class
                // links break the inline C++ layout dependency.
                if (isInlineStructType(field.second)) {
                    dependencies[aggregate.first].insert(field.second.name);
                }
            }
        }
        std::map<std::string, int> state;
        std::vector<std::string> stack;
        std::function<void(const std::string&)> visit = [&](const std::string& name) {
            if (state[name] == 2) return;
            if (state[name] == 1) {
                auto start = std::find(stack.begin(), stack.end(), name);
                std::string cycle;
                for (auto item = start; item != stack.end(); ++item) {
                    if (!cycle.empty()) cycle += " -> ";
                    cycle += *item;
                }
                if (!cycle.empty()) cycle += " -> ";
                cycle += name;
                const auto site = typeDeclarationSites.find(name);
                error(site == typeDeclarationSites.end() ? 1 : site->second.first,
                    site == typeDeclarationSites.end() ? 1 : site->second.second,
                    "recursive struct value cycle: " + cycle +
                    "; use a class link to break the inline value cycle");
                return;
            }
            state[name] = 1;
            stack.push_back(name);
            for (const std::string& dependency : dependencies[name]) visit(dependency);
            stack.pop_back();
            state[name] = 2;
            result.aggregateEmissionOrder.push_back(name);
        };
        // Start from source order so independent declarations keep their
        // historical generated-C++ order. DFS moves only actual dependencies.
        for (const auto& statement : program.body.statements) {
            const auto* aggregate = dynamic_cast<const AggregateDeclarationAst*>(statement.get());
            if (aggregate && result.aggregateFields.count(aggregate->name) != 0) {
                visit(aggregate->name);
            }
        }
    }

    bool analyzeExpected(Expr& expression, const Type& expected, int line,
                         const std::string& conversionContext = "") {
        if (auto* input = dynamic_cast<CallExpr*>(&expression);
            input && !input->receiver && input->callee == "input") {
            input->inferredType = expected;
            input->resolvedCallable = "builtin:input(target=" + cpppTypeName(expected) + ")";
            input->semanticAnalyzed = true;
            input->semanticValid = expected != PrimitiveType::Unknown;
            for (auto& argument : input->arguments) analyzeExpression(*argument, line);
            return input->semanticValid;
        }
        if (auto* literal = dynamic_cast<LiteralExpr*>(&expression);
            literal && literal->kind == LiteralExpr::Kind::Null && isClassType(expected)) {
            literal->inferredType = PrimitiveType::Unknown;
            literal->hasImplicitConversion = true;
            literal->implicitConversionTarget = expected;
            literal->semanticAnalyzed = true;
            literal->semanticValid = true;
            return true;
        }
        if (auto* list = dynamic_cast<ListLiteralExpr*>(&expression);
            list && isListType(expected)) {
            bool valid = true;
            for (auto& element : list->elements) {
                valid = analyzeExpected(*element, expected.subtypes[0], line,
                    " in list literal") && valid;
            }
            list->inferredType = expected;
            list->semanticAnalyzed = true;
            list->semanticValid = valid;
            return valid;
        }
        if (auto* set = dynamic_cast<SetLiteralExpr*>(&expression);
            set && isSetType(expected)) {
            bool valid = true;
            for (auto& element : set->elements) {
                valid = analyzeExpected(*element, expected.subtypes[0], line,
                    " in set literal") && valid;
            }
            set->inferredType = expected;
            set->semanticAnalyzed = true;
            set->semanticValid = valid;
            return valid;
        }
        if (auto* map = dynamic_cast<MapLiteralExpr*>(&expression);
            map && isMapType(expected)) {
            bool valid = true;
            for (auto& entry : map->entries) {
                valid = analyzeExpected(*entry.key, expected.subtypes[0], line,
                    " in map literal key") && valid;
                valid = analyzeExpected(*entry.value, expected.subtypes[1], line,
                    " in map literal value") && valid;
            }
            map->inferredType = expected;
            map->semanticAnalyzed = true;
            map->semanticValid = valid;
            return valid;
        }
        if (auto* pair = dynamic_cast<PairLiteralExpr*>(&expression);
            pair && isPairType(expected)) {
            const bool first = analyzeExpected(*pair->first, expected.subtypes[0], line);
            const bool second = analyzeExpected(*pair->second, expected.subtypes[1], line);
            pair->inferredType = expected;
            pair->semanticAnalyzed = true;
            pair->semanticValid = first && second;
            return pair->semanticValid;
        }
        if (!analyzeExpression(expression, line)) return false;
        if (!expression.explicitCast && !isImplicitlyConvertible(expression.inferredType, expected)) {
            error(line, expression.sourceColumn, "cannot implicitly convert " +
                cpppTypeName(expression.inferredType) + " to " + cpppTypeName(expected) +
                conversionContext);
            expression.semanticValid = false;
            return false;
        }
        if (expression.inferredType != expected) {
            expression.hasImplicitConversion = true;
            expression.implicitConversionTarget = expected;
        }
        return true;
    }

    void primeCallArguments(CallExpr& call, int line) {
        if (call.receiver) analyzeExpression(*call.receiver, line);

        std::vector<Type> expected;
        const auto function = result.functions.find(call.callee);
        if (!call.receiver && function != result.functions.end()) {
            for (const FunctionParameter& parameter : function->second.parameters) {
                expected.push_back(parameter.type);
            }
        } else if (!call.receiver && result.aggregateConstructors.count(call.callee) != 0) {
            for (const FunctionParameter& parameter : result.aggregateConstructors[call.callee].parameters) {
                expected.push_back(parameter.type);
            }
        } else if (!call.receiver && result.aggregateFields.count(call.callee) != 0) {
            for (const std::string& field : result.aggregateFieldOrder[call.callee]) {
                expected.push_back(result.aggregateFields[call.callee].at(field));
            }
        } else if (call.receiver && isStructType(call.receiver->inferredType)) {
            const auto aggregate = result.aggregateMethods.find(call.receiver->inferredType.name);
            if (aggregate != result.aggregateMethods.end()) {
                const auto method = aggregate->second.find(call.callee);
                if (method != aggregate->second.end()) {
                    for (const FunctionParameter& parameter : method->second.parameters) {
                        expected.push_back(parameter.type);
                    }
                }
            }
        }

        for (size_t i = 0; i < call.arguments.size(); ++i) {
            if (i < expected.size()) analyzeExpected(*call.arguments[i], expected[i], line);
            else analyzeExpression(*call.arguments[i], line);
        }
    }

    bool analyzeExpression(Expr& expression, int line) {
        if (expression.semanticAnalyzed) return expression.semanticValid;
        if (auto* recovered = dynamic_cast<ErrorExpr*>(&expression)) {
            bool reportedUnknownToken = false;
            const auto sourceLine = context.sourceLines.find(line);
            if (recovered->sourceSpan.valid() && sourceLine != context.sourceLines.end()) {
                const int startColumn = std::max(1, recovered->sourceColumn);
                const size_t start = std::min(
                    sourceLine->second.size(), static_cast<size_t>(startColumn - 1));
                for (const Token& token : tokenize(sourceLine->second.substr(start))) {
                    if (token.kind != TokenKind::Unknown) continue;
                    error(line, startColumn + token.span.startColumn - 1,
                        "unrecognized token '" + token.text + "'");
                    reportedUnknownToken = true;
                    break;
                }
            }
            if (!reportedUnknownToken) {
                error(line, std::max(1, recovered->sourceColumn),
                    recovered->reason == "expression syntax recovery"
                        ? (recovered->sourceSpan.valid()
                            ? "unexpected token in expression"
                            : "expected expression")
                        : recovered->reason);
            }
            recovered->inferredType = PrimitiveType::Unknown;
            recovered->semanticAnalyzed = true;
            recovered->semanticValid = false;
            return false;
        }
        if (auto* call = dynamic_cast<CallExpr*>(&expression)) primeCallArguments(*call, line);
        return analyzeExpressionAst(
            expression,
            context.options.inputFile,
            line,
            context.sourceLines,
            visibleVariables(),
            result.functions,
            &futureVariableLines
        );
    }

    bool analyzeComparator(Expr& expression, const Type& itemType, int line) {
        const Type comparatorType(
            PrimitiveType::Function, {PrimitiveType::Bool, itemType, itemType});
        if (auto* compare = dynamic_cast<CallExpr*>(&expression);
            compare && !compare->receiver && compare->callee == "compare") {
            bool valid = compare->arguments.size() == 1;
            if (!valid) {
                error(line, compare->sourceColumn, "compare() expects an index or field name");
            }
            for (auto& argument : compare->arguments) {
                valid = analyzeExpression(*argument, line) && valid;
            }
            if (valid) {
                const auto* selector = dynamic_cast<const LiteralExpr*>(compare->arguments[0].get());
                if (!selector || (selector->kind != LiteralExpr::Kind::Int &&
                    selector->kind != LiteralExpr::Kind::String)) {
                    error(line, compare->arguments[0]->sourceColumn,
                        "compare() expects a literal index or field name");
                    valid = false;
                } else if (selector->kind == LiteralExpr::Kind::Int) {
                    if (isPairType(itemType)) {
                        int index = -1;
                        try { index = std::stoi(selector->text); } catch (...) { index = -1; }
                        if (index < 0 || index > 1) {
                            error(line, selector->sourceColumn,
                                "compare(index) requires index 0 or 1 for Pair values");
                            valid = false;
                        }
                    } else if (!isListType(itemType)) {
                        error(line, selector->sourceColumn,
                            "compare(index) can only order Lists or Pairs");
                        valid = false;
                    }
                } else if (!isStructType(itemType)) {
                    error(line, selector->sourceColumn,
                        "compare(field) can only order structs or classes");
                    valid = false;
                } else {
                    std::string field = selector->text;
                    if (field.size() >= 2) field = field.substr(1, field.size() - 2);
                    if (result.aggregateFields[itemType.name].count(field) == 0) {
                        error(line, selector->sourceColumn,
                            std::string(isClassType(itemType) ? "class " : "struct ") +
                            itemType.name + " has no field '" + field + "'");
                        valid = false;
                    }
                }
            }
            compare->inferredType = comparatorType;
            compare->functionType = comparatorType;
            compare->resolvedCallable = "builtin:compare";
            compare->semanticAnalyzed = true;
            compare->semanticValid = valid;
            return valid;
        }
        if (auto* builtin = dynamic_cast<VariableExpr*>(&expression);
            builtin && (builtin->name == "greater" || builtin->name == "default")) {
            builtin->inferredType = comparatorType;
            builtin->resolvedSymbol = "builtin:" + builtin->name;
            builtin->semanticAnalyzed = true;
            builtin->semanticValid = true;
            return true;
        }
        return analyzeExpected(expression, comparatorType, line);
    }

    bool analyzeSpecialStatementCall(CallExpr& call, int line) {
        bool valid = !call.receiver || analyzeExpression(*call.receiver, line);
        if (call.receiver && valid && isStructType(call.receiver->inferredType)) {
            return analyzeExpression(call, line);
        }
        if (call.callee == "sort" && call.receiver && valid) {
            if (!isListType(call.receiver->inferredType)) {
                error(line, call.sourceColumn, "sort() can only be used on List values");
                valid = false;
            } else if (call.arguments.size() > 1) {
                error(line, call.sourceColumn, "sort() expects zero arguments or one comparator");
                valid = false;
            } else if (!call.arguments.empty()) {
                valid = analyzeComparator(*call.arguments[0], call.receiver->inferredType.subtypes[0], line) && valid;
            }
        } else if (call.callee == "reverse" && call.receiver && valid) {
            if (!isListType(call.receiver->inferredType) || !call.arguments.empty()) {
                error(line, call.sourceColumn, "reverse() expects a List receiver and no arguments");
                valid = false;
            }
        } else if (call.callee == "add" && call.receiver && valid) {
            const bool listArity = isListType(call.receiver->inferredType) &&
                (call.arguments.size() == 1 || call.arguments.size() == 2);
            const bool otherArity =
                (isSetType(call.receiver->inferredType) ||
                 isLinearDataStructureType(call.receiver->inferredType)) &&
                call.arguments.size() == 1;
            if (!listArity && !otherArity) {
                error(line, call.sourceColumn,
                    "add() expects a value and optional List index");
                valid = false;
                for (auto& argument : call.arguments) valid = analyzeExpression(*argument, line) && valid;
            } else {
                valid = analyzeExpected(*call.arguments[0], call.receiver->inferredType.subtypes[0], line) && valid;
                if (call.arguments.size() == 2) {
                    valid = analyzeExpected(*call.arguments[1], PrimitiveType::Int, line) && valid;
                }
            }
        } else {
            for (auto& argument : call.arguments) {
                if (call.callee == "print") {
                    if (auto* builtin = dynamic_cast<VariableExpr*>(argument.get());
                        builtin && builtin->name == "flush") {
                        builtin->inferredType = declaredTypeForName("string");
                        builtin->resolvedSymbol = "builtin:flush";
                        builtin->semanticAnalyzed = true;
                        builtin->semanticValid = true;
                        continue;
                    }
                }
                valid = analyzeExpression(*argument, line) && valid;
            }
        }
        call.inferredType = PrimitiveType::Void;
        call.mutableValue = false;
        call.resolvedCallable = "builtin-or-container:" + call.callee;
        call.semanticAnalyzed = true;
        call.semanticValid = valid;
        return valid;
    }

    bool analyzeCondition(Expr* condition, int line, const std::string& owner) {
        if (!condition) { error(line, 1, "expected condition"); return false; }
        if (auto* recovered = dynamic_cast<ErrorExpr*>(condition);
            recovered && !recovered->sourceSpan.valid()) {
            error(line, std::max(1, recovered->sourceColumn), "expected condition");
            recovered->inferredType = PrimitiveType::Unknown;
            recovered->semanticAnalyzed = true;
            recovered->semanticValid = false;
            return false;
        }
        if (!analyzeExpression(*condition, line)) return false;
        if (!isImplicitlyConvertible(condition->inferredType, PrimitiveType::Bool)) {
            error(line, condition->sourceColumn, owner + " condition must be bool");
            return false;
        }
        if (condition->inferredType != PrimitiveType::Bool) {
            condition->hasImplicitConversion = true;
            condition->implicitConversionTarget = PrimitiveType::Bool;
        }
        return true;
    }

    bool blockAlwaysReturns(const BlockAst& block) const {
        for (const auto& statement : block.statements) {
            if (dynamic_cast<const ReturnStatementAst*>(statement.get())) return true;
            if (const auto* branch = dynamic_cast<const IfStatementAst*>(statement.get())) {
                if (!branch->elseBranch || !blockAlwaysReturns(branch->thenBody) ||
                    !blockAlwaysReturns(branch->elseBranch->body)) continue;
                bool all = true;
                for (const auto& elseIf : branch->elseIfBranches) {
                    all = all && blockAlwaysReturns(elseIf.body);
                }
                if (all) return true;
            }
        }
        return false;
    }

    void analyzeFunctionBody(FunctionDeclarationAst& function, const FunctionSignature& signature, bool method) {
        const FunctionSignature savedFunction = currentFunction;
        const bool savedInFunction = inFunction;
        currentFunction = signature;
        inFunction = true;
        pushScope();
        if (method) {
            // Inside a method, self denotes the already-dereferenced receiver.
            const Type selfType(PrimitiveType::Struct, currentAggregate);
            scopes.back()["self"] = {selfType, "self", function.nameSpan};
            for (const auto& field : result.aggregateFields[currentAggregate]) {
                scopes.back()[field.first] = {field.second, "field", {}};
            }
        }
        for (const auto& parameter : signature.parameters) {
            if (scopes.back().count(parameter.name) != 0) {
                error(function.syntax.lineNumber, parameter.column,
                    "parameter '" + parameter.name + "' conflicts with an existing method symbol");
                continue;
            }
            scopes.back()[parameter.name] = {parameter.type, "parameter", {}};
        }
        analyzeBlock(function.body, true);
        if (!signature.returnsVoid && !blockAlwaysReturns(function.body)) {
            error(function.syntax.lineNumber, function.syntax.startColumn,
                "non-void function '" + function.name + "' does not return " +
                cpppTypeName(signature.returnType) + " on every path");
        }
        popScope();
        currentFunction = savedFunction;
        inFunction = savedInFunction;
    }

    void analyzeConstructorBody(ConstructorDeclarationAst& constructor, const FunctionSignature& signature) {
        const FunctionSignature savedFunction = currentFunction;
        const bool savedInFunction = inFunction;
        currentFunction = signature;
        inFunction = true;
        pushScope();
        const Type selfType(PrimitiveType::Struct, currentAggregate);
        scopes.back()["self"] = {selfType, "self", constructor.nameSpan};
        for (const auto& field : result.aggregateFields[currentAggregate]) {
            scopes.back()[field.first] = {field.second, "field", {}};
        }
        for (const FunctionParameter& parameter : signature.parameters) {
            if (scopes.back().count(parameter.name) != 0) {
                error(constructor.syntax.lineNumber, parameter.column,
                    "parameter '" + parameter.name + "' conflicts with an existing constructor symbol");
                continue;
            }
            scopes.back()[parameter.name] = {parameter.type, "parameter", {}};
        }
        analyzeBlock(constructor.body, true);
        popScope();
        currentFunction = savedFunction;
        inFunction = savedInFunction;
    }

    void analyzeVariable(VariableDeclarationAst& variable) {
        const int line = variable.syntax.lineNumber;
        bool valid = true;
        Type type;
        if (!variable.inferredType) {
            type = resolveType(variable.type, false, line, variable.syntax.startColumn);
            valid = type != PrimitiveType::Unknown;
        }
        if (variable.inferredType) {
            if (variable.initializers.empty()) {
                error(line, variable.syntax.startColumn, "var requires an initializer");
                valid = false;
            } else if (analyzeExpression(*variable.initializers.front(), line)) {
                type = variable.initializers.front()->inferredType;
            } else {
                valid = false;
            }
        } else if (variable.initializerKind == VariableDeclarationAst::InitializerKind::Parenthesized &&
                   isListType(type)) {
            if (variable.names.size() != 1) {
                error(line, variable.syntax.startColumn,
                    "a List size initializer can declare only one variable");
                valid = false;
            }
            if (variable.initializers.empty()) {
                error(line, variable.syntax.startColumn,
                    "List size initializer requires at least one size");
                valid = false;
            }
            if (static_cast<int>(variable.initializers.size()) > sizedListDepth(type)) {
                error(line, variable.syntax.startColumn,
                    "List size initializer has more sizes than its List/string depth");
                valid = false;
            }
            for (auto& initializer : variable.initializers) {
                const bool sizeValid = analyzeExpression(*initializer, line);
                valid = sizeValid && valid;
                if (sizeValid && initializer->inferredType != PrimitiveType::Int) {
                    error(line, initializer->sourceColumn,
                        "List size initializer requires int sizes, got " +
                        cpppTypeName(initializer->inferredType));
                    valid = false;
                }
            }
        } else if (variable.initializerKind == VariableDeclarationAst::InitializerKind::Parenthesized &&
                   (isSetType(type) || isMapType(type) || isHeapType(type))) {
            // Comparator syntax is represented as expression children by the
            // syntax AST. Its collection-specific validation is performed by
            // the specialized semantic call path below and the compatibility
            // comparator adapter during lowering.
            for (auto& initializer : variable.initializers) {
                valid = analyzeComparator(*initializer, type.subtypes[0], line) && valid;
            }
        } else {
            for (auto& initializer : variable.initializers) {
                valid = analyzeExpected(*initializer, type, line) && valid;
                variable.initializerConversionTargets.push_back(type);
            }
        }
        variable.resolvedType = type;
        for (size_t i = 0; i < variable.names.size(); ++i) {
            const SourceSpan span = i < variable.nameSpans.size() ? variable.nameSpans[i] : SourceSpan{};
            valid = declareValue(variable.names[i], type, "variable", span, line,
                variable.syntax.startColumn) && valid;
        }
        variable.semanticAnalyzed = true;
        variable.semanticValid = valid;
    }

    bool analyzeFieldInitializers(VariableDeclarationAst& field) {
        bool valid = field.resolvedType != PrimitiveType::Unknown;
        const int line = field.syntax.lineNumber;
        const Type& type = field.resolvedType;
        if (field.initializerKind == VariableDeclarationAst::InitializerKind::Parenthesized &&
            isListType(type)) {
            if (static_cast<int>(field.initializers.size()) > sizedListDepth(type)) valid = false;
            for (auto& initializer : field.initializers) {
                const bool sizeValid = analyzeExpression(*initializer, line);
                valid = sizeValid && valid;
                if (sizeValid && initializer->inferredType != PrimitiveType::Int) valid = false;
            }
        } else if (field.initializerKind == VariableDeclarationAst::InitializerKind::Parenthesized &&
                   (isSetType(type) || isMapType(type) || isHeapType(type))) {
            for (auto& initializer : field.initializers) {
                valid = analyzeComparator(*initializer, type.subtypes[0], line) && valid;
            }
        } else {
            for (auto& initializer : field.initializers) {
                valid = analyzeExpected(*initializer, type, line) && valid;
            }
        }
        return valid;
    }

    void analyzeAssignment(AssignmentStatementAst& assignment) {
        const int line = assignment.syntax.lineNumber;
        bool valid = true;
        for (auto& target : assignment.targets) {
            const bool targetValid = analyzeExpression(*target, line);
            if (targetValid && !target->mutableValue) {
                error(line, target->sourceColumn, "assignment target must be mutable");
                valid = false;
            }
            assignment.resolvedTargetTypes.push_back(target->inferredType);
            valid = targetValid && valid;
        }
        if (assignment.targets.size() > 1 && assignment.values.size() != assignment.targets.size() &&
            !(assignment.values.size() == 1 && dynamic_cast<CallExpr*>(assignment.values[0].get()) &&
              static_cast<CallExpr*>(assignment.values[0].get())->callee == "input")) {
            error(line, assignment.syntax.startColumn,
                "multi-assignment requires the same number of values as targets");
            valid = false;
        }
        for (size_t i = 0; i < assignment.values.size(); ++i) {
            const Type expected = assignment.resolvedTargetTypes.empty()
                ? Type(PrimitiveType::Unknown)
                : assignment.resolvedTargetTypes[std::min(i, assignment.resolvedTargetTypes.size() - 1)];
            const bool valueValid = expected == PrimitiveType::Unknown
                ? analyzeExpression(*assignment.values[i], line)
                : analyzeExpected(*assignment.values[i], expected, line);
            valid = valueValid && valid;
            assignment.valueConversionTargets.push_back(expected);
        }
        assignment.semanticAnalyzed = true;
        assignment.semanticValid = valid;
    }

    void analyzeReturn(ReturnStatementAst& statement) {
        const int line = statement.syntax.lineNumber;
        bool valid = true;
        if (!inFunction) {
            error(line, statement.syntax.startColumn, "return can only be used inside a function");
            valid = false;
        } else if (!statement.value && !currentFunction.returnsVoid) {
            error(line, statement.syntax.startColumn, "non-void function must return a value of type " +
                cpppTypeName(currentFunction.returnType));
            valid = false;
        } else if (statement.value && currentFunction.returnsVoid) {
            error(line, statement.value->sourceColumn, "void function cannot return a value");
            analyzeExpression(*statement.value, line);
            valid = false;
        } else if (statement.value) {
            statement.expectedType = currentFunction.returnType;
            valid = analyzeExpected(*statement.value, currentFunction.returnType, line);
            statement.hasValueConversion = statement.value->inferredType != currentFunction.returnType;
        }
        statement.semanticAnalyzed = true;
        statement.semanticValid = valid;
    }

    void analyzeFunction(FunctionDeclarationAst& function, bool method) {
        FunctionSignature signature;
        bool valid = buildSignature(function, signature);
        if (!method) {
            if (isReservedKeyword(function.name)) {
                error(function.syntax.lineNumber,
                    columnForSpan(function.nameSpan, function.syntax.startColumn),
                    "reserved keyword '" + function.name + "' cannot be used as a function name");
                valid = false;
            } else if (lookup(function.name) != nullptr || result.functions.count(function.name) != 0 ||
                result.aggregateFields.count(function.name) != 0) {
                error(function.syntax.lineNumber, function.syntax.startColumn,
                    "function '" + function.name + "' conflicts with an existing symbol in this scope");
                valid = false;
            } else {
                result.functions[function.name] = signature;
            }
        }
        if (valid) analyzeFunctionBody(function, signature, method);
        function.semanticAnalyzed = true;
        function.semanticValid = valid;
    }

    void analyzeAggregate(AggregateDeclarationAst& aggregate) {
        const std::string saved = currentAggregate;
        currentAggregate = aggregate.name;
        bool valid = result.aggregateFields.count(aggregate.name) != 0;
        for (const auto& member : aggregate.body.statements) {
            if (auto* method = dynamic_cast<FunctionDeclarationAst*>(member.get())) {
                const auto found = result.aggregateMethods[aggregate.name].find(method->name);
                if (found != result.aggregateMethods[aggregate.name].end()) {
                    analyzeFunctionBody(*method, found->second, true);
                    method->semanticAnalyzed = true;
                    method->semanticValid = true;
                }
            } else if (auto* constructor = dynamic_cast<ConstructorDeclarationAst*>(member.get())) {
                const auto found = result.aggregateConstructors.find(aggregate.name);
                if (found != result.aggregateConstructors.end()) {
                    analyzeConstructorBody(*constructor, found->second);
                    constructor->semanticAnalyzed = true;
                    constructor->semanticValid = true;
                } else {
                    constructor->semanticAnalyzed = true;
                    constructor->semanticValid = false;
                }
            } else if (auto* field = dynamic_cast<VariableDeclarationAst*>(member.get())) {
                field->semanticAnalyzed = true;
                field->semanticValid = analyzeFieldInitializers(*field);
            } else if (auto* comment = dynamic_cast<CommentStatementAst*>(member.get())) {
                comment->semanticAnalyzed = true;
                comment->semanticValid = true;
            } else if (auto* errorNode = dynamic_cast<ErrorStatementAst*>(member.get())) {
                error(errorNode->syntax.lineNumber, errorNode->syntax.startColumn, errorNode->reason);
                if (errorNode->recoveredBody) analyzeBlock(*errorNode->recoveredBody, false);
                errorNode->semanticAnalyzed = true;
                errorNode->semanticValid = false;
            }
        }
        aggregate.semanticAnalyzed = true;
        aggregate.semanticValid = valid;
        currentAggregate = saved;
    }

    void analyzeBlock(BlockAst& block, bool existingScope) {
        if (!existingScope) pushScope();
        for (auto& statement : block.statements) analyzeStatement(*statement);
        if (!existingScope) popScope();
    }

    void analyzeStatement(ProgramStatement& statement) {
        if (!statement.syntaxOk) {
            error(statement.syntax.lineNumber,
                statement.syntax.startColumn + static_cast<int>(statement.syntaxErrorOffset),
                statement.syntaxError.empty() ? "invalid statement syntax" : statement.syntaxError);
        }
        if (auto* errorNode = dynamic_cast<ErrorStatementAst*>(&statement)) {
            error(errorNode->syntax.lineNumber, errorNode->syntax.startColumn, errorNode->reason);
            if (errorNode->recoveredBody) analyzeBlock(*errorNode->recoveredBody, false);
            errorNode->semanticAnalyzed = true;
            errorNode->semanticValid = false;
        } else if (auto* variable = dynamic_cast<VariableDeclarationAst*>(&statement)) {
            analyzeVariable(*variable);
        } else if (auto* assignment = dynamic_cast<AssignmentStatementAst*>(&statement)) {
            analyzeAssignment(*assignment);
        } else if (auto* expressionStatement = dynamic_cast<ExpressionStatementAst*>(&statement)) {
            bool valid = false;
            if (auto* call = dynamic_cast<CallExpr*>(expressionStatement->expression.get());
                call && (call->callee == "print" || call->callee == "describe" ||
                    call->callee == "add" || call->callee == "sort" || call->callee == "reverse")) {
                valid = analyzeSpecialStatementCall(*call, statement.syntax.lineNumber);
            } else if (expressionStatement->expression) {
                valid = analyzeExpression(*expressionStatement->expression, statement.syntax.lineNumber);
            }
            statement.semanticAnalyzed = true;
            statement.semanticValid = valid;
        } else if (auto* returned = dynamic_cast<ReturnStatementAst*>(&statement)) {
            analyzeReturn(*returned);
        } else if (statement.kind == ProgramStatementKind::Break ||
                   statement.kind == ProgramStatementKind::Continue) {
            const bool valid = loopDepth > 0;
            if (!valid) error(statement.syntax.lineNumber, statement.syntax.startColumn,
                std::string(statement.kind == ProgramStatementKind::Break ? "break" : "continue") +
                " can only be used inside a loop");
            statement.semanticAnalyzed = true;
            statement.semanticValid = valid;
        } else if (auto* branch = dynamic_cast<IfStatementAst*>(&statement)) {
            bool valid = analyzeCondition(branch->condition.get(), branch->syntax.lineNumber, "if");
            analyzeBlock(branch->thenBody, false);
            for (auto& elseIf : branch->elseIfBranches) {
                valid = analyzeCondition(elseIf.condition.get(), elseIf.headerSyntax.lineNumber, "else if") && valid;
                analyzeBlock(elseIf.body, false);
            }
            if (branch->elseBranch) analyzeBlock(branch->elseBranch->body, false);
            branch->semanticAnalyzed = true;
            branch->semanticValid = valid;
        } else if (auto* loop = dynamic_cast<WhileStatementAst*>(&statement)) {
            const bool valid = analyzeCondition(loop->condition.get(), loop->syntax.lineNumber, "while");
            ++loopDepth; analyzeBlock(loop->body, false); --loopDepth;
            if (loop->nobreakBranch) analyzeBlock(loop->nobreakBranch->body, false);
            loop->semanticAnalyzed = true; loop->semanticValid = valid;
        } else if (auto* loop = dynamic_cast<RepStatementAst*>(&statement)) {
            bool valid = false;
            if (loop->count) {
                if (auto* recovered = dynamic_cast<ErrorExpr*>(loop->count.get());
                    recovered && !recovered->sourceSpan.valid()) {
                    error(loop->syntax.lineNumber, std::max(1, recovered->sourceColumn),
                        "expected rep count");
                    recovered->inferredType = PrimitiveType::Unknown;
                    recovered->semanticAnalyzed = true;
                    recovered->semanticValid = false;
                } else {
                    valid = analyzeExpression(*loop->count, loop->syntax.lineNumber);
                }
            } else {
                error(loop->syntax.lineNumber, loop->syntax.startColumn, "expected rep count");
            }
            if (valid && loop->count->inferredType != PrimitiveType::Bool &&
                loop->count->inferredType != PrimitiveType::Char &&
                loop->count->inferredType != PrimitiveType::Int &&
                loop->count->inferredType != PrimitiveType::Float) {
                error(loop->syntax.lineNumber, loop->count->sourceColumn, "rep count must be numeric");
                valid = false;
            }
            if (loop->count) loop->resolvedCountType = loop->count->inferredType;
            ++loopDepth; analyzeBlock(loop->body, false); --loopDepth;
            if (loop->nobreakBranch) analyzeBlock(loop->nobreakBranch->body, false);
            loop->semanticAnalyzed = true; loop->semanticValid = valid;
        } else if (auto* loop = dynamic_cast<ForEachStatementAst*>(&statement)) {
            bool valid = loop->iterable && analyzeExpression(*loop->iterable, loop->syntax.lineNumber);
            Type element = PrimitiveType::Unknown;
            if (valid && isRangeType(loop->iterable->inferredType)) element = PrimitiveType::Int;
            else if (valid && isMapType(loop->iterable->inferredType)) element = Type(PrimitiveType::Pair,
                {loop->iterable->inferredType.subtypes[0], loop->iterable->inferredType.subtypes[1]});
            else if (valid && (isListType(loop->iterable->inferredType) || isSetType(loop->iterable->inferredType)))
                element = loop->iterable->inferredType.subtypes[0];
            else if (valid) {
                error(loop->syntax.lineNumber, loop->iterable->sourceColumn,
                    "for-in expects List, Set, Map, or range; convert this value with List(value)");
                valid = false;
            }
            Type variableType = loop->inferredVariable ? element :
                resolveType(loop->variableType, false, loop->syntax.lineNumber, loop->syntax.startColumn);
            loop->resolvedVariableType = variableType;
            if (valid && !isImplicitlyConvertible(element, variableType)) {
                error(loop->syntax.lineNumber, loop->syntax.startColumn, "cannot implicitly convert " +
                    cpppTypeName(element) + " to foreach variable type " + cpppTypeName(variableType));
                valid = false;
            }
            ++loopDepth; pushScope();
            SourceSpan variableSpan = loop->syntax.sourceSpan;
            variableSpan.startOffset = loop->variableOffset;
            variableSpan.endOffset = loop->variableOffset + loop->variableName.size();
            declareValue(loop->variableName, variableType, "loop variable", variableSpan,
                loop->syntax.lineNumber, loop->syntax.startColumn);
            analyzeBlock(loop->body, true); popScope(); --loopDepth;
            if (loop->nobreakBranch) analyzeBlock(loop->nobreakBranch->body, false);
            loop->semanticAnalyzed = true; loop->semanticValid = valid;
        } else if (auto* loop = dynamic_cast<ForStatementAst*>(&statement)) {
            pushScope(); ++loopDepth;
            bool valid = true;
            if (loop->initializer.kind == ForClauseKind::VariableDeclaration) {
                Type type = loop->initializer.inferredType ? Type(PrimitiveType::Unknown) :
                    resolveType(loop->initializer.type, false, loop->syntax.lineNumber, loop->syntax.startColumn);
                for (auto& expression : loop->initializer.expressions) {
                    if (loop->initializer.inferredType && analyzeExpression(*expression, loop->syntax.lineNumber))
                        type = expression->inferredType;
                    else valid = analyzeExpected(*expression, type, loop->syntax.lineNumber) && valid;
                }
                for (size_t index = 0; index < loop->initializer.names.size(); ++index) {
                    const SourceSpan span = index < loop->initializer.nameSpans.size()
                        ? loop->initializer.nameSpans[index] : SourceSpan{};
                    valid = declareValue(loop->initializer.names[index], type, "loop variable", span,
                        loop->syntax.lineNumber, loop->syntax.startColumn) && valid;
                }
            } else {
                for (auto& expression : loop->initializer.expressions)
                    valid = analyzeExpression(*expression, loop->syntax.lineNumber) && valid;
            }
            if (loop->condition) valid = analyzeCondition(loop->condition.get(), loop->syntax.lineNumber, "for") && valid;
            for (auto& expression : loop->iteration.expressions)
                valid = analyzeExpression(*expression, loop->syntax.lineNumber) && valid;
            analyzeBlock(loop->body, true);
            --loopDepth; popScope();
            if (loop->nobreakBranch) analyzeBlock(loop->nobreakBranch->body, false);
            loop->semanticAnalyzed = true; loop->semanticValid = valid;
        } else if (auto* function = dynamic_cast<FunctionDeclarationAst*>(&statement)) {
            analyzeFunction(*function, false);
        } else if (auto* aggregate = dynamic_cast<AggregateDeclarationAst*>(&statement)) {
            analyzeAggregate(*aggregate);
        } else {
            statement.semanticAnalyzed = true;
            statement.semanticValid = true;
        }
    }
};

bool validateExpr(const Expr* expression, std::string& error) {
    if (!expression) return true;
    if (!expression->semanticAnalyzed) { error = "expression was not semantically analyzed"; return false; }
    if (expression->semanticValid && expression->inferredType == PrimitiveType::Unknown &&
        !dynamic_cast<const LiteralExpr*>(expression)) {
        error = "valid expression has unknown type";
        return false;
    }
    if (const auto* node = dynamic_cast<const VariableExpr*>(expression);
        node && node->semanticValid && node->resolvedSymbol.empty()) {
        error = "valid identifier has no resolved symbol";
        return false;
    }
    if (const auto* node = dynamic_cast<const FieldExpr*>(expression);
        node && node->semanticValid && node->resolvedSymbol.empty()) {
        error = "valid field access has no resolved field";
        return false;
    }
    if (const auto* node = dynamic_cast<const CallExpr*>(expression);
        node && node->semanticValid && node->resolvedCallable.empty()) {
        error = "valid call has no resolved callable";
        return false;
    }
    const auto child = [&](const Expr* value) { return validateExpr(value, error); };
    if (const auto* node = dynamic_cast<const FieldExpr*>(expression)) return child(node->base.get());
    if (const auto* node = dynamic_cast<const UnaryExpr*>(expression)) return child(node->operand.get());
    if (const auto* node = dynamic_cast<const BinaryExpr*>(expression)) return child(node->left.get()) && child(node->right.get());
    if (const auto* node = dynamic_cast<const CastExpr*>(expression)) return child(node->operand.get());
    if (const auto* node = dynamic_cast<const CallExpr*>(expression)) {
        if (node->receiver && !child(node->receiver.get())) return false;
        for (const auto& argument : node->arguments) if (!child(argument.get())) return false;
    }
    if (const auto* node = dynamic_cast<const IndexExpr*>(expression)) return child(node->base.get()) && child(node->index.get());
    if (const auto* node = dynamic_cast<const SliceExpr*>(expression))
        return child(node->base.get()) && child(node->start.get()) && child(node->end.get());
    if (const auto* node = dynamic_cast<const ListLiteralExpr*>(expression))
        for (const auto& item : node->elements) if (!child(item.get())) return false;
    if (const auto* node = dynamic_cast<const SetLiteralExpr*>(expression))
        for (const auto& item : node->elements) if (!child(item.get())) return false;
    if (const auto* node = dynamic_cast<const MapLiteralExpr*>(expression))
        for (const auto& item : node->entries) if (!child(item.key.get()) || !child(item.value.get())) return false;
    if (const auto* node = dynamic_cast<const PairLiteralExpr*>(expression))
        return child(node->first.get()) && child(node->second.get());
    return true;
}

bool validateBlock(const BlockAst& block, std::string& error) {
    for (const auto& statement : block.statements) {
        if (!statement->semanticAnalyzed) { error = "statement was not semantically analyzed"; return false; }
        if (const auto* node = dynamic_cast<const VariableDeclarationAst*>(statement.get()))
            for (const auto& value : node->initializers) if (!validateExpr(value.get(), error)) return false;
        if (const auto* node = dynamic_cast<const AssignmentStatementAst*>(statement.get())) {
            for (const auto& value : node->targets) if (!validateExpr(value.get(), error)) return false;
            for (const auto& value : node->values) if (!validateExpr(value.get(), error)) return false;
        }
        if (const auto* node = dynamic_cast<const ExpressionStatementAst*>(statement.get()))
            if (!validateExpr(node->expression.get(), error)) return false;
        if (const auto* node = dynamic_cast<const ReturnStatementAst*>(statement.get()))
            if (!validateExpr(node->value.get(), error)) return false;
        if (const auto* node = dynamic_cast<const IfStatementAst*>(statement.get())) {
            if (!validateExpr(node->condition.get(), error) || !validateBlock(node->thenBody, error)) return false;
            for (const auto& branch : node->elseIfBranches)
                if (!validateExpr(branch.condition.get(), error) || !validateBlock(branch.body, error)) return false;
            if (node->elseBranch && !validateBlock(node->elseBranch->body, error)) return false;
        }
        if (const auto* node = dynamic_cast<const WhileStatementAst*>(statement.get())) {
            if (!validateExpr(node->condition.get(), error) || !validateBlock(node->body, error)) return false;
            if (node->nobreakBranch && !validateBlock(node->nobreakBranch->body, error)) return false;
        }
        if (const auto* node = dynamic_cast<const ForEachStatementAst*>(statement.get())) {
            if (!validateExpr(node->iterable.get(), error) || !validateBlock(node->body, error)) return false;
            if (node->nobreakBranch && !validateBlock(node->nobreakBranch->body, error)) return false;
        }
        if (const auto* node = dynamic_cast<const RepStatementAst*>(statement.get())) {
            if (!validateExpr(node->count.get(), error) || !validateBlock(node->body, error)) return false;
            if (node->nobreakBranch && !validateBlock(node->nobreakBranch->body, error)) return false;
        }
        if (const auto* node = dynamic_cast<const ForStatementAst*>(statement.get())) {
            for (const auto& value : node->initializer.expressions)
                if (!validateExpr(value.get(), error)) return false;
            if (!validateExpr(node->condition.get(), error)) return false;
            for (const auto& value : node->iteration.expressions)
                if (!validateExpr(value.get(), error)) return false;
            if (!validateBlock(node->body, error)) return false;
            if (node->nobreakBranch && !validateBlock(node->nobreakBranch->body, error)) return false;
        }
        if (const auto* node = dynamic_cast<const FunctionDeclarationAst*>(statement.get()))
            if (!validateBlock(node->body, error)) return false;
        if (const auto* node = dynamic_cast<const ConstructorDeclarationAst*>(statement.get()))
            if (!validateBlock(node->body, error)) return false;
        if (const auto* node = dynamic_cast<const AggregateDeclarationAst*>(statement.get()))
            if (!validateBlock(node->body, error)) return false;
        if (const auto* node = dynamic_cast<const ErrorStatementAst*>(statement.get()))
            if (node->recoveredBody && !validateBlock(*node->recoveredBody, error)) return false;
    }
    return true;
}
}

AnalyzedProgramAst analyzeProgramAst(CompileContext& context, ProgramAst& program) {
    return SemanticAnalyzer(context, program).run();
}

bool validateAnalyzedProgramAst(const AnalyzedProgramAst& program, std::string& invariantError) {
    if (!program.program) { invariantError = "analyzed program has no syntax tree"; return false; }
    return validateBlock(program.program->body, invariantError);
}
