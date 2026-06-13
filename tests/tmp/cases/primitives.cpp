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

    long long a = 3;
    long double b = 2.5L;
    CPPPChar c = CPPPChar('x');
    bool ok = true;
    cout << a << '\n';
    cout << b << '\n';
    cout << c << '\n';
    cout << ok << '\n';
    return 0;
}
