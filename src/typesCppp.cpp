#include "typesCppp.h"

#include "listsCppp.h"

#include <map>
#include <set>

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
        }
    };

    const std::vector<RuntimeHelper> listHelpers = listRuntimeHelpers();
    helpers.insert(helpers.end(), listHelpers.begin(), listHelpers.end());
    return helpers;
}

std::vector<std::string> typeSupportPreamble() {
    std::vector<std::string> preamble;
    for (const RuntimeHelper& helper : runtimeHelpers()) {
        preamble.insert(preamble.end(), helper.code.begin(), helper.code.end());
    }
    return preamble;
}

std::vector<std::string> typeSupportPreambleForSubmit(const std::string& generatedProgramText) {
    const std::vector<RuntimeHelper> helpers = runtimeHelpers();
    std::map<std::string, RuntimeHelper> helpersByName;
    for (const RuntimeHelper& helper : helpers) {
        helpersByName[helper.name] = helper;
    }

    std::set<std::string> requiredHelpers;
    std::vector<std::string> worklist;
    for (const RuntimeHelper& helper : helpers) {
        for (const std::string& trigger : helper.triggers) {
            if (generatedProgramText.find(trigger) != std::string::npos) {
                if (requiredHelpers.insert(helper.name).second) {
                    worklist.push_back(helper.name);
                }
                break;
            }
        }
    }

    for (size_t i = 0; i < worklist.size(); ++i) {
        const RuntimeHelper& helper = helpersByName.at(worklist[i]);
        for (const std::string& dep : helper.deps) {
            if (requiredHelpers.insert(dep).second) {
                worklist.push_back(dep);
            }
        }
    }

    std::vector<std::string> preamble;
    for (const RuntimeHelper& helper : helpers) {
        if (requiredHelpers.count(helper.name) == 0) {
            continue;
        }

        preamble.insert(preamble.end(), helper.code.begin(), helper.code.end());
    }

    return preamble;
}
