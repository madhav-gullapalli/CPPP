#include <bits/stdc++.h>
using namespace std;

long long CPPPInputInt() { long long value; cin >> value; return value; }

template <typename Reader>
auto CPPPInputList(long long count, Reader reader) {
    using Value = decltype(reader());
    vector<Value> values;
    for (long long i = 0; i < count; ++i) {
        values.push_back(reader());
    }
    return values;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    long long t = CPPPInputInt();
    for (int _0 = 0; _0 < static_cast<long long>(t); ++_0) {
        long long n = CPPPInputInt();
        vector<long long> A = CPPPInputList(n, [&]() { return CPPPInputInt(); });
        vector<long long> B = CPPPInputList(n, [&]() { return CPPPInputInt(); });
        long long less = 0, more = 0;
        for (long long i = 0; (i < n); (i++)) {
            if (((A[i]) < (B[i]))) {
                less = (less + (B[i]));
                less = (less - (A[i]));
            }
            else {
                more = (more + (A[i]));
                more = (more - (B[i]));
            }
        }
        cout << (more + 1) << '\n';
    }
    return 0;
}
