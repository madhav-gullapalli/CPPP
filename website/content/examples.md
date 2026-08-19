# See CP++ in motion

These examples use current, tested language forms. They are intentionally small: CP++ removes incidental syntax while leaving the contest algorithm in the foreground.

## Target-typed input

```cpp
int n, target = input();
List<int> values = input(n);
print(target in values);
```

The assignment target controls what `input()` reads. Multiple targets read multiple values; `input(n)` fills a List without a handwritten input loop.

## Multidimensional input and compact loops

```cpp
int rows, cols = input();
List<List<int>> grid = input(rows, cols);

rep(rows) {
    int value = input();
    print(value);
}
```

`rep(count)` repeats a block. Nested List input reads the requested row-major shape.

## Source-located development errors

```cpp
Heap<int> values;
print(values.top());
```

Checked run mode catches the empty-container operation and reports `cannot take top of empty Heap` at the originating CP++ line. Submission mode removes this development-only diagnostic path.

## Search, slicing, and splitting

```cpp
string text = input();
print(text[1:-1]);
print(text.find("aba"));
print(text.split('-'));
```

Strings share List-style indexing and slicing. `find` returns every matching position, and `split` returns the pieces.

## Ordered containers

```cpp
Set<int> seen = {4, 1, 4, 2};
Map<int, int> counts;

for (int value in seen) {
    counts[value] += 1;
}
print(seen, counts);
```

Sets and Maps iterate in comparator order. Membership, insertion, removal, and lookup use logarithmic ordered-container operations.

## The original graph example

This is the repository’s original README example, preserved as a fuller view of ordinary CP++.

```cpp
void dfs(List<List<int>> adj, List<bool> vis, int u){
    vis[u] = true;
    print(u, end = " ");
    for (int v in adj[u]) {
        if (!vis[v]) {
            dfs(adj, vis, v);
        }
    }
}

int n = input();
List<List<int>> edges = input(n - 1, 2);
List<List<int>> adj = [];
rep(n) {
    adj.add([]);
}
for (int i = 0; i < len(edges); i++) {
    int a = edges[i][0];
    int b = edges[i][1];
    adj[a].add(b);
    adj[b].add(a);
}
for (int i = 0; i < n; i++) {
    adj[i].sort();
}
List<bool> vis = [];
rep(n) {
    vis.add(false);
}
dfs(adj, vis, 0);
print();
```

Continue with the guided [Learn CP++](/learn/basics/) path or jump straight to the [Quick Reference](/reference/quick/).
