/*
 * typesCppp.cpp
 *
 * Provides runtime helper definitions and type support for generated C++ output.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "typesCppp.h"

#include "listsCppp.h"

#include <map>
#include <set>
#include <sstream>

namespace {
std::set<std::string>& runtimeRequirementSet() {
    static std::set<std::string> helpers;
    return helpers;
}
}

// runtimeHelpers provides runtime support for generated code.
std::vector<RuntimeHelper> runtimeHelpers() {
    std::vector<RuntimeHelper> helpers = {
        {
            "CPPPCharCore",
            {
                "struct CPPPChar {",
                "    char value = '\\0';",
                "    CPPPChar() = default;",
                "    CPPPChar(char initialValue) : value(initialValue) {}",
                "    operator char() const { return value; }",
                "};",
                "",
                "ostream& operator<<(ostream& output, const CPPPChar& value) {",
                "    if (value.value == '\\0') {",
                "        return output << 0;",
                "    }",
                "",
                "    return output << value.value;",
                "}",
                "",
                "istream& operator>>(istream& input, CPPPChar& value) {",
                "    char ch;",
                "    input >> ch;",
                "    value = CPPPChar(ch);",
                "    return input;",
                "}",
                "",
                "CPPPChar& operator++(CPPPChar& value) { ++value.value; return value; }",
                "CPPPChar operator++(CPPPChar& value, int) { CPPPChar old = value; ++value; return old; }",
                "CPPPChar& operator--(CPPPChar& value) { --value.value; return value; }",
                "CPPPChar operator--(CPPPChar& value, int) { CPPPChar old = value; --value; return old; }",
                ""
            },
            {},
            {"CPPPChar"}
        },
        {
            "CPPPToBoolBool",
            {
                "bool CPPPToBoolBool(bool value) { return value; }",
                ""
            },
            {},
            {"CPPPToBoolBool("}
        },
        {
            "CPPPToBoolInt",
            {
                "bool CPPPToBoolInt(long long value) { return value != 0; }",
                ""
            },
            {},
            {"CPPPToBoolInt("}
        },
        {
            "CPPPToBoolFloat",
            {
                "bool CPPPToBoolFloat(long double value) { return value != 0.0L && !isnan(value); }",
                ""
            },
            {},
            {"CPPPToBoolFloat("}
        },
        {
            "CPPPToBoolChar",
            {
                "bool CPPPToBoolChar(const CPPPChar& value) { return value.value != '\\0'; }",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPToBoolChar("}
        },
        {
            "CPPPToBoolFallback",
            {
                "bool CPPPToBool(bool value) { return value; }",
                "bool CPPPToBool(int value) { return value != 0; }",
                "bool CPPPToBool(long long value) { return value != 0; }",
                "bool CPPPToBool(long double value) { return value != 0.0L && !isnan(value); }",
                "bool CPPPToBool(const CPPPChar& value) { return value.value != '\\0'; }",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPToBool("}
        },
        {
            "CPPPInputBool",
            {
                "bool CPPPInputBool() { bool value; cin >> value; return value; }",
                ""
            },
            {},
            {"CPPPInputBool("}
        },
        {
            "CPPPInputChar",
            {
                "CPPPChar CPPPInputChar() { CPPPChar value; cin >> value; return value; }",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPInputChar("}
        },
        {
            "CPPPInputInt",
            {
                "long long CPPPInputInt() { long long value; cin >> value; return value; }",
                ""
            },
            {},
            {"CPPPInputInt("}
        },
        {
            "CPPPInputFloat",
            {
                "long double CPPPInputFloat() { long double value; cin >> value; return value; }",
                ""
            },
            {},
            {"CPPPInputFloat("}
        },
        {
            "CPPPInputString",
            {
                "vector<CPPPChar> CPPPInputString() {",
                "    string value;",
                "    cin >> value;",
                "    vector<CPPPChar> result;",
                "    result.reserve(value.size());",
                "    for (char ch : value) {",
                "        result.push_back(CPPPChar(ch));",
                "    }",
                "    return result;",
                "}",
                "",
                "vector<CPPPChar> CPPPInputString(long long count) {",
                "    string value;",
                "    value.reserve(static_cast<size_t>(max(0LL, count)));",
                "    cin >> ws;",
                "    for (long long i = 0; i < count; ++i) {",
                "        char ch = '\\0';",
                "        if (!cin.get(ch)) {",
                "            break;",
                "        }",
                "        value.push_back(ch);",
                "    }",
                "    vector<CPPPChar> result;",
                "    result.reserve(value.size());",
                "    for (char ch : value) {",
                "        result.push_back(CPPPChar(ch));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPInputString("}
        },
        {
            "CPPPInputList",
            {
                "template <typename Reader>",
                "auto CPPPInputList(long long count, Reader reader) {",
                "    using Value = decltype(reader());",
                "    vector<Value> values;",
                "    for (long long i = 0; i < count; ++i) {",
                "        values.push_back(reader());",
                "    }",
                "    return values;",
                "}",
                ""
            },
            {},
            {"CPPPInputList("}
        },
        {
            "CPPPInputListLine",
            {
                "template <typename T>",
                "vector<T> CPPPInputListLine() {",
                "    string line;",
                "    getline(cin >> ws, line);",
                "    istringstream stream(line);",
                "    vector<T> values;",
                "    T value;",
                "    while (stream >> value) {",
                "        values.push_back(value);",
                "    }",
                "    return values;",
                "}",
                ""
            },
            {},
            {"CPPPInputListLine<"}
        },
        {
            "CPPPStringLiteral",
            {
                "vector<CPPPChar> CPPPStringLiteral(const string& value) {",
                "    vector<CPPPChar> result;",
                "    result.reserve(value.size());",
                "    for (char ch : value) {",
                "        result.push_back(CPPPChar(ch));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPStringLiteral("}
        },
        {
            "CPPPPrintValueString",
            {
                "void CPPPPrintValue(ostream& output, const vector<CPPPChar>& value) {",
                "    for (const CPPPChar& ch : value) {",
                "        output << ch.value;",
                "    }",
                "}",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPPrintValueString("}
        },
        {
            "CPPPPrintValue",
            {
                "template <typename T>",
                "void CPPPPrintValue(ostream& output, const T& value) {",
                "    output << value;",
                "}",
                "",
                "template <typename T>",
                "void CPPPPrintValue(ostream& output, const vector<T>& values) {",
                "    output << '[';",
                "    for (size_t i = 0; i < values.size(); ++i) {",
                "        if (i > 0) {",
                "            output << \", \";",
                "        }",
                "        CPPPPrintValue(output, values[i]);",
                "    }",
                "    output << ']';",
                "}",
                ""
            },
            {},
            {"CPPPPrintValue("}
        },
        {
            "CPPPPrintDelimited",
            {
                "void CPPPPrintDelimiter(ostream& output, const vector<CPPPChar>& value) {",
                "    CPPPPrintValue(output, value);",
                "}",
                "",
                "template <typename T>",
                "void CPPPPrintDelimiter(ostream& output, const T& value) {",
                "    CPPPPrintValue(output, value);",
                "}",
                "",
                "template <typename T, typename Delimiter>",
                "void CPPPPrintDelimited(ostream& output, const vector<T>& values, const Delimiter& delimiter) {",
                "    for (size_t i = 0; i < values.size(); ++i) {",
                "        if (i > 0) {",
                "            CPPPPrintDelimiter(output, delimiter);",
                "        }",
                "        CPPPPrintValue(output, values[i]);",
                "    }",
                "}",
                ""
            },
            {"CPPPCharCore", "CPPPPrintValue"},
            {"CPPPPrintDelimited("}
        }
    };

    const std::vector<RuntimeHelper> listHelpers = listRuntimeHelpers();
    helpers.insert(helpers.end(), listHelpers.begin(), listHelpers.end());
    return helpers;
}

// typeSupportPreamble implements the typeSupportPreamble behavior for the typesCppp.cpp module.
std::vector<std::string> typeSupportPreamble() {
    std::vector<std::string> preamble;
    for (const RuntimeHelper& helper : runtimeHelpers()) {
        preamble.insert(preamble.end(), helper.code.begin(), helper.code.end());
    }
    return preamble;
}

void clearRequiredRuntimeHelpers() {
    runtimeRequirementSet().clear();
}

void requireRuntimeHelper(const std::string& helperName) {
    runtimeRequirementSet().insert(helperName);
}

const std::set<std::string>& requiredRuntimeHelpers() {
    return runtimeRequirementSet();
}

// typeSupportPreambleForSubmit implements the typeSupportPreambleForSubmit behavior for the typesCppp.cpp module.
std::vector<std::string> typeSupportPreambleForSubmit(const std::set<std::string>& requiredHelpers) {
    const std::vector<RuntimeHelper> helpers = runtimeHelpers();
    std::map<std::string, RuntimeHelper> helpersByName;
    for (const RuntimeHelper& helper : helpers) {
        helpersByName[helper.name] = helper;
    }

    std::set<std::string> resolvedHelpers = requiredHelpers;
// worklist implements the worklist behavior for the typesCppp.cpp module.
    std::vector<std::string> worklist(requiredHelpers.begin(), requiredHelpers.end());

    for (size_t i = 0; i < worklist.size(); ++i) {
        const RuntimeHelper& helper = helpersByName.at(worklist[i]);
        for (const std::string& dep : helper.deps) {
            if (resolvedHelpers.insert(dep).second) {
                worklist.push_back(dep);
            }
        }
    }

    std::vector<std::string> preamble;
    for (const RuntimeHelper& helper : helpers) {
        if (resolvedHelpers.count(helper.name) == 0) {
            continue;
        }

        preamble.insert(preamble.end(), helper.code.begin(), helper.code.end());
    }

    return preamble;
}
