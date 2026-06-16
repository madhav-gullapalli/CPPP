#include <bits/stdc++.h>
using namespace std;

struct CPPPChar {
    char value = '\0';
    CPPPChar() = default;
    CPPPChar(char initialValue) : value(initialValue) {}
    operator char() const { return value; }
};

ostream& operator<<(ostream& output, const CPPPChar& value) {
    if (value.value == '\0') {
        return output << 0;
    }

    return output << value.value;
}

istream& operator>>(istream& input, CPPPChar& value) {
    char ch;
    input >> ch;
    value = CPPPChar(ch);
    return input;
}

CPPPChar& operator++(CPPPChar& value) { ++value.value; return value; }
CPPPChar operator++(CPPPChar& value, int) { CPPPChar old = value; ++value; return old; }
CPPPChar& operator--(CPPPChar& value) { --value.value; return value; }
CPPPChar operator--(CPPPChar& value, int) { CPPPChar old = value; --value; return old; }

bool CPPPToBoolBool(bool value) { return value; }

bool CPPPToBoolInt(long long value) { return value != 0; }

bool CPPPToBoolFloat(long double value) { return value != 0.0L && !isnan(value); }

bool CPPPToBoolChar(const CPPPChar& value) { return value.value != '\0'; }

bool CPPPToBool(bool value) { return value; }
bool CPPPToBool(int value) { return value != 0; }
bool CPPPToBool(long long value) { return value != 0; }
bool CPPPToBool(long double value) { return value != 0.0L && !isnan(value); }
bool CPPPToBool(const CPPPChar& value) { return value.value != '\0'; }

bool CPPPInputBool() { bool value; cin >> value; return value; }

CPPPChar CPPPInputChar() { CPPPChar value; cin >> value; return value; }

long long CPPPInputInt() { long long value; cin >> value; return value; }

long double CPPPInputFloat() { long double value; cin >> value; return value; }

template <typename Reader>
auto CPPPInputList(long long count, Reader reader) {
    using Value = decltype(reader());
    vector<Value> values;
    for (long long i = 0; i < count; ++i) {
        values.push_back(reader());
    }
    return values;
}

template <typename T>
void CPPPPrintValue(ostream& output, const T& value) {
    output << value;
}

template <typename T>
void CPPPPrintValue(ostream& output, const vector<T>& values) {
    output << '[';
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            output << ", ";
        }
        CPPPPrintValue(output, values[i]);
    }
    output << ']';
}

template <typename T, typename U>
void CPPPListInsert(vector<T>& list, const U& value, long long index, int line, int column) {
    if (index < 0 || index > static_cast<long long>(list.size())) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid list index");
    }
    list.insert(list.begin() + static_cast<typename vector<T>::difference_type>(index), value);
}

template <typename T>
T CPPPListPop(vector<T>& list, int line, int column) {
    if (list.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot remove from empty list");
    }
    T value = list.back();
    list.pop_back();
    return value;
}

template <typename T>
T CPPPListRemoveAt(vector<T>& list, long long index, int line, int column) {
    if (index < 0 || index >= static_cast<long long>(list.size())) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid list index");
    }
    auto iterator = list.begin() + static_cast<typename vector<T>::difference_type>(index);
    T value = *iterator;
    list.erase(iterator);
    return value;
}

template <typename T, typename U>
void CPPPListSet(vector<T>& list, long long index, const U& value, int line, int column) {
    if (index < 0 || index >= static_cast<long long>(list.size())) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid list index");
    }
    list[static_cast<typename vector<T>::difference_type>(index)] = value;
}

template <typename T>
long long CPPPNormalizeListIndex(const vector<T>& list, long long index, int line, int column) {
    if (index < 0) {
        index += static_cast<long long>(list.size());
    }
    if (index < 0 || index >= static_cast<long long>(list.size())) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid list index");
    }
    return index;
}

template <typename T>
typename vector<T>::const_reference CPPPListAt(const vector<T>& list, long long index, int line, int column) {
    return list[static_cast<typename vector<T>::difference_type>(CPPPNormalizeListIndex(list, index, line, column))];
}

template <typename T>
vector<T> CPPPListSlice(const vector<T>& list, long long start, long long end) {
    const long long size = static_cast<long long>(list.size());
    if (start < 0) {
        start += size;
    }
    if (end < 0) {
        end += size;
    }
    start = max(0LL, min(start, size));
    end = max(0LL, min(end, size));
    if (start >= end) {
        return {};
    }
    return vector<T>(
        list.begin() + static_cast<typename vector<T>::difference_type>(start),
        list.begin() + static_cast<typename vector<T>::difference_type>(end)
    );
}

template <typename T>
bool CPPPListContainsSublist(const vector<T>& haystack, const vector<T>& needle) {
    if (needle.empty()) {
        return true;
    }
    if (needle.size() > haystack.size()) {
        return false;
    }
    vector<size_t> prefix(needle.size(), 0);
    for (size_t i = 1, matched = 0; i < needle.size(); ++i) {
        while (matched > 0 && needle[i] != needle[matched]) {
            matched = prefix[matched - 1];
        }
        if (needle[i] == needle[matched]) {
            ++matched;
        }
        prefix[i] = matched;
    }
    for (size_t i = 0, matched = 0; i < haystack.size(); ++i) {
        while (matched > 0 && haystack[i] != needle[matched]) {
            matched = prefix[matched - 1];
        }
        if (haystack[i] == needle[matched]) {
            ++matched;
            if (matched == needle.size()) {
                return true;
            }
        }
    }
    return false;
}

template <typename T>
typename vector<T>::const_reference CPPPListMin(const vector<T>& list, int line, int column) {
    if (list.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take min of empty list");
    }
    return *min_element(list.begin(), list.end());
}

template <typename T>
typename vector<T>::const_reference CPPPListMax(const vector<T>& list, int line, int column) {
    if (list.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take max of empty list");
    }
    return *max_element(list.begin(), list.end());
}

static const vector<string> __cppp_source_lines = {
    "",
    "int a,b = 1,2;\r",
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    try {
    long long a = 1, b = 2;
    return 0;
    } catch (const runtime_error& __cppp_error) {
        string __cppp_message = __cppp_error.what();
        size_t __cppp_first = __cppp_message.find(':');
        size_t __cppp_second = __cppp_message.find(':', __cppp_first + 1);
        if (__cppp_first != string::npos && __cppp_second != string::npos) {
            int __cppp_line = stoi(__cppp_message.substr(0, __cppp_first));
            int __cppp_column = stoi(__cppp_message.substr(__cppp_first + 1, __cppp_second - __cppp_first - 1));
            cout << "in.cppp:" << __cppp_line << ':' << __cppp_column << ": error: runtime error: " << __cppp_message.substr(__cppp_second + 1) << '\n';
            if (__cppp_line >= 0 && __cppp_line < static_cast<int>(__cppp_source_lines.size())) {
                cout << __cppp_source_lines[static_cast<size_t>(__cppp_line)] << '\n';
                cout << string(static_cast<size_t>(max(0, __cppp_column - 1)), ' ') << '^' << '\n';
            }
        } else {
            cout << "CP++ runtime error: " << __cppp_message << '\n';
        }
        return 1;
    }
}
