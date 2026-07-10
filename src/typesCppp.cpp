/*
 * typesCppp.cpp
 *
 * Provides runtime helper definitions and type support for generated C++ output.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "typesCppp.h"

#include "listsCppp.h"

#include <cctype>
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
            "CPPPRangeType",
            {
                "struct CPPPRange {",
                "    struct Iterator {",
                "        long long current = 0;",
                "        long long stop = 0;",
                "        long long step = 1;",
                "",
                "        long long operator*() const { return current; }",
                "        Iterator& operator++() { current += step; return *this; }",
                "        bool operator!=(const Iterator&) const {",
                "            return step > 0 ? current < stop : current > stop;",
                "        }",
                "    };",
                "",
                "    long long start = 0;",
                "    long long stop = 0;",
                "    long long step = 1;",
                "",
                "    CPPPRange() = default;",
                "    CPPPRange(long long startValue, long long stopValue, long long stepValue) :",
                "        start(startValue),",
                "        stop(stopValue),",
                "        step(stepValue) {}",
                "",
                "    Iterator begin() const { return {start, stop, step}; }",
                "    Iterator end() const { return {stop, stop, step}; }",
                "    bool empty() const { return step > 0 ? start >= stop : start <= stop; }",
                "    bool contains(long long value) const {",
                "        if (empty()) {",
                "            return false;",
                "        }",
                "        if (step > 0) {",
                "            if (value < start || value >= stop) {",
                "                return false;",
                "            }",
                "        } else if (value > start || value <= stop) {",
                "            return false;",
                "        }",
                "        const long long distance = value >= start ? value - start : start - value;",
                "        const long long stride = step >= 0 ? step : -step;",
                "        return stride != 0 && distance % stride == 0;",
                "    }",
                "};",
                ""
            },
            {},
            {"CPPPRange"}
        },
        {
            "CPPPRangeMakeStop",
            {
                "CPPPRange CPPPMakeRange(long long stop) {",
                "    return stop >= 0 ? CPPPRange(0, stop, 1) : CPPPRange(0, stop, -1);",
                "}",
                ""
            },
            {"CPPPRangeType"},
            {"CPPPMakeRange("}
        },
        {
            "CPPPRangeMakeBounds",
            {
                "CPPPRange CPPPMakeRange(long long start, long long stop) {",
                "    return start <= stop ? CPPPRange(start, stop, 1) : CPPPRange(start, stop, -1);",
                "}",
                ""
            },
            {"CPPPRangeType"},
            {"CPPPMakeRange("}
        },
        {
            "CPPPRangeMakeStep",
            {
                "CPPPRange CPPPMakeRange(long long start, long long stop, long long step, int line, int column) {",
                "    if (step == 0) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":range step cannot be zero\");",
                "    }",
                "    const long long stride = step >= 0 ? step : -step;",
                "    return start <= stop ? CPPPRange(start, stop, stride) : CPPPRange(start, stop, -stride);",
                "}",
                ""
            },
            {"CPPPRangeType"},
            {"CPPPMakeRange("}
        },
        {
            "CPPPRangeToList",
            {
                "vector<long long> CPPPRangeToList(const CPPPRange& range) {",
                "    vector<long long> values;",
                "    for (long long value : range) {",
                "        values.push_back(value);",
                "    }",
                "    return values;",
                "}",
                ""
            },
            {"CPPPRangeType"},
            {"CPPPRangeToList("}
        },
        {
            "CPPPRangeToSet",
            {
                "set<long long> CPPPRangeToSet(const CPPPRange& range) {",
                "    set<long long> values;",
                "    for (long long value : range) {",
                "        values.insert(value);",
                "    }",
                "    return values;",
                "}",
                ""
            },
            {"CPPPRangeType"},
            {"CPPPRangeToSet("}
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
            "CPPPStringFromStd",
            {
                "vector<CPPPChar> CPPPStringFromStd(const string& value) {",
                "    vector<CPPPChar> result;",
                "    result.reserve(value.size());",
                "    for (char ch : value) {",
                "        result.push_back(CPPPChar(ch));",
                "    }",
                "    return result;",
                "}",
                "",
                "string CPPPStdStringFromChars(const vector<CPPPChar>& value) {",
                "    string result;",
                "    result.reserve(value.size());",
                "    for (const CPPPChar& ch : value) {",
                "        result.push_back(ch.value);",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPStringFromStd(", "CPPPStdStringFromChars("}
        },
        {
            "CPPPToStringBool",
            {
                "vector<CPPPChar> CPPPToStringBool(bool value) {",
                "    return CPPPStringFromStd(value ? \"1\" : \"0\");",
                "}",
                ""
            },
            {"CPPPStringFromStd"},
            {"CPPPToStringBool("}
        },
        {
            "CPPPToStringChar",
            {
                "vector<CPPPChar> CPPPToStringChar(const CPPPChar& value) {",
                "    return {value};",
                "}",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPToStringChar("}
        },
        {
            "CPPPToStringInt",
            {
                "vector<CPPPChar> CPPPToStringInt(long long value) {",
                "    return CPPPStringFromStd(to_string(value));",
                "}",
                ""
            },
            {"CPPPStringFromStd"},
            {"CPPPToStringInt("}
        },
        {
            "CPPPToStringFloat",
            {
                "vector<CPPPChar> CPPPToStringFloat(long double value) {",
                "    ostringstream stream;",
                "    stream << value;",
                "    return CPPPStringFromStd(stream.str());",
                "}",
                ""
            },
            {"CPPPStringFromStd"},
            {"CPPPToStringFloat("}
        },
        {
            "CPPPStringToBool",
            {
                "bool CPPPStringToBool(const vector<CPPPChar>& value, int line, int column) {",
                "    const string text = CPPPStdStringFromChars(value);",
                "    if (text == \"1\" || text == \"true\") {",
                "        return true;",
                "    }",
                "    if (text == \"0\" || text == \"false\") {",
                "        return false;",
                "    }",
                "    throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid bool string\");",
                "}",
                ""
            },
            {"CPPPStringFromStd"},
            {"CPPPStringToBool("}
        },
        {
            "CPPPStringToChar",
            {
                "CPPPChar CPPPStringToChar(const vector<CPPPChar>& value, int line, int column) {",
                "    if (value.size() != 1) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid char string\");",
                "    }",
                "    return value[0];",
                "}",
                ""
            },
            {"CPPPCharCore"},
            {"CPPPStringToChar("}
        },
        {
            "CPPPStringToInt",
            {
                "long long CPPPStringToInt(const vector<CPPPChar>& value, int line, int column) {",
                "    const string text = CPPPStdStringFromChars(value);",
                "    if (text.empty()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid int string\");",
                "    }",
                "    size_t index = 0;",
                "    if (text[index] == '+' || text[index] == '-') {",
                "        ++index;",
                "    }",
                "    if (index >= text.size()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid int string\");",
                "    }",
                "    for (size_t i = index; i < text.size(); ++i) {",
                "        if (!isdigit(static_cast<unsigned char>(text[i]))) {",
                "            throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid int string\");",
                "        }",
                "    }",
                "    try {",
                "        return stoll(text);",
                "    } catch (...) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid int string\");",
                "    }",
                "}",
                ""
            },
            {"CPPPStringFromStd"},
            {"CPPPStringToInt("}
        },
        {
            "CPPPStringToFloat",
            {
                "long double CPPPStringToFloat(const vector<CPPPChar>& value, int line, int column) {",
                "    const string text = CPPPStdStringFromChars(value);",
                "    if (text.empty()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid float string\");",
                "    }",
                "    size_t index = 0;",
                "    if (text[index] == '+' || text[index] == '-') {",
                "        ++index;",
                "    }",
                "    const size_t wholeStart = index;",
                "    while (index < text.size() && isdigit(static_cast<unsigned char>(text[index]))) {",
                "        ++index;",
                "    }",
                "    if (wholeStart == index) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid float string\");",
                "    }",
                "    if (index < text.size() && text[index] == '.') {",
                "        ++index;",
                "        const size_t fractionalStart = index;",
                "        while (index < text.size() && isdigit(static_cast<unsigned char>(text[index]))) {",
                "            ++index;",
                "        }",
                "        if (fractionalStart == index) {",
                "            throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid float string\");",
                "        }",
                "    }",
                "    if (index != text.size()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid float string\");",
                "    }",
                "    try {",
                "        return stold(text);",
                "    } catch (...) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid float string\");",
                "    }",
                "}",
                ""
            },
            {"CPPPStringFromStd"},
            {"CPPPStringToFloat("}
        },
        {
            "CPPPListToSet",
            {
                "template <typename Out, typename In, typename Converter>",
                "set<Out> CPPPListToSet(const vector<In>& values, Converter convert) {",
                "    set<Out> result;",
                "    for (const In& value : values) {",
                "        result.insert(convert(value));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {},
            {"CPPPListToSet<"}
        },
        {
            "CPPPSetToList",
            {
                "template <typename Out, typename In, typename Converter>",
                "vector<Out> CPPPSetToList(const set<In>& values, Converter convert) {",
                "    vector<Out> result;",
                "    result.reserve(values.size());",
                "    for (const In& value : values) {",
                "        result.push_back(convert(value));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {},
            {"CPPPSetToList<"}
        },
        {
            "CPPPListToMap",
            {
                "template <typename KOut, typename VOut, typename KIn, typename VIn, typename KeyConverter, typename ValueConverter>",
                "map<KOut, VOut> CPPPListToMap(const vector<pair<KIn, VIn>>& values, KeyConverter keyConvert, ValueConverter valueConvert) {",
                "    map<KOut, VOut> result;",
                "    for (const pair<KIn, VIn>& entry : values) {",
                "        result[keyConvert(entry.first)] = valueConvert(entry.second);",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {},
            {"CPPPListToMap<"}
        },
        {
            "CPPPMapToList",
            {
                "template <typename KOut, typename VOut, typename KIn, typename VIn, typename KeyConverter, typename ValueConverter>",
                "vector<pair<KOut, VOut>> CPPPMapToList(const map<KIn, VIn>& values, KeyConverter keyConvert, ValueConverter valueConvert) {",
                "    vector<pair<KOut, VOut>> result;",
                "    result.reserve(values.size());",
                "    for (const pair<const KIn, VIn>& entry : values) {",
                "        result.push_back(make_pair(keyConvert(entry.first), valueConvert(entry.second)));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {},
            {"CPPPMapToList<"}
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
                "void CPPPPrintValue(ostream& output, const T& value);",
                "",
                "template <typename A, typename B>",
                "void CPPPPrintValue(ostream& output, const pair<A, B>& value);",
                "",
                "template <typename T>",
                "void CPPPPrintValue(ostream& output, const vector<T>& values);",
                "",
                "template <typename T>",
                "void CPPPPrintValue(ostream& output, const set<T>& values);",
                "",
                "template <typename K, typename V>",
                "void CPPPPrintValue(ostream& output, const map<K, V>& values);",
                "",
                "template <typename A, typename B>",
                "void CPPPPrintValue(ostream& output, const pair<A, B>& value) {",
                "    output << '(';",
                "    CPPPPrintValue(output, value.first);",
                "    output << ',';",
                "    CPPPPrintValue(output, value.second);",
                "    output << ')';",
                "}",
                "",
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
                "",
                "template <typename T>",
                "void CPPPPrintValue(ostream& output, const set<T>& values) {",
                "    output << '{';",
                "    bool first = true;",
                "    for (const auto& value : values) {",
                "        if (!first) {",
                "            output << \", \";",
                "        }",
                "        first = false;",
                "        CPPPPrintValue(output, value);",
                "    }",
                "    output << '}';",
                "}",
                "",
                "template <typename K, typename V>",
                "void CPPPPrintValue(ostream& output, const map<K, V>& values) {",
                "    output << '{';",
                "    bool first = true;",
                "    for (const auto& entry : values) {",
                "        if (!first) {",
                "            output << \", \";",
                "        }",
                "        first = false;",
                "        CPPPPrintValue(output, entry.first);",
                "        output << ':';",
                "        CPPPPrintValue(output, entry.second);",
                "    }",
                "    output << '}';",
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
                "",
                "template <typename T, typename Delimiter>",
                "void CPPPPrintDelimited(ostream& output, const set<T>& values, const Delimiter& delimiter) {",
                "    bool first = true;",
                "    for (const auto& value : values) {",
                "        if (!first) {",
                "            CPPPPrintDelimiter(output, delimiter);",
                "        }",
                "        first = false;",
                "        CPPPPrintValue(output, value);",
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
