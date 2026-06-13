#include <bits/stdc++.h>
using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    vector<bool> even = vector<bool>{true, false};
    vector<vector<long long>> grid = vector<vector<long long>>{vector<long long>{1, 2}, vector<long long>{3}};
    if ((!(even).empty())) {
        cout << 1 << '\n';
    }
    if ((!(vector<long long>{1}).empty())) {
        cout << ((grid[0])[1]) << '\n';
    }
    return 0;
}
