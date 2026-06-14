#include <bits/stdc++.h>
using namespace std;

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

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    vector<vector<long long>> A = vector<vector<long long>>{vector<long long>{1}, vector<long long>{2, 3}, vector<long long>{4, 5, 6}};
    for (vector<long long> x : A) {
        CPPPPrintValue(cout, x); cout << '\n';
    }
    return 0;
}
