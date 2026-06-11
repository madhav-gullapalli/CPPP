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

bool CPPPToBool(bool value) { return value; }
bool CPPPToBool(int value) { return value != 0; }
bool CPPPToBool(long long value) { return value != 0; }
bool CPPPToBool(long double value) { return value != 0.0L && !isnan(value); }
bool CPPPToBool(const CPPPChar& value) { return value.value != '\0'; }

bool CPPPInputBool() { bool value; cin >> value; return value; }
CPPPChar CPPPInputChar() { CPPPChar value; cin >> value; return value; }
long long CPPPInputInt() { long long value; cin >> value; return value; }
long double CPPPInputFloat() { long double value; cin >> value; return value; }

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    long long n = CPPPInputInt();
    long long a = (100000 / ((n - 2)));
    if ((CPPPToBool((CPPPToBool((n % 2)) || CPPPToBool((n == 2)))) || CPPPToBool((a == 1000)))) {
        cout << "No" << '\n';
    }
    else {
        cout << "Yes" << '\n';
    }
    return 0;
}
