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

template <typename T>
const T& CPPPListAt(const vector<T>& list, long long index, int line, int column) {
    if (index < 0 || index >= static_cast<long long>(list.size())) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid list index");
    }
    return list[static_cast<typename vector<T>::difference_type>(index)];
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    try {
    long long a = 2;
    long long b = 3;
    long long c = 4;
    vector<vector<long long>> grid = vector<vector<long long>>{vector<long long>{10, 20}, vector<long long>{30, 40}};
    cout << ([&](long long __cppp_left, long long __cppp_right) { if ((__cppp_right > 0 && __cppp_left > LLONG_MAX - __cppp_right) || (__cppp_right < 0 && __cppp_left < LLONG_MIN - __cppp_right)) { throw runtime_error("6:9:integer overflow"); } return __cppp_left + __cppp_right; })(a, ([&](long long __cppp_left, long long __cppp_right) { __int128 __cppp_product = static_cast<__int128>(__cppp_left) * static_cast<__int128>(__cppp_right); if (__cppp_product > LLONG_MAX || __cppp_product < LLONG_MIN) { throw runtime_error("6:13:integer overflow"); } return static_cast<long long>(__cppp_product); })(b, c)) << ' ' << ([&](long long __cppp_left, long long __cppp_right) { __int128 __cppp_product = static_cast<__int128>(__cppp_left) * static_cast<__int128>(__cppp_right); if (__cppp_product > LLONG_MAX || __cppp_product < LLONG_MIN) { throw runtime_error("6:26:integer overflow"); } return static_cast<long long>(__cppp_product); })(([&](long long __cppp_left, long long __cppp_right) { if ((__cppp_right > 0 && __cppp_left > LLONG_MAX - __cppp_right) || (__cppp_right < 0 && __cppp_left < LLONG_MIN - __cppp_right)) { throw runtime_error("6:21:integer overflow"); } return __cppp_left + __cppp_right; })(a, b), c) << ' ' << ([&](long long __cppp_left, long long __cppp_right) { if ((__cppp_right > 0 && __cppp_left > LLONG_MAX - __cppp_right) || (__cppp_right < 0 && __cppp_left < LLONG_MIN - __cppp_right)) { throw runtime_error("6:37:integer overflow"); } return __cppp_left + __cppp_right; })(([&](long long __cppp_left, long long __cppp_right) { if ((__cppp_right > 0 && __cppp_left > LLONG_MAX - __cppp_right) || (__cppp_right < 0 && __cppp_left < LLONG_MIN - __cppp_right)) { throw runtime_error("6:33:integer overflow"); } return __cppp_left + __cppp_right; })(a, b), c) << ' ' << CPPPListAt(CPPPListAt(grid, 1, 6, 46), 0, 6, 49) << ' ' << static_cast<long long>((vector<vector<long long>>{vector<long long>{1}, vector<long long>{2, 3}}).size()) << '\n';
    return 0;
    } catch (const runtime_error& __cppp_error) {
        string __cppp_message = __cppp_error.what();
        size_t __cppp_first = __cppp_message.find(':');
        size_t __cppp_second = __cppp_message.find(':', __cppp_first + 1);
        if (__cppp_first != string::npos && __cppp_second != string::npos) {
            cout << "tests/tmp/cases/expression_behavior.cppp:" << __cppp_message.substr(0, __cppp_first) << ":" << __cppp_message.substr(__cppp_first + 1, __cppp_second - __cppp_first - 1) << ": error: runtime error: " << __cppp_message.substr(__cppp_second + 1) << '\n';
        } else {
            cout << "CP++ runtime error: " << __cppp_message << '\n';
        }
        return 1;
    }
}
