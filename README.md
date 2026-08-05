# CP++

CP++ is a competitive-programming-optimized language that transpiles to C++.

The point is not to replace C++ everywhere. The point is to keep the parts that are useful in contests and algorithm work, while cutting down on the parts that are noisy, verbose, or hostile when something goes wrong.

CP++ is aimed at maximal algorithmic expressiveness:

- C++ speed and ecosystem on the back end
- shorter, more direct source on the front end
- friendlier CP++-level diagnostics instead of raw generated-C++ confusion
- built-in support for common data structures and algorithmic primitives
- no attempt to stuff full algorithms into the language itself

That means things like lists, slicing, membership checks, target-typed input, loop forms, small utility operations, and lightweight user-defined functions are part of the language, while the actual algorithm is still yours to write.

Implemented highlights include:

- nested `List<T>` support with indexing, slicing, membership, `find(...)`, and `split(...)`
- target-typed scalar, string, and list `input(...)`
- list-aware `print(...)`, normal string printing, and `delim = ...`
- top-level functions, recursion, and `deep` pass-by-copy container parameters

## Running CP++

Requirements:

- `g++` with C++17 support
- `make`, or `mingw32-make` on some Windows MinGW setups

Build the compiler:

```sh
make
```

On some Windows setups:

```sh
mingw32-make
```

This produces:

```text
build/cppp.exe    on Windows
build/cppp        on Linux/macOS
```

Transpile a CP++ program to C++:

```sh
make transpile INPUT=in.cppp
```

Compile the generated C++:

```sh
make compile INPUT=in.cppp
```

Transpile, compile, and run:

```sh
make run INPUT=in.cppp
```

Generate submit-style output:

```sh
make submit INPUT=in.cppp
```

Direct compiler usage:

```sh
build/cppp --cppp in.cppp
build/cppp --cppp in.cppp --tokens
build/cppp --cppp in.cppp --compile
build/cppp --cppp in.cppp --run
build/cppp --cppp in.cppp --submit
build/cppp --cppp in.cppp --submit --readable
```

Windows equivalents:

```sh
.\build\cppp.exe --cppp in.cppp
.\build\cppp.exe --cppp in.cppp --tokens
.\build\cppp.exe --cppp in.cppp --compile
.\build\cppp.exe --cppp in.cppp --run
.\build\cppp.exe --cppp in.cppp --submit
.\build\cppp.exe --cppp in.cppp --submit --readable
```

`--run` keeps extra runtime checks and CP++-style runtime diagnostics. `--submit` prunes unused support and whitespace-minifies the generated contest C++. Add `--readable` after `--submit` to inspect the same pruned program without compaction. The Make equivalents are `make submit INPUT=in.cppp READABLE=1` and `make subrun INPUT=in.cppp READABLE=1`.

## Local Codegen Freeze

Before changing the AST or lowering pipeline, record the current generated C++
for every example in `correct.txt`:

```sh
make codegen-freeze-record
```

Use the standalone freeze suite for local migration testing:

```sh
make codegen-freeze
```

The baseline is stored in the ignored `tests/codegen_snapshots/` directory. The
check transpiles all `correct.txt` examples and compares the generated C++
byte-for-byte. Record a new baseline only after reviewing an intentional codegen
change. This suite is separate from `make test` and CI, and it does not snapshot
`errors.txt`.

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

## Language Docs

The implemented language surface lives in [cppp_language.md](cppp_language.md).

That file is the readable language reference. Its content is based on the executable documentation in `errors.txt` and `correct.txt`, which are still the best source for exact implemented behavior, but the guide now also calls out the newer function surface, `deep` parameters, list/string printing, `split(...)`, and the recent input forms more directly.
