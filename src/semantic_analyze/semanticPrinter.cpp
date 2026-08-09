#include "semanticPrinter.h"

#include "expressions.h"

#include <ostream>
#include <string>

namespace {
void line(std::ostream& output, int depth, const std::string& value) {
    output << std::string(static_cast<size_t>(depth * 2), ' ') << value << '\n';
}

std::string typeName(const Type& type) {
    return type == PrimitiveType::Unknown ? "<error>" : cpppTypeName(type);
}

void printExpr(std::ostream& output, const Expr* expression, int depth) {
    if (!expression) { line(output, depth, "MissingBound"); return; }
    std::string name = "Expr";
    if (const auto* node = dynamic_cast<const ErrorExpr*>(expression)) name = "Error " + node->reason;
    else if (const auto* node = dynamic_cast<const LiteralExpr*>(expression)) name = "Literal " + node->text;
    else if (const auto* node = dynamic_cast<const VariableExpr*>(expression)) name = "Variable " + node->name;
    else if (const auto* node = dynamic_cast<const FieldExpr*>(expression)) name = "Field " + node->field;
    else if (const auto* node = dynamic_cast<const UnaryExpr*>(expression)) name = "Unary " + node->op;
    else if (const auto* node = dynamic_cast<const BinaryExpr*>(expression)) name = "Binary " + node->op;
    else if (dynamic_cast<const CastExpr*>(expression)) name = "Cast";
    else if (const auto* node = dynamic_cast<const CallExpr*>(expression)) name = "Call " + node->callee;
    else if (dynamic_cast<const IndexExpr*>(expression)) name = "Index";
    else if (dynamic_cast<const SliceExpr*>(expression)) name = "Slice";
    else if (dynamic_cast<const ListLiteralExpr*>(expression)) name = "ListLiteral";
    else if (dynamic_cast<const SetLiteralExpr*>(expression)) name = "SetLiteral";
    else if (dynamic_cast<const MapLiteralExpr*>(expression)) name = "MapLiteral";
    else if (dynamic_cast<const PairLiteralExpr*>(expression)) name = "PairLiteral";
    line(output, depth, name);
    line(output, depth + 1, "type: " + typeName(expression->inferredType));
    line(output, depth + 1, std::string("valid: ") + (expression->semanticValid ? "true" : "false"));
    line(output, depth + 1, std::string("lvalue: ") + (expression->mutableValue ? "mutable" : "value"));
    if (!expression->resolvedSymbol.empty()) line(output, depth + 1, "symbol: " + expression->resolvedSymbol);
    if (expression->hasImplicitConversion)
        line(output, depth + 1, "conversion: " + typeName(expression->inferredType) + " -> " +
            typeName(expression->implicitConversionTarget));
    if (const auto* field = dynamic_cast<const FieldExpr*>(expression)) {
        if (!field->resolvedOwnerType.empty()) line(output, depth + 1, "owner: " + field->resolvedOwnerType);
        printExpr(output, field->base.get(), depth + 1);
    } else if (const auto* unary = dynamic_cast<const UnaryExpr*>(expression)) {
        printExpr(output, unary->operand.get(), depth + 1);
    } else if (const auto* binary = dynamic_cast<const BinaryExpr*>(expression)) {
        printExpr(output, binary->left.get(), depth + 1);
        printExpr(output, binary->right.get(), depth + 1);
    } else if (const auto* cast = dynamic_cast<const CastExpr*>(expression)) {
        line(output, depth + 1, "target: " + typeName(cast->targetType));
        printExpr(output, cast->operand.get(), depth + 1);
    } else if (const auto* call = dynamic_cast<const CallExpr*>(expression)) {
        if (!call->resolvedCallable.empty()) line(output, depth + 1, "callable: " + call->resolvedCallable);
        if (isFunctionType(call->functionType)) line(output, depth + 1, "signature: " + typeName(call->functionType));
        line(output, depth + 1, std::string("partial: ") + (call->partialApplication ? "true" : "false"));
        if (call->receiver) {
            line(output, depth + 1, "receiver:");
            printExpr(output, call->receiver.get(), depth + 2);
        }
        for (size_t i = 0; i < call->arguments.size(); ++i) {
            line(output, depth + 1, i < call->argumentNames.size() && !call->argumentNames[i].empty()
                ? "argument " + call->argumentNames[i] + ":" : "argument:");
            printExpr(output, call->arguments[i].get(), depth + 2);
        }
    } else if (const auto* index = dynamic_cast<const IndexExpr*>(expression)) {
        if (index->dynamicPairIndex) line(output, depth + 1, "pair-index: runtime-checked");
        printExpr(output, index->base.get(), depth + 1);
        printExpr(output, index->index.get(), depth + 1);
    } else if (const auto* slice = dynamic_cast<const SliceExpr*>(expression)) {
        line(output, depth + 1, "base:"); printExpr(output, slice->base.get(), depth + 2);
        line(output, depth + 1, "start:"); printExpr(output, slice->start.get(), depth + 2);
        line(output, depth + 1, "end:"); printExpr(output, slice->end.get(), depth + 2);
    } else if (const auto* list = dynamic_cast<const ListLiteralExpr*>(expression)) {
        for (const auto& item : list->elements) printExpr(output, item.get(), depth + 1);
    } else if (const auto* set = dynamic_cast<const SetLiteralExpr*>(expression)) {
        for (const auto& item : set->elements) printExpr(output, item.get(), depth + 1);
    } else if (const auto* map = dynamic_cast<const MapLiteralExpr*>(expression)) {
        for (const auto& item : map->entries) {
            line(output, depth + 1, "entry:");
            printExpr(output, item.key.get(), depth + 2);
            printExpr(output, item.value.get(), depth + 2);
        }
    } else if (const auto* pair = dynamic_cast<const PairLiteralExpr*>(expression)) {
        printExpr(output, pair->first.get(), depth + 1);
        printExpr(output, pair->second.get(), depth + 1);
    }
}

void printBlock(std::ostream& output, const BlockAst& block, int depth);

void printStatement(std::ostream& output, const ProgramStatement& statement, int depth) {
    std::string name = "Statement";
    if (dynamic_cast<const CommentStatementAst*>(&statement)) name = "Comment";
    else if (const auto* node = dynamic_cast<const ErrorStatementAst*>(&statement)) name = "ErrorStatement " + node->reason;
    else if (dynamic_cast<const VariableDeclarationAst*>(&statement)) name = "VariableDeclaration";
    else if (const auto* node = dynamic_cast<const AssignmentStatementAst*>(&statement)) name = "Assignment " + node->operation;
    else if (dynamic_cast<const ExpressionStatementAst*>(&statement)) name = "ExpressionStatement";
    else if (dynamic_cast<const ReturnStatementAst*>(&statement)) name = "Return";
    else if (statement.kind == ProgramStatementKind::Break) name = "Break";
    else if (statement.kind == ProgramStatementKind::Continue) name = "Continue";
    else if (dynamic_cast<const IfStatementAst*>(&statement)) name = "If";
    else if (dynamic_cast<const WhileStatementAst*>(&statement)) name = "While";
    else if (dynamic_cast<const ForStatementAst*>(&statement)) name = "For";
    else if (dynamic_cast<const ForEachStatementAst*>(&statement)) name = "ForEach";
    else if (dynamic_cast<const RepStatementAst*>(&statement)) name = "Rep";
    else if (const auto* node = dynamic_cast<const FunctionDeclarationAst*>(&statement)) name = "Function " + node->name;
    else if (const auto* node = dynamic_cast<const AggregateDeclarationAst*>(&statement))
        name = std::string(node->isClass ? "Class " : "Struct ") + node->name;
    line(output, depth, name);
    line(output, depth + 1, std::string("valid: ") + (statement.semanticValid ? "true" : "false"));
    if (const auto* node = dynamic_cast<const VariableDeclarationAst*>(&statement)) {
        line(output, depth + 1, "type: " + typeName(node->resolvedType));
        for (const std::string& item : node->names) line(output, depth + 1, "symbol: variable:" + item);
        for (const auto& value : node->initializers) printExpr(output, value.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const AssignmentStatementAst*>(&statement)) {
        for (const auto& value : node->targets) printExpr(output, value.get(), depth + 1);
        for (const auto& value : node->values) printExpr(output, value.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const ExpressionStatementAst*>(&statement)) {
        printExpr(output, node->expression.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const ReturnStatementAst*>(&statement)) {
        if (node->value) {
            line(output, depth + 1, "expected: " + typeName(node->expectedType));
            printExpr(output, node->value.get(), depth + 1);
        }
    } else if (const auto* node = dynamic_cast<const IfStatementAst*>(&statement)) {
        printExpr(output, node->condition.get(), depth + 1); printBlock(output, node->thenBody, depth + 1);
        for (const auto& item : node->elseIfBranches) { printExpr(output, item.condition.get(), depth + 1); printBlock(output, item.body, depth + 1); }
        if (node->elseBranch) printBlock(output, node->elseBranch->body, depth + 1);
    } else if (const auto* node = dynamic_cast<const WhileStatementAst*>(&statement)) {
        printExpr(output, node->condition.get(), depth + 1); printBlock(output, node->body, depth + 1);
        if (node->nobreakBranch) printBlock(output, node->nobreakBranch->body, depth + 1);
    } else if (const auto* node = dynamic_cast<const ForEachStatementAst*>(&statement)) {
        line(output, depth + 1, "loop-variable: " + node->variableName + " : " + typeName(node->resolvedVariableType));
        printExpr(output, node->iterable.get(), depth + 1); printBlock(output, node->body, depth + 1);
    } else if (const auto* node = dynamic_cast<const RepStatementAst*>(&statement)) {
        printExpr(output, node->count.get(), depth + 1); printBlock(output, node->body, depth + 1);
    } else if (const auto* node = dynamic_cast<const FunctionDeclarationAst*>(&statement)) {
        line(output, depth + 1, "signature: " + typeName(node->resolvedFunctionType));
        printBlock(output, node->body, depth + 1);
    } else if (const auto* node = dynamic_cast<const AggregateDeclarationAst*>(&statement)) {
        line(output, depth + 1, "type: " + typeName(node->resolvedType)); printBlock(output, node->body, depth + 1);
    }
}

void printBlock(std::ostream& output, const BlockAst& block, int depth) {
    line(output, depth, "Block");
    for (const auto& statement : block.statements) printStatement(output, *statement, depth + 1);
}
}

void printAnalyzedProgramAst(std::ostream& output, const AnalyzedProgramAst& program) {
    line(output, 0, std::string("AnalyzedProgram valid=") + (program.valid ? "true" : "false"));
    if (program.program) printBlock(output, program.program->body, 1);
}
