# CP++

CP++ is a language for competitive programming. It keeps C++ performance while
providing shorter syntax for common contest tasks: containers, input, printing,
loops, ranges, and functions.

## Get started

You need a C++17-capable `g++` compiler and `make` (or `mingw32-make` on some
Windows MinGW installations).

Build the CP++ compiler:

```sh
make
```

On Windows, use:

```sh
mingw32-make
```

Write CP++ in a `.cppp` file, then choose a workflow:

```sh
make transpile INPUT=solution.cppp  # create solution.cpp
make compile INPUT=solution.cppp    # create a native executable
make run INPUT=solution.cppp        # compile and run it
make submit INPUT=solution.cppp     # create compact submission C++
```

`--run` keeps CP++ runtime diagnostics enabled. `--submit` produces compact
C++ suitable for contest submission; add `READABLE=1` to inspect the pruned
output without minification.

## A Small Example

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

## Learn CP++

Read the [CP++ language reference](cppp_language.md) for syntax, types,
containers, input/output, functions, and control flow. The executable examples
in `correct.txt` and diagnostics in `errors.txt` define the supported surface.

Compiler architecture, inspection modes, and development/test workflows live
in [docs/](docs/), starting with [the compiler pipeline](docs/compiler_pipeline.md).
