/* Reserved CP++ identifier spellings shared by parsing and semantic analysis. */

#pragma once

#include <array>
#include <string>

// reservedKeywords is deliberately one complete list: when a word gains
// syntactic meaning, add it here so it cannot silently become a declaration.
inline const std::array<const char*, 52>& reservedKeywords() {
    static const std::array<const char*, 52> keywords = {
        // Control flow and declarations.
        "if", "else", "while", "for", "rep", "nobreak", "return", "break", "continue", "struct", "class",
        // Types and declaration modifiers.
        "var", "bool", "char", "int", "float", "string", "range", "void",
        "List", "Stack", "Queue", "Deque", "Heap", "Set", "Map", "Pair", "copy", "deep",
        // Removed type spellings remain reserved so they keep their useful diagnostic.
        "bigint", "Bigint", "bigfloat", "BigFloat",
        // Literal and expression syntax.
        "true", "false", "NULL", "in", "len", "min", "max", "sum", "abs", "input",
        // Built-in statement and option names.
        "print", "describe", "end", "delim", "flush",
        // Contextual built-ins.
        "compare", "default", "greater", "self"
    };
    return keywords;
}

inline bool isReservedKeyword(const std::string& spelling) {
    for (const char* keyword : reservedKeywords()) {
        if (spelling == keyword) return true;
    }
    return false;
}
