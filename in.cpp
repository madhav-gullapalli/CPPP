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

struct CPPPBigInt {
    string value = "0";
    CPPPBigInt() = default;
    CPPPBigInt(string initialValue) : value(initialValue) {}
};

ostream& operator<<(ostream& output, const CPPPBigInt& value) {
    return output << value.value;
}

struct CPPPBigFloat {
    string value = "0";
    CPPPBigFloat() = default;
    CPPPBigFloat(string initialValue) : value(initialValue) {}
};

ostream& operator<<(ostream& output, const CPPPBigFloat& value) {
    return output << value.value;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

