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

CPPPChar CPPPInputChar() { CPPPChar value; cin >> value; return value; }

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    CPPPChar c = CPPPInputChar();
    cout << c << '\n';
    return 0;
}
