/* Deterministic tree printer for ProgramAst. */

#include "astPrinter.h"

#include <ostream>
#include <sstream>
#include <string>

namespace {
std::string quote(const std::string& text) {
    std::ostringstream output;
    output << '"';
    for (unsigned char ch : text) {
        switch (ch) {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default: output << static_cast<char>(ch); break;
        }
    }
    output << '"';
    return output.str();
}

std::string spanText(SourceSpan span) {
    if (!span.valid()) return "[no-span]";
    return "[" + std::to_string(span.startOffset) + ".." + std::to_string(span.endOffset) + "]";
}

void line(std::ostream& output, int depth, const std::string& text, SourceSpan span = {}) {
    output << std::string(static_cast<size_t>(depth * 2), ' ') << text;
    if (span.valid()) output << ' ' << spanText(span);
    output << '\n';
}

const char* literalKindName(LiteralExpr::Kind kind) {
    switch (kind) {
        case LiteralExpr::Kind::Bool: return "Bool";
        case LiteralExpr::Kind::Null: return "Null";
        case LiteralExpr::Kind::Int: return "Int";
        case LiteralExpr::Kind::Float: return "Float";
        case LiteralExpr::Kind::String: return "String";
        case LiteralExpr::Kind::Char: return "Char";
    }
    return "Unknown";
}

void printExpr(std::ostream& output, const Expr* expression, int depth) {
    if (!expression) {
        line(output, depth, "MissingExpr");
        return;
    }
    if (const auto* node = dynamic_cast<const ErrorExpr*>(expression)) {
        line(output, depth, "ErrorExpr " + quote(node->reason), node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const LiteralExpr*>(expression)) {
        line(output, depth, std::string("LiteralExpr ") + literalKindName(node->kind) + " " + quote(node->text), node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const VariableExpr*>(expression)) {
        line(output, depth, "VariableExpr " + quote(node->name), node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const FieldExpr*>(expression)) {
        line(output, depth, "FieldExpr " + quote(node->field), node->sourceSpan);
        printExpr(output, node->base.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const UnaryExpr*>(expression)) {
        line(output, depth, std::string(node->postfix ? "PostfixExpr " : "UnaryExpr ") + quote(node->op), node->sourceSpan);
        printExpr(output, node->operand.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const BinaryExpr*>(expression)) {
        line(output, depth, "BinaryExpr " + quote(node->op), node->sourceSpan);
        printExpr(output, node->left.get(), depth + 1);
        printExpr(output, node->right.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const CastExpr*>(expression)) {
        line(output, depth, "CastExpr", node->sourceSpan);
        printExpr(output, node->operand.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const CallExpr*>(expression)) {
        line(output, depth, "CallExpr " + quote(node->callee), node->sourceSpan);
        if (node->receiver) {
            line(output, depth + 1, "Receiver");
            printExpr(output, node->receiver.get(), depth + 2);
        }
        line(output, depth + 1, "Arguments");
        for (size_t index = 0; index < node->arguments.size(); ++index) {
            if (index < node->argumentNames.size() && !node->argumentNames[index].empty()) {
                line(output, depth + 2, "NamedArgument " + quote(node->argumentNames[index]));
                printExpr(output, node->arguments[index].get(), depth + 3);
            } else {
                printExpr(output, node->arguments[index].get(), depth + 2);
            }
        }
    } else if (const auto* node = dynamic_cast<const IndexExpr*>(expression)) {
        line(output, depth, "IndexExpr", node->sourceSpan);
        printExpr(output, node->base.get(), depth + 1);
        printExpr(output, node->index.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const SliceExpr*>(expression)) {
        line(output, depth, "SliceExpr", node->sourceSpan);
        printExpr(output, node->base.get(), depth + 1);
        printExpr(output, node->start.get(), depth + 1);
        printExpr(output, node->end.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const ListLiteralExpr*>(expression)) {
        line(output, depth, "ListLiteralExpr", node->sourceSpan);
        for (const auto& element : node->elements) printExpr(output, element.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const SetLiteralExpr*>(expression)) {
        line(output, depth, "SetLiteralExpr", node->sourceSpan);
        for (const auto& element : node->elements) printExpr(output, element.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const MapLiteralExpr*>(expression)) {
        line(output, depth, "MapLiteralExpr", node->sourceSpan);
        for (const MapLiteralEntry& entry : node->entries) {
            line(output, depth + 1, "Entry");
            printExpr(output, entry.key.get(), depth + 2);
            printExpr(output, entry.value.get(), depth + 2);
        }
    } else if (const auto* node = dynamic_cast<const PairLiteralExpr*>(expression)) {
        line(output, depth, "PairLiteralExpr", node->sourceSpan);
        printExpr(output, node->first.get(), depth + 1);
        printExpr(output, node->second.get(), depth + 1);
    } else {
        line(output, depth, "UnknownExpr", expression->sourceSpan);
    }
}

void printType(std::ostream& output, const TypeSyntax& type, int depth) {
    line(output, depth, "TypeSyntax " + quote(type.spelling), type.sourceSpan);
    for (const TypeSyntax& argument : type.arguments) printType(output, argument, depth + 1);
    if (type.functionType) {
        line(output, depth + 1, "FunctionParameters");
        for (const TypeSyntax& parameter : type.functionParameters) printType(output, parameter, depth + 2);
    }
}

void printBlock(std::ostream& output, const BlockAst& block, int depth);

void printCompletion(std::ostream& output, const CompletionBranchAst* branch, int depth, const std::string& name) {
    if (!branch) return;
    line(output, depth, name, branch->sourceSpan);
    printBlock(output, branch->body, depth + 1);
}

void printForClause(std::ostream& output, const ForClauseAst& clause, int depth, const std::string& name) {
    static const char* names[] = {"Empty", "VariableDeclaration", "Assignment", "Expression", "Error"};
    line(output, depth, name + " " + names[static_cast<int>(clause.kind)], clause.sourceSpan);
    if (!clause.type.spelling.empty()) printType(output, clause.type, depth + 1);
    for (const std::string& variable : clause.names) line(output, depth + 1, "Name " + quote(variable));
    if (!clause.operation.empty()) line(output, depth + 1, "Operation " + quote(clause.operation));
    for (const auto& expression : clause.expressions) printExpr(output, expression.get(), depth + 1);
}

void printStatement(std::ostream& output, const ProgramStatement& statement, int depth) {
    if (const auto* node = dynamic_cast<const CommentStatementAst*>(&statement)) {
        line(output, depth, "CommentStmt " + quote(node->syntax.commentText), node->sourceSpan);
    } else if (const auto* node = dynamic_cast<const ErrorStatementAst*>(&statement)) {
        line(output, depth, "ErrorStmt " + quote(node->reason), node->sourceSpan);
        if (node->recoveredBody) printBlock(output, *node->recoveredBody, depth + 1);
    } else if (const auto* node = dynamic_cast<const VariableDeclarationAst*>(&statement)) {
        line(output, depth, "VariableDeclStmt", node->sourceSpan);
        printType(output, node->type, depth + 1);
        for (size_t index = 0; index < node->names.size(); ++index) {
            line(output, depth + 1, "Name " + quote(node->names[index]), index < node->nameSpans.size() ? node->nameSpans[index] : SourceSpan{});
        }
        if (!node->initializers.empty()) line(output, depth + 1, "Initializers");
        for (const auto& initializer : node->initializers) printExpr(output, initializer.get(), depth + 2);
    } else if (const auto* node = dynamic_cast<const AssignmentStatementAst*>(&statement)) {
        line(output, depth, "AssignmentStmt " + quote(node->operation), node->sourceSpan);
        line(output, depth + 1, "Targets");
        for (const auto& target : node->targets) printExpr(output, target.get(), depth + 2);
        line(output, depth + 1, "Values");
        for (const auto& value : node->values) printExpr(output, value.get(), depth + 2);
    } else if (const auto* node = dynamic_cast<const ExpressionStatementAst*>(&statement)) {
        line(output, depth, "ExpressionStmt", node->sourceSpan);
        printExpr(output, node->expression.get(), depth + 1);
    } else if (const auto* node = dynamic_cast<const ReturnStatementAst*>(&statement)) {
        line(output, depth, "ReturnStmt", node->sourceSpan);
        if (node->value) printExpr(output, node->value.get(), depth + 1);
    } else if (statement.kind == ProgramStatementKind::Break) {
        line(output, depth, "BreakStmt", statement.sourceSpan);
    } else if (statement.kind == ProgramStatementKind::Continue) {
        line(output, depth, "ContinueStmt", statement.sourceSpan);
    } else if (const auto* node = dynamic_cast<const IfStatementAst*>(&statement)) {
        line(output, depth, "IfStmt", node->sourceSpan);
        line(output, depth + 1, "Condition");
        printExpr(output, node->condition.get(), depth + 2);
        line(output, depth + 1, "Then");
        printBlock(output, node->thenBody, depth + 2);
        for (const ConditionalBranchAst& branch : node->elseIfBranches) {
            line(output, depth + 1, "ElseIf", branch.sourceSpan);
            printExpr(output, branch.condition.get(), depth + 2);
            printBlock(output, branch.body, depth + 2);
        }
        printCompletion(output, node->elseBranch.get(), depth + 1, "Else");
    } else if (const auto* node = dynamic_cast<const WhileStatementAst*>(&statement)) {
        line(output, depth, "WhileStmt", node->sourceSpan);
        printExpr(output, node->condition.get(), depth + 1);
        printBlock(output, node->body, depth + 1);
        printCompletion(output, node->nobreakBranch.get(), depth + 1, "Nobreak");
    } else if (const auto* node = dynamic_cast<const ForStatementAst*>(&statement)) {
        line(output, depth, "ForStmt", node->sourceSpan);
        printForClause(output, node->initializer, depth + 1, "Initializer");
        line(output, depth + 1, "Condition");
        if (node->condition) printExpr(output, node->condition.get(), depth + 2);
        printForClause(output, node->iteration, depth + 1, "Iteration");
        printBlock(output, node->body, depth + 1);
        printCompletion(output, node->nobreakBranch.get(), depth + 1, "Nobreak");
    } else if (const auto* node = dynamic_cast<const ForEachStatementAst*>(&statement)) {
        line(output, depth, "ForEachStmt " + quote(node->variableName), node->sourceSpan);
        if (!node->variableType.spelling.empty()) printType(output, node->variableType, depth + 1);
        printExpr(output, node->iterable.get(), depth + 1);
        printBlock(output, node->body, depth + 1);
        printCompletion(output, node->nobreakBranch.get(), depth + 1, "Nobreak");
    } else if (const auto* node = dynamic_cast<const RepStatementAst*>(&statement)) {
        line(output, depth, "RepStmt", node->sourceSpan);
        printExpr(output, node->count.get(), depth + 1);
        printBlock(output, node->body, depth + 1);
        printCompletion(output, node->nobreakBranch.get(), depth + 1, "Nobreak");
    } else if (const auto* node = dynamic_cast<const FunctionDeclarationAst*>(&statement)) {
        line(output, depth, "FunctionDecl " + quote(node->name), node->sourceSpan);
        line(output, depth + 1, "ReturnType");
        printType(output, node->returnType, depth + 2);
        line(output, depth + 1, "Parameters");
        for (const ParameterSyntax& parameter : node->parameters) {
            line(output, depth + 2, std::string(parameter.copyParameter ? "CopyParam " : "Param ") + quote(parameter.name), parameter.sourceSpan);
            printType(output, parameter.type, depth + 3);
        }
        line(output, depth + 1, "Body");
        printBlock(output, node->body, depth + 2);
    } else if (const auto* node = dynamic_cast<const ConstructorDeclarationAst*>(&statement)) {
        line(output, depth, "ConstructorDecl " + quote(node->name), node->sourceSpan);
        line(output, depth + 1, "Parameters");
        for (const ParameterSyntax& parameter : node->parameters) {
            line(output, depth + 2, std::string(parameter.copyParameter ? "CopyParam " : "Param ") + quote(parameter.name), parameter.sourceSpan);
            printType(output, parameter.type, depth + 3);
        }
        line(output, depth + 1, "Body");
        printBlock(output, node->body, depth + 2);
    } else if (const auto* node = dynamic_cast<const AggregateDeclarationAst*>(&statement)) {
        line(output, depth, std::string(node->isClass ? "ClassDecl " : "StructDecl ") + quote(node->name), node->sourceSpan);
        printBlock(output, node->body, depth + 1);
    } else {
        line(output, depth, "UnknownStmt", statement.sourceSpan);
    }
}

void printBlock(std::ostream& output, const BlockAst& block, int depth) {
    line(output, depth, "Block", block.sourceSpan);
    for (const auto& statement : block.statements) printStatement(output, *statement, depth + 1);
}
}

void printProgramAst(std::ostream& output, const ProgramAst& program) {
    line(output, 0, "Program", program.sourceSpan);
    printBlock(output, program.body, 1);
}
