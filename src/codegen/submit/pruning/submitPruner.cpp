/*
 * Computes user-declaration reachability from semantic AST facts, then builds
 * an owning analyzed AST containing only declarations submit codegen receives.
 */

#include "submitPruner.h"

#include "semanticAnalyzer.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {
struct Reachability {
    std::set<std::string> functions;
    std::set<std::string> aggregates;
    std::set<std::string> methods;
};

struct PendingUnit {
    enum class Kind { Function, Aggregate, Method } kind;
    std::string owner;
    std::string name;
};

std::string methodKey(const std::string& owner, const std::string& name) {
    return owner + "." + name;
}

class ReachabilityCollector {
public:
    explicit ReachabilityCollector(const AnalyzedProgramAst& analyzed) : analyzed(analyzed) {
        if (!analyzed.program) return;
        for (const auto& statement : analyzed.program->body.statements) {
            if (const auto* function = dynamic_cast<const FunctionDeclarationAst*>(statement.get())) {
                functions[function->name] = function;
            } else if (const auto* aggregate = dynamic_cast<const AggregateDeclarationAst*>(statement.get())) {
                aggregates[aggregate->name] = aggregate;
                for (const auto& member : aggregate->body.statements) {
                    if (const auto* method = dynamic_cast<const FunctionDeclarationAst*>(member.get())) {
                        methods[methodKey(aggregate->name, method->name)] = method;
                    }
                }
            }
        }
    }

    Reachability collect() {
        if (!analyzed.program) return reachable;
        for (const auto& statement : analyzed.program->body.statements) {
            if (!dynamic_cast<const FunctionDeclarationAst*>(statement.get()) &&
                !dynamic_cast<const AggregateDeclarationAst*>(statement.get())) {
                scanStatement(*statement);
            }
        }
        for (size_t index = 0; index < pending.size(); ++index) {
            const PendingUnit unit = pending[index];
            if (unit.kind == PendingUnit::Kind::Function) scanFunction(unit.name);
            else if (unit.kind == PendingUnit::Kind::Aggregate) scanAggregate(unit.name);
            else scanMethod(unit.owner, unit.name);
        }
        return reachable;
    }

private:
    const AnalyzedProgramAst& analyzed;
    Reachability reachable;
    std::vector<PendingUnit> pending;
    std::map<std::string, const FunctionDeclarationAst*> functions;
    std::map<std::string, const AggregateDeclarationAst*> aggregates;
    std::map<std::string, const FunctionDeclarationAst*> methods;

    void markFunction(const std::string& name) {
        if (name.empty() || functions.count(name) == 0 || !reachable.functions.insert(name).second) return;
        pending.push_back({PendingUnit::Kind::Function, "", name});
    }

    void markAggregate(const std::string& name) {
        if (name.empty() || aggregates.count(name) == 0 || !reachable.aggregates.insert(name).second) return;
        pending.push_back({PendingUnit::Kind::Aggregate, name, ""});
    }

    void markMethod(const std::string& owner, const std::string& name) {
        const std::string key = methodKey(owner, name);
        if (methods.count(key) == 0 || !reachable.methods.insert(key).second) return;
        markAggregate(owner);
        pending.push_back({PendingUnit::Kind::Method, owner, name});
    }

    void scanType(const Type& type) {
        if (isStructType(type)) markAggregate(type.name);
        for (const Type& subtype : type.subtypes) scanType(subtype);
    }

    void scanExpr(const Expr* expression) {
        if (!expression) return;
        scanType(expression->inferredType);
        scanType(expression->implicitConversionTarget);

        if (const auto* variable = dynamic_cast<const VariableExpr*>(expression)) {
            const std::string prefix = "function:";
            if (variable->resolvedSymbol.rfind(prefix, 0) == 0) {
                markFunction(variable->resolvedSymbol.substr(prefix.size()));
            }
            return;
        }
        if (const auto* field = dynamic_cast<const FieldExpr*>(expression)) {
            scanExpr(field->base.get());
            return;
        }
        if (const auto* unary = dynamic_cast<const UnaryExpr*>(expression)) {
            scanExpr(unary->operand.get());
            return;
        }
        if (const auto* binary = dynamic_cast<const BinaryExpr*>(expression)) {
            scanExpr(binary->left.get());
            scanExpr(binary->right.get());
            return;
        }
        if (const auto* cast = dynamic_cast<const CastExpr*>(expression)) {
            scanType(cast->targetType);
            scanExpr(cast->operand.get());
            return;
        }
        if (const auto* call = dynamic_cast<const CallExpr*>(expression)) {
            const std::string functionPrefix = "function:";
            const std::string constructorPrefix = "constructor:";
            const std::string methodPrefix = "method:";
            if (call->resolvedCallable.rfind(functionPrefix, 0) == 0) {
                markFunction(call->resolvedCallable.substr(functionPrefix.size()));
            } else if (call->resolvedCallable.rfind(constructorPrefix, 0) == 0) {
                markAggregate(call->resolvedCallable.substr(constructorPrefix.size()));
            } else if (call->resolvedCallable.rfind(methodPrefix, 0) == 0) {
                const std::string identity = call->resolvedCallable.substr(methodPrefix.size());
                const size_t dot = identity.rfind('.');
                if (dot != std::string::npos) markMethod(identity.substr(0, dot), identity.substr(dot + 1));
            } else if (call->receiver && isStructType(call->receiver->inferredType)) {
                markMethod(call->receiver->inferredType.name, call->callee);
            }
            scanType(call->functionType);
            scanType(call->explicitConstructedType);
            scanExpr(call->receiver.get());
            for (const auto& argument : call->arguments) scanExpr(argument.get());
            return;
        }
        if (const auto* index = dynamic_cast<const IndexExpr*>(expression)) {
            scanExpr(index->base.get());
            scanExpr(index->index.get());
            return;
        }
        if (const auto* slice = dynamic_cast<const SliceExpr*>(expression)) {
            scanExpr(slice->base.get());
            scanExpr(slice->start.get());
            scanExpr(slice->end.get());
            return;
        }
        if (const auto* list = dynamic_cast<const ListLiteralExpr*>(expression)) {
            for (const auto& item : list->elements) scanExpr(item.get());
            return;
        }
        if (const auto* set = dynamic_cast<const SetLiteralExpr*>(expression)) {
            for (const auto& item : set->elements) scanExpr(item.get());
            return;
        }
        if (const auto* map = dynamic_cast<const MapLiteralExpr*>(expression)) {
            for (const auto& entry : map->entries) {
                scanExpr(entry.key.get());
                scanExpr(entry.value.get());
            }
            return;
        }
        if (const auto* pair = dynamic_cast<const PairLiteralExpr*>(expression)) {
            scanExpr(pair->first.get());
            scanExpr(pair->second.get());
        }
    }

    void scanForClause(const ForClauseAst& clause) {
        if (clause.type.hasResolvedType) scanType(clause.type.resolvedType);
        for (const auto& expression : clause.expressions) scanExpr(expression.get());
    }

    void scanBlock(const BlockAst& block) {
        for (const auto& statement : block.statements) scanStatement(*statement);
    }

    void scanStatement(const ProgramStatement& statement) {
        if (const auto* variable = dynamic_cast<const VariableDeclarationAst*>(&statement)) {
            scanType(variable->resolvedType);
            for (const auto& initializer : variable->initializers) scanExpr(initializer.get());
        } else if (const auto* assignment = dynamic_cast<const AssignmentStatementAst*>(&statement)) {
            for (const Type& type : assignment->resolvedTargetTypes) scanType(type);
            for (const Type& type : assignment->valueConversionTargets) scanType(type);
            for (const auto& target : assignment->targets) scanExpr(target.get());
            for (const auto& value : assignment->values) scanExpr(value.get());
        } else if (const auto* expression = dynamic_cast<const ExpressionStatementAst*>(&statement)) {
            scanExpr(expression->expression.get());
        } else if (const auto* returned = dynamic_cast<const ReturnStatementAst*>(&statement)) {
            scanType(returned->expectedType);
            scanExpr(returned->value.get());
        } else if (const auto* branch = dynamic_cast<const IfStatementAst*>(&statement)) {
            scanExpr(branch->condition.get());
            scanBlock(branch->thenBody);
            for (const ConditionalBranchAst& item : branch->elseIfBranches) {
                scanExpr(item.condition.get());
                scanBlock(item.body);
            }
            if (branch->elseBranch) scanBlock(branch->elseBranch->body);
        } else if (const auto* loop = dynamic_cast<const WhileStatementAst*>(&statement)) {
            scanExpr(loop->condition.get());
            scanBlock(loop->body);
            if (loop->nobreakBranch) scanBlock(loop->nobreakBranch->body);
        } else if (const auto* loop = dynamic_cast<const ForStatementAst*>(&statement)) {
            scanForClause(loop->initializer);
            scanExpr(loop->condition.get());
            scanForClause(loop->iteration);
            scanBlock(loop->body);
            if (loop->nobreakBranch) scanBlock(loop->nobreakBranch->body);
        } else if (const auto* loop = dynamic_cast<const ForEachStatementAst*>(&statement)) {
            scanType(loop->resolvedVariableType);
            scanExpr(loop->iterable.get());
            scanBlock(loop->body);
            if (loop->nobreakBranch) scanBlock(loop->nobreakBranch->body);
        } else if (const auto* loop = dynamic_cast<const RepStatementAst*>(&statement)) {
            scanType(loop->resolvedCountType);
            scanExpr(loop->count.get());
            scanBlock(loop->body);
            if (loop->nobreakBranch) scanBlock(loop->nobreakBranch->body);
        } else if (const auto* error = dynamic_cast<const ErrorStatementAst*>(&statement)) {
            if (error->recoveredBody) scanBlock(*error->recoveredBody);
        }
    }

    void scanFunction(const std::string& name) {
        const auto found = functions.find(name);
        if (found == functions.end()) return;
        const FunctionDeclarationAst& function = *found->second;
        scanType(function.resolvedFunctionType);
        scanBlock(function.body);
    }

    void scanAggregate(const std::string& name) {
        const auto found = aggregates.find(name);
        if (found == aggregates.end()) return;
        const AggregateDeclarationAst& aggregate = *found->second;
        scanType(aggregate.resolvedType);
        const auto fields = analyzed.aggregateFields.find(name);
        if (fields != analyzed.aggregateFields.end()) {
            for (const auto& field : fields->second) scanType(field.second);
        }
        const auto constructor = analyzed.aggregateConstructors.find(name);
        if (constructor != analyzed.aggregateConstructors.end()) {
            scanType(constructor->second.returnType);
            for (const FunctionParameter& parameter : constructor->second.parameters) scanType(parameter.type);
        }
        for (const auto& member : aggregate.body.statements) {
            if (const auto* field = dynamic_cast<const VariableDeclarationAst*>(member.get())) {
                scanStatement(*field);
            } else if (const auto* ctor = dynamic_cast<const ConstructorDeclarationAst*>(member.get())) {
                scanType(ctor->resolvedFunctionType);
                scanBlock(ctor->body);
            }
        }
    }

    void scanMethod(const std::string& owner, const std::string& name) {
        const auto found = methods.find(methodKey(owner, name));
        if (found == methods.end()) return;
        scanType(found->second->resolvedFunctionType);
        scanBlock(found->second->body);
    }
};

void copyExprFacts(const Expr& source, Expr& target) {
    target.sourceColumn = source.sourceColumn;
    target.sourceSpan = source.sourceSpan;
    target.originSpan = source.originSpan;
    target.inferredType = source.inferredType;
    target.mutableValue = source.mutableValue;
    target.explicitCast = source.explicitCast;
    target.semanticAnalyzed = source.semanticAnalyzed;
    target.semanticValid = source.semanticValid;
    target.hasImplicitConversion = source.hasImplicitConversion;
    target.implicitConversionTarget = source.implicitConversionTarget;
    target.resolvedSymbol = source.resolvedSymbol;
}

std::unique_ptr<Expr> cloneExpr(const Expr* source) {
    if (!source) return nullptr;
    std::unique_ptr<Expr> result;
    if (const auto* node = dynamic_cast<const ErrorExpr*>(source)) {
        result = std::make_unique<ErrorExpr>(
            node->reason,
            node->sourceColumn,
            node->sourceSpan,
            node->suggestedReplacement,
            node->suggestionMessage,
            node->suggestionIsMachineApplicable
        );
    } else if (const auto* node = dynamic_cast<const LiteralExpr*>(source)) {
        result = std::make_unique<LiteralExpr>(node->kind, node->text, node->sourceColumn, node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const VariableExpr*>(source)) {
        result = std::make_unique<VariableExpr>(node->name, node->sourceColumn, node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const FieldExpr*>(source)) {
        auto value = std::make_unique<FieldExpr>(cloneExpr(node->base.get()), node->field, node->sourceColumn, node->sourceSpan);
        value->resolvedOwnerType = node->resolvedOwnerType;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const UnaryExpr*>(source)) {
        result = std::make_unique<UnaryExpr>(node->op, cloneExpr(node->operand.get()), node->sourceColumn, node->postfix, node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const BinaryExpr*>(source)) {
        result = std::make_unique<BinaryExpr>(node->op, cloneExpr(node->left.get()), cloneExpr(node->right.get()), node->sourceColumn, node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const CastExpr*>(source)) {
        result = std::make_unique<CastExpr>(node->targetType, cloneExpr(node->operand.get()), node->sourceColumn, node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const CallExpr*>(source)) {
        std::vector<std::unique_ptr<Expr>> arguments;
        for (const auto& argument : node->arguments) arguments.push_back(cloneExpr(argument.get()));
        auto value = std::make_unique<CallExpr>(node->callee, cloneExpr(node->receiver.get()), std::move(arguments),
            node->sourceColumn, node->sourceSpan, node->argumentNames);
        value->functionType = node->functionType;
        value->explicitConstructedType = node->explicitConstructedType;
        value->partialApplication = node->partialApplication;
        value->resolvedCallable = node->resolvedCallable;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const IndexExpr*>(source)) {
        auto value = std::make_unique<IndexExpr>(cloneExpr(node->base.get()), cloneExpr(node->index.get()), node->sourceColumn, node->sourceSpan);
        value->dynamicPairIndex = node->dynamicPairIndex;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const SliceExpr*>(source)) {
        result = std::make_unique<SliceExpr>(cloneExpr(node->base.get()), cloneExpr(node->start.get()), cloneExpr(node->end.get()), node->sourceColumn, node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const ListLiteralExpr*>(source)) {
        std::vector<std::unique_ptr<Expr>> elements;
        for (const auto& item : node->elements) elements.push_back(cloneExpr(item.get()));
        result = std::make_unique<ListLiteralExpr>(std::move(elements), node->sourceColumn, node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const SetLiteralExpr*>(source)) {
        std::vector<std::unique_ptr<Expr>> elements;
        for (const auto& item : node->elements) elements.push_back(cloneExpr(item.get()));
        result = std::make_unique<SetLiteralExpr>(std::move(elements), node->sourceColumn, node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const MapLiteralExpr*>(source)) {
        std::vector<MapLiteralEntry> entries;
        for (const MapLiteralEntry& entry : node->entries) entries.push_back({cloneExpr(entry.key.get()), cloneExpr(entry.value.get())});
        result = std::make_unique<MapLiteralExpr>(std::move(entries), node->sourceColumn, node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const PairLiteralExpr*>(source)) {
        result = std::make_unique<PairLiteralExpr>(cloneExpr(node->first.get()), cloneExpr(node->second.get()), node->sourceColumn, node->sourceSpan);
    }
    if (result) copyExprFacts(*source, *result);
    return result;
}

void copyStatementFacts(const ProgramStatement& source, ProgramStatement& target) {
    target.sourceSpan = source.sourceSpan;
    target.syntaxOk = source.syntaxOk;
    target.syntaxErrorOffset = source.syntaxErrorOffset;
    target.syntaxError = source.syntaxError;
    target.semanticAnalyzed = source.semanticAnalyzed;
    target.semanticValid = source.semanticValid;
}

ForClauseAst cloneForClause(const ForClauseAst& source) {
    ForClauseAst result;
    result.sourceSpan = source.sourceSpan;
    result.kind = source.kind;
    result.type = source.type;
    result.inferredType = source.inferredType;
    result.names = source.names;
    result.nameSpans = source.nameSpans;
    result.continuationTokenIndex = source.continuationTokenIndex;
    result.operation = source.operation;
    result.operationToken = source.operationToken;
    for (const auto& expression : source.expressions) result.expressions.push_back(cloneExpr(expression.get()));
    result.targetTokens = source.targetTokens;
    result.valueTokens = source.valueTokens;
    result.targetOffsets = source.targetOffsets;
    result.valueOffsets = source.valueOffsets;
    result.tokens = source.tokens;
    result.offset = source.offset;
    return result;
}

BlockAst cloneBlock(const BlockAst& source, const Reachability& reachable, bool topLevel = false, const std::string& aggregate = "");

std::unique_ptr<CompletionBranchAst> cloneCompletion(
    const CompletionBranchAst* source,
    const Reachability& reachable
) {
    if (!source) return nullptr;
    auto result = std::make_unique<CompletionBranchAst>();
    result->sourceSpan = source->sourceSpan;
    result->headerSyntax = source->headerSyntax;
    result->body = cloneBlock(source->body, reachable);
    return result;
}

std::unique_ptr<ProgramStatement> cloneStatement(
    const ProgramStatement& source,
    const Reachability& reachable,
    bool topLevel,
    const std::string& aggregate
) {
    std::unique_ptr<ProgramStatement> result;
    if (const auto* node = dynamic_cast<const CommentStatementAst*>(&source)) {
        result = std::make_unique<CommentStatementAst>(node->syntax);
    } else if (const auto* node = dynamic_cast<const ErrorStatementAst*>(&source)) {
        auto value = std::make_unique<ErrorStatementAst>(node->syntax, node->reason);
        if (node->recoveredBody) value->recoveredBody = std::make_unique<BlockAst>(cloneBlock(*node->recoveredBody, reachable));
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const VariableDeclarationAst*>(&source)) {
        auto value = std::make_unique<VariableDeclarationAst>(node->syntax);
        value->type = node->type;
        value->inferredType = node->inferredType;
        value->names = node->names;
        value->nameSpans = node->nameSpans;
        for (const auto& initializer : node->initializers) value->initializers.push_back(cloneExpr(initializer.get()));
        value->continuationTokenIndex = node->continuationTokenIndex;
        value->initializerKind = node->initializerKind;
        value->resolvedType = node->resolvedType;
        value->initializerConversionTargets = node->initializerConversionTargets;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const AssignmentStatementAst*>(&source)) {
        auto value = std::make_unique<AssignmentStatementAst>(node->syntax);
        value->operation = node->operation;
        value->operationSpan = node->operationSpan;
        value->operationToken = node->operationToken;
        for (const auto& target : node->targets) value->targets.push_back(cloneExpr(target.get()));
        for (const auto& item : node->values) value->values.push_back(cloneExpr(item.get()));
        value->targetTokens = node->targetTokens;
        value->valueTokens = node->valueTokens;
        value->targetOffsets = node->targetOffsets;
        value->valueOffsets = node->valueOffsets;
        value->resolvedTargetTypes = node->resolvedTargetTypes;
        value->valueConversionTargets = node->valueConversionTargets;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const ExpressionStatementAst*>(&source)) {
        auto value = std::make_unique<ExpressionStatementAst>(node->syntax);
        value->expression = cloneExpr(node->expression.get());
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const ReturnStatementAst*>(&source)) {
        auto value = std::make_unique<ReturnStatementAst>(node->syntax);
        value->value = cloneExpr(node->value.get());
        value->valueTokens = node->valueTokens;
        value->valueOffset = node->valueOffset;
        value->expectedType = node->expectedType;
        value->hasValueConversion = node->hasValueConversion;
        result = std::move(value);
    } else if (source.kind == ProgramStatementKind::Break || source.kind == ProgramStatementKind::Continue) {
        result = std::make_unique<SimpleControlStatementAst>(source.kind, source.syntax);
    } else if (const auto* node = dynamic_cast<const IfStatementAst*>(&source)) {
        auto value = std::make_unique<IfStatementAst>(node->syntax);
        value->condition = cloneExpr(node->condition.get());
        value->conditionTokens = node->conditionTokens;
        value->conditionOffset = node->conditionOffset;
        value->thenBody = cloneBlock(node->thenBody, reachable);
        for (const ConditionalBranchAst& branch : node->elseIfBranches) {
            ConditionalBranchAst cloned;
            cloned.sourceSpan = branch.sourceSpan;
            cloned.headerSyntax = branch.headerSyntax;
            cloned.condition = cloneExpr(branch.condition.get());
            cloned.conditionTokens = branch.conditionTokens;
            cloned.conditionOffset = branch.conditionOffset;
            cloned.syntaxOk = branch.syntaxOk;
            cloned.syntaxErrorOffset = branch.syntaxErrorOffset;
            cloned.syntaxError = branch.syntaxError;
            cloned.body = cloneBlock(branch.body, reachable);
            cloned.hasConditionConversion = branch.hasConditionConversion;
            value->elseIfBranches.push_back(std::move(cloned));
        }
        value->elseBranch = cloneCompletion(node->elseBranch.get(), reachable);
        value->hasConditionConversion = node->hasConditionConversion;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const WhileStatementAst*>(&source)) {
        auto value = std::make_unique<WhileStatementAst>(node->syntax);
        value->condition = cloneExpr(node->condition.get());
        value->conditionTokens = node->conditionTokens;
        value->conditionOffset = node->conditionOffset;
        value->body = cloneBlock(node->body, reachable);
        value->nobreakBranch = cloneCompletion(node->nobreakBranch.get(), reachable);
        value->hasConditionConversion = node->hasConditionConversion;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const ForStatementAst*>(&source)) {
        auto value = std::make_unique<ForStatementAst>(node->syntax);
        value->initializer = cloneForClause(node->initializer);
        value->condition = cloneExpr(node->condition.get());
        value->conditionTokens = node->conditionTokens;
        value->conditionOffset = node->conditionOffset;
        value->iteration = cloneForClause(node->iteration);
        value->body = cloneBlock(node->body, reachable);
        value->nobreakBranch = cloneCompletion(node->nobreakBranch.get(), reachable);
        value->hasConditionConversion = node->hasConditionConversion;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const ForEachStatementAst*>(&source)) {
        auto value = std::make_unique<ForEachStatementAst>(node->syntax);
        value->variableType = node->variableType;
        value->variableName = node->variableName;
        value->inferredVariable = node->inferredVariable;
        value->iterable = cloneExpr(node->iterable.get());
        value->iterableTokens = node->iterableTokens;
        value->variableOffset = node->variableOffset;
        value->iterableOffset = node->iterableOffset;
        value->body = cloneBlock(node->body, reachable);
        value->nobreakBranch = cloneCompletion(node->nobreakBranch.get(), reachable);
        value->resolvedVariableType = node->resolvedVariableType;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const RepStatementAst*>(&source)) {
        auto value = std::make_unique<RepStatementAst>(node->syntax);
        value->count = cloneExpr(node->count.get());
        value->countTokens = node->countTokens;
        value->countOffset = node->countOffset;
        value->body = cloneBlock(node->body, reachable);
        value->nobreakBranch = cloneCompletion(node->nobreakBranch.get(), reachable);
        value->resolvedCountType = node->resolvedCountType;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const FunctionDeclarationAst*>(&source)) {
        if ((topLevel && reachable.functions.count(node->name) == 0) ||
            (!aggregate.empty() && reachable.methods.count(methodKey(aggregate, node->name)) == 0)) return nullptr;
        auto value = std::make_unique<FunctionDeclarationAst>(node->syntax);
        value->returnType = node->returnType;
        value->name = node->name;
        value->nameSpan = node->nameSpan;
        value->parameters = node->parameters;
        value->body = cloneBlock(node->body, reachable);
        value->resolvedFunctionType = node->resolvedFunctionType;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const ConstructorDeclarationAst*>(&source)) {
        auto value = std::make_unique<ConstructorDeclarationAst>(node->syntax);
        value->name = node->name;
        value->nameSpan = node->nameSpan;
        value->parameters = node->parameters;
        value->body = cloneBlock(node->body, reachable);
        value->resolvedFunctionType = node->resolvedFunctionType;
        result = std::move(value);
    } else if (const auto* node = dynamic_cast<const AggregateDeclarationAst*>(&source)) {
        if (topLevel && reachable.aggregates.count(node->name) == 0) return nullptr;
        auto value = std::make_unique<AggregateDeclarationAst>(node->syntax);
        value->name = node->name;
        value->nameSpan = node->nameSpan;
        value->isClass = node->isClass;
        value->body = cloneBlock(node->body, reachable, false, node->name);
        value->resolvedType = node->resolvedType;
        result = std::move(value);
    }
    if (result) copyStatementFacts(source, *result);
    return result;
}

BlockAst cloneBlock(const BlockAst& source, const Reachability& reachable, bool topLevel, const std::string& aggregate) {
    BlockAst result;
    result.sourceSpan = source.sourceSpan;
    result.closingSyntax = source.closingSyntax;
    result.hasClosingSyntax = source.hasClosingSyntax;
    for (const auto& statement : source.statements) {
        std::unique_ptr<ProgramStatement> cloned = cloneStatement(*statement, reachable, topLevel, aggregate);
        if (cloned) result.statements.push_back(std::move(cloned));
    }
    return result;
}

std::unique_ptr<ProgramAst> cloneProgram(const ProgramAst& source, const Reachability& reachable) {
    auto result = std::make_unique<ProgramAst>();
    result->sourceSpan = source.sourceSpan;
    result->body = cloneBlock(source.body, reachable, true);
    return result;
}

bool sameSpan(SourceSpan left, SourceSpan right) {
    return left.source == right.source && left.startOffset == right.startOffset && left.endOffset == right.endOffset;
}
}

PrunedAnalyzedProgramAst pruneAnalyzedProgramForSubmit(const AnalyzedProgramAst& analyzed) {
    PrunedAnalyzedProgramAst result;
    if (!analyzed.program) return result;
    const Reachability reachable = ReachabilityCollector(analyzed).collect();
    result.reachableFunctions = reachable.functions;
    result.reachableAggregates = reachable.aggregates;
    result.reachableMethods = reachable.methods;
    result.ownedProgram = cloneProgram(*analyzed.program, reachable);
    result.analyzed.program = result.ownedProgram.get();
    result.analyzed.valid = analyzed.valid;

    for (const std::string& name : reachable.functions) {
        const auto found = analyzed.functions.find(name);
        if (found != analyzed.functions.end()) result.analyzed.functions[name] = found->second;
    }
    for (const std::string& name : reachable.aggregates) {
        const auto fields = analyzed.aggregateFields.find(name);
        if (fields != analyzed.aggregateFields.end()) result.analyzed.aggregateFields[name] = fields->second;
        const auto order = analyzed.aggregateFieldOrder.find(name);
        if (order != analyzed.aggregateFieldOrder.end()) result.analyzed.aggregateFieldOrder[name] = order->second;
        const auto constructor = analyzed.aggregateConstructors.find(name);
        if (constructor != analyzed.aggregateConstructors.end()) result.analyzed.aggregateConstructors[name] = constructor->second;
        if (analyzed.classNames.count(name) != 0) result.analyzed.classNames.insert(name);

        const auto methods = analyzed.aggregateMethods.find(name);
        result.analyzed.aggregateMethods[name] = {};
        if (methods != analyzed.aggregateMethods.end()) {
            for (const auto& method : methods->second) {
                if (reachable.methods.count(methodKey(name, method.first)) != 0) {
                    result.analyzed.aggregateMethods[name][method.first] = method.second;
                }
            }
        }
    }
    for (const std::string& name : analyzed.aggregateEmissionOrder) {
        if (reachable.aggregates.count(name) != 0) result.analyzed.aggregateEmissionOrder.push_back(name);
    }
    return result;
}

bool validatePrunedAnalyzedProgramAst(
    const PrunedAnalyzedProgramAst& program,
    const AnalyzedProgramAst& original,
    std::string& invariantError
) {
    if (!program.ownedProgram || program.analyzed.program != program.ownedProgram.get()) {
        invariantError = "pruned analyzed AST does not own its program";
        return false;
    }
    if (original.program == program.analyzed.program) {
        invariantError = "submit pruning reused the complete ProgramAst instead of cloning it";
        return false;
    }
    if (!original.program || !sameSpan(original.program->sourceSpan, program.ownedProgram->sourceSpan)) {
        invariantError = "submit pruning changed the program source span";
        return false;
    }
    if (!validateAnalyzedProgramAst(program.analyzed, invariantError)) return false;

    const Reachability expected = ReachabilityCollector(original).collect();
    if (expected.functions != program.reachableFunctions ||
        expected.aggregates != program.reachableAggregates ||
        expected.methods != program.reachableMethods) {
        invariantError = "submit reachability changed between pruning and validation";
        return false;
    }

    std::set<std::string> astFunctions;
    std::set<std::string> astAggregates;
    std::set<std::string> astMethods;
    for (const auto& statement : program.ownedProgram->body.statements) {
        if (const auto* function = dynamic_cast<const FunctionDeclarationAst*>(statement.get())) {
            astFunctions.insert(function->name);
        } else if (const auto* aggregate = dynamic_cast<const AggregateDeclarationAst*>(statement.get())) {
            astAggregates.insert(aggregate->name);
            for (const auto& member : aggregate->body.statements) {
                if (const auto* method = dynamic_cast<const FunctionDeclarationAst*>(member.get())) {
                    astMethods.insert(methodKey(aggregate->name, method->name));
                }
            }
        }
    }
    if (astFunctions != program.reachableFunctions || astAggregates != program.reachableAggregates ||
        astMethods != program.reachableMethods) {
        invariantError = "pruned AST declarations do not match its reachability sets";
        return false;
    }
    std::set<std::string> tableFunctions;
    for (const auto& function : program.analyzed.functions) tableFunctions.insert(function.first);
    if (tableFunctions != astFunctions) {
        invariantError = "pruned function table does not match the pruned AST";
        return false;
    }
    for (const auto& aggregate : program.analyzed.aggregateMethods) {
        for (const auto& method : aggregate.second) {
            if (astMethods.count(methodKey(aggregate.first, method.first)) == 0) {
                invariantError = "pruned method table contains a method absent from the pruned AST";
                return false;
            }
        }
    }
    for (const auto& statement : program.ownedProgram->body.statements) {
        const auto* aggregate = dynamic_cast<const AggregateDeclarationAst*>(statement.get());
        if (!aggregate) continue;
        std::set<std::string> astFields;
        bool hasConstructor = false;
        for (const auto& member : aggregate->body.statements) {
            if (const auto* field = dynamic_cast<const VariableDeclarationAst*>(member.get())) {
                astFields.insert(field->names.begin(), field->names.end());
            } else if (dynamic_cast<const ConstructorDeclarationAst*>(member.get())) {
                hasConstructor = true;
            } else if (const auto* method = dynamic_cast<const FunctionDeclarationAst*>(member.get())) {
                const auto table = program.analyzed.aggregateMethods.find(aggregate->name);
                if (table == program.analyzed.aggregateMethods.end() || table->second.count(method->name) == 0) {
                    invariantError = "retained method has no semantic signature";
                    return false;
                }
            }
        }
        const auto fields = program.analyzed.aggregateFields.find(aggregate->name);
        if (fields == program.analyzed.aggregateFields.end()) {
            invariantError = "retained aggregate has no semantic field table";
            return false;
        }
        std::set<std::string> tableFields;
        for (const auto& field : fields->second) tableFields.insert(field.first);
        if (tableFields != astFields) {
            invariantError = "aggregate field table does not match the pruned AST";
            return false;
        }
        const auto order = program.analyzed.aggregateFieldOrder.find(aggregate->name);
        if (order == program.analyzed.aggregateFieldOrder.end() ||
            std::set<std::string>(order->second.begin(), order->second.end()) != astFields ||
            order->second.size() != astFields.size()) {
            invariantError = "aggregate field order does not match its retained fields";
            return false;
        }
        const bool hasConstructorSignature = program.analyzed.aggregateConstructors.count(aggregate->name) != 0;
        if (hasConstructor != hasConstructorSignature) {
            invariantError = "aggregate constructor table does not match the pruned AST";
            return false;
        }
    }
    return true;
}
