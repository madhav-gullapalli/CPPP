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

    try {
    long long n = CPPPInputInt();
    long long a = ([&](long long __cppp_left, long long __cppp_right) { if (__cppp_right == 0) { throw runtime_error("2:15:division by zero"); } if (__cppp_left == LLONG_MIN && __cppp_right == -1) { throw runtime_error("2:15:integer overflow"); } return __cppp_left / __cppp_right; })(100000, (([&](long long __cppp_left, long long __cppp_right) { if ((__cppp_right < 0 && __cppp_left > LLONG_MAX + __cppp_right) || (__cppp_right > 0 && __cppp_left < LLONG_MIN + __cppp_right)) { throw runtime_error("2:18:integer overflow"); } return __cppp_left - __cppp_right; })(n, 2)));
    if ((CPPPToBool((CPPPToBool(([&](long long __cppp_left, long long __cppp_right) { if (__cppp_right == 0) { throw runtime_error("3:5:modulo by zero"); } if (__cppp_left == LLONG_MIN && __cppp_right == -1) { throw runtime_error("3:5:integer overflow"); } return __cppp_left % __cppp_right; })(n, 2)) || CPPPToBool((n == 2)))) || CPPPToBool((a == 1000)))) {
        cout << "NO" << '\n';
    }
    else {
        cout << "YES" << '\n';
    }
    return 0;
    } catch (const runtime_error& __cppp_error) {
        string __cppp_message = __cppp_error.what();
        size_t __cppp_first = __cppp_message.find(':');
        size_t __cppp_second = __cppp_message.find(':', __cppp_first + 1);
        if (__cppp_first != string::npos && __cppp_second != string::npos) {
            cout << "in.cppp:" << __cppp_message.substr(0, __cppp_first) << ":" << __cppp_message.substr(__cppp_first + 1, __cppp_second - __cppp_first - 1) << ": error: runtime error: " << __cppp_message.substr(__cppp_second + 1) << '\n';
        } else {
            cout << "CP++ runtime error: " << __cppp_message << '\n';
        }
        return 1;
    }
}
