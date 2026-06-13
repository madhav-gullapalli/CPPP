#include <bits/stdc++.h>
using namespace std;

bool CPPPToBoolInt(long long value) { return value != 0; }

long long CPPPInputInt() { long long value; cin >> value; return value; }

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    long long x = CPPPInputInt();
    bool ok = CPPPToBoolInt(x);
    cout << ok << '\n';
    return 0;
}
