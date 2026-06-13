#include <bits/stdc++.h>
using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    vector<long long> l = vector<long long>{1, 2, 3, 4};
    long long x = ([&]() { auto __cppp_removed = (l).back(); (l).pop_back(); return __cppp_removed; }());
    cout << x << '\n';
    return 0;
}
