#include <bits/stdc++.h>
using namespace std;

using CPPPBigFloat = long double;

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

struct CPPPBigInt {
    bool negative = false;
    string digits = "0";

    CPPPBigInt() = default;
    CPPPBigInt(long long value) {
        if (value < 0) {
            negative = true;
            unsigned long long magnitude = static_cast<unsigned long long>(-(value + 1)) + 1;
            digits = to_string(magnitude);
        } else {
            digits = to_string(value);
        }
    }
    CPPPBigInt(const string& value) { assign(value); }
    CPPPBigInt(const char* value) { assign(string(value)); }
    explicit operator long long() const {
        long long result = 0;
        for (char digit : digits) { result = result * 10 + (digit - '0'); }
        return negative ? -result : result;
    }
    explicit operator long double() const {
        long double result = 0;
        for (char digit : digits) { result = result * 10 + (digit - '0'); }
        return negative ? -result : result;
    }

    void assign(string value) {
        negative = false;
        if (!value.empty() && (value[0] == '-' || value[0] == '+')) {
            negative = value[0] == '-';
            value = value.substr(1);
        }
        digits = value.empty() ? "0" : value;
        normalize();
    }

    void normalize() {
        size_t first = digits.find_first_not_of('0');
        digits = first == string::npos ? "0" : digits.substr(first);
        if (digits == "0") {
            negative = false;
        }
    }

    static int compareAbs(const CPPPBigInt& left, const CPPPBigInt& right) {
        if (left.digits.size() != right.digits.size()) {
            return left.digits.size() < right.digits.size() ? -1 : 1;
        }
        if (left.digits == right.digits) {
            return 0;
        }
        return left.digits < right.digits ? -1 : 1;
    }

    static string addAbs(string left, string right) {
        string result;
        int carry = 0;
        int i = static_cast<int>(left.size()) - 1;
        int j = static_cast<int>(right.size()) - 1;
        while (i >= 0 || j >= 0 || carry != 0) {
            int sum = carry;
            if (i >= 0) { sum += left[i--] - '0'; }
            if (j >= 0) { sum += right[j--] - '0'; }
            result.push_back(static_cast<char>('0' + (sum % 10)));
            carry = sum / 10;
        }
        reverse(result.begin(), result.end());
        return result;
    }

    static string subAbs(string left, string right) {
        string result;
        int borrow = 0;
        int i = static_cast<int>(left.size()) - 1;
        int j = static_cast<int>(right.size()) - 1;
        while (i >= 0) {
            int digit = (left[i--] - '0') - borrow;
            if (j >= 0) { digit -= right[j--] - '0'; }
            if (digit < 0) {
                digit += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result.push_back(static_cast<char>('0' + digit));
        }
        while (result.size() > 1 && result.back() == '0') { result.pop_back(); }
        reverse(result.begin(), result.end());
        return result;
    }

    static string mulAbs(const string& left, const string& right) {
        vector<int> result(left.size() + right.size(), 0);
        for (int i = static_cast<int>(left.size()) - 1; i >= 0; --i) {
            for (int j = static_cast<int>(right.size()) - 1; j >= 0; --j) {
                int product = (left[i] - '0') * (right[j] - '0') + result[i + j + 1];
                result[i + j + 1] = product % 10;
                result[i + j] += product / 10;
            }
        }
        string text;
        size_t index = 0;
        while (index < result.size() - 1 && result[index] == 0) { ++index; }
        for (; index < result.size(); ++index) { text.push_back(static_cast<char>('0' + result[index])); }
        return text;
    }

    static pair<string, string> divModAbs(const CPPPBigInt& left, const CPPPBigInt& right) {
        if (right.digits == "0") { throw runtime_error("division by zero"); }
        CPPPBigInt remainder;
        string quotient;
        for (char digit : left.digits) {
            remainder.digits.push_back(digit);
            remainder.normalize();
            int q = 0;
            while (compareAbs(remainder, right) >= 0) {
                remainder.digits = subAbs(remainder.digits, right.digits);
                remainder.normalize();
                ++q;
            }
            quotient.push_back(static_cast<char>('0' + q));
        }
        CPPPBigInt cleanQuotient(quotient);
        return {cleanQuotient.digits, remainder.digits};
    }
};

CPPPBigInt operator+(const CPPPBigInt& left, const CPPPBigInt& right) {
    CPPPBigInt result;
    if (left.negative == right.negative) {
        result.negative = left.negative;
        result.digits = CPPPBigInt::addAbs(left.digits, right.digits);
    } else if (CPPPBigInt::compareAbs(left, right) >= 0) {
        result.negative = left.negative;
        result.digits = CPPPBigInt::subAbs(left.digits, right.digits);
    } else {
        result.negative = right.negative;
        result.digits = CPPPBigInt::subAbs(right.digits, left.digits);
    }
    result.normalize();
    return result;
}

CPPPBigInt operator-(const CPPPBigInt& left, const CPPPBigInt& right) {
    CPPPBigInt negated = right;
    if (negated.digits != "0") { negated.negative = !negated.negative; }
    return left + negated;
}

CPPPBigInt operator*(const CPPPBigInt& left, const CPPPBigInt& right) {
    CPPPBigInt result;
    result.negative = left.negative != right.negative;
    result.digits = CPPPBigInt::mulAbs(left.digits, right.digits);
    result.normalize();
    return result;
}

CPPPBigInt operator/(const CPPPBigInt& left, const CPPPBigInt& right) {
    auto parts = CPPPBigInt::divModAbs(left, right);
    CPPPBigInt result(parts.first);
    result.negative = left.negative != right.negative;
    result.normalize();
    return result;
}

CPPPBigInt operator%(const CPPPBigInt& left, const CPPPBigInt& right) {
    auto parts = CPPPBigInt::divModAbs(left, right);
    CPPPBigInt result(parts.second);
    result.negative = left.negative;
    result.normalize();
    return result;
}

CPPPBigInt& operator+=(CPPPBigInt& left, const CPPPBigInt& right) { left = left + right; return left; }
CPPPBigInt& operator-=(CPPPBigInt& left, const CPPPBigInt& right) { left = left - right; return left; }
CPPPBigInt& operator*=(CPPPBigInt& left, const CPPPBigInt& right) { left = left * right; return left; }
CPPPBigInt& operator/=(CPPPBigInt& left, const CPPPBigInt& right) { left = left / right; return left; }
CPPPBigInt& operator%=(CPPPBigInt& left, const CPPPBigInt& right) { left = left % right; return left; }

ostream& operator<<(ostream& output, const CPPPBigInt& value) {
    if (value.negative) { output << '-'; }
    return output << value.digits;
}

istream& operator>>(istream& input, CPPPBigInt& value) {
    string text;
    input >> text;
    value = CPPPBigInt(text);
    return input;
}

bool CPPPInputBool() { bool value; cin >> value; return value; }
CPPPChar CPPPInputChar() { CPPPChar value; cin >> value; return value; }
long long CPPPInputInt() { long long value; cin >> value; return value; }
CPPPBigInt CPPPInputBigInt() { CPPPBigInt value; cin >> value; return value; }
long double CPPPInputFloat() { long double value; cin >> value; return value; }

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    long long x = CPPPInputInt();
    long long y = CPPPInputInt();
    cout << (((x * y)) / 2) << '\n';
    return 0;
}
