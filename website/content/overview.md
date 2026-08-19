## Why CP++?

Competitive programming rewards the algorithm, but C++ often makes you spend time on everything around it: repetitive input loops, output formatting, container ceremony, and hard-to-read crash reports. CP++ shortens that incidental work while keeping the algorithm visible.

It does not make graph theory, dynamic programming, or data structures easier. It gives those ideas a smaller, contest-oriented language surface: target-typed input, concise loops, useful collection operations, and source-located diagnostics during development.

CP++ is a real compiler pipeline—not a macro pack. It tokenizes, parses, checks, and translates the program into C++17 that an ordinary C++ compiler can build.

## Show me the language

Target-typed input and collection operations keep the data flow visible:

```cpp
int n = input();
List<int> values = input(n);
values.sort();
print(values, delim = " ");
```

The approximate handwritten C++17 carries the same algorithm with more input and output ceremony:

```cpp
long long n;
cin >> n;
vector<long long> values(n);
for (long long &value : values) cin >> value;
sort(values.begin(), values.end());
for (long long i = 0; i < n; i++) {
    if (i) cout << ' ';
    cout << values[i];
}
cout << '\n';
```

CP++ also makes common sequence work direct: `values[-1]` reads the final element, `values[1:-1]` slices, `x in values` tests membership, and `values.find(pattern)` returns every match position. The [examples page](/examples/) shows I/O, loops, strings, Sets, Maps, and the repository’s original graph program.

## The contest loop

Write and test with checked run mode. When the solution is ready, produce submission C++ for Codeforces or another ordinary online judge.

```text
solution.cppp
    → CP++ compiler
    → generated C++17
    → ordinary C++ compiler
    → executable or online-judge submission
```

Run mode keeps readable CP++ runtime diagnostics. Submit mode removes development-only machinery, prunes unused generated support, and can compact the result. Both modes begin with the same parsed and analyzed program.

## Start where you are

- **New to CP++:** follow [Getting Started](/getting-started/) and then [Learn CP++](/learn/basics/).
- **Already coding:** keep the [Quick Reference](/reference/quick/) open.
- **Checking exact behavior:** use the complete [Language Reference](/reference/).
- **Curious about the compiler:** enter the [Advanced Reference](/advanced/).
