# CP++

CP++ is a competitive-programming-optimized language that transpiles to C++.

The point is not to replace C++ everywhere. The point is to keep the parts that are useful in contests and algorithm work, while cutting down on the parts that are noisy, verbose, or hostile when something goes wrong.

CP++ is aimed at maximal algorithmic expressiveness:

- C++ speed and ecosystem on the back end
- shorter, more direct source on the front end
- friendlier CP++-level diagnostics instead of raw generated-C++ confusion
- built-in support for common data structures and algorithmic primitives
- no attempt to stuff full algorithms into the language itself

That means things like lists, slicing, membership checks, target-typed input, loop forms, and small utility operations are part of the language, while the actual algorithm is still yours to write.

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
build/cppp --cppp in.cppp --compile
build/cppp --cppp in.cppp --run
build/cppp --cppp in.cppp --submit
```

Windows equivalents:

```sh
.\build\cppp.exe --cppp in.cppp
.\build\cppp.exe --cppp in.cppp --compile
.\build\cppp.exe --cppp in.cppp --run
.\build\cppp.exe --cppp in.cppp --submit
```

`--run` keeps extra runtime checks and CP++-style runtime diagnostics. `--submit` trims helpers more aggressively and emits code meant to look closer to ordinary contest C++.

## A Small Example

```cpp
int n = input();
List<int> values = input(n);
values.sort();

if (n) {
    print(values[0], values[-1]);
} else {
    print("empty");
}
```

## Language Docs

The implemented language surface lives in [cppp_language.md](cppp_language.md).

That file is the readable language reference. Its content is based on the executable documentation in `errors.txt` and `correct.txt`, which are still the best source for exact implemented behavior.
