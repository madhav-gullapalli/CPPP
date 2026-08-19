# Getting Started

You need Git, `make`, and a C++17-capable compiler such as `g++` or Clang.

## Get and build CP++

```sh
git clone https://github.com/madhav-gullapalli/CPPP.git
cd CPPP
make
```

On Windows with MinGW, use `mingw32-make` in place of `make`.

## Write a program

Create `solution.cppp`:

```cpp
int n = input();
List<int> values = input(n);
print(sum(values));
```

The destination type tells `input()` what to read. This program reads an integer, then that many integers, and prints their sum.

## Run while developing

```sh
make run INPUT=solution.cppp
```

This transpiles, compiles, and runs the program with CP++ runtime diagnostics enabled.

## Choose an output workflow

| Command | Result |
| --- | --- |
| `make transpile INPUT=solution.cppp` | Write adjacent `solution.cpp`. |
| `make compile INPUT=solution.cppp` | Transpile and build a native executable. |
| `make run INPUT=solution.cppp` | Build and execute with checked runtime behavior. |
| `make submit INPUT=solution.cppp` | Write compact, submission-ready C++. |
| `make submit INPUT=solution.cppp READABLE=1` | Write the same pruned submit program without minification. |
| `make subrun INPUT=solution.cppp` | Generate and execute submit-mode output. |

The executable itself accepts the corresponding forms:

```sh
build/cppp --cppp solution.cppp
build/cppp --cppp solution.cppp --compile
build/cppp --cppp solution.cppp --run
build/cppp --cppp solution.cppp --submit
build/cppp --cppp solution.cppp --submit --readable
```

## Next

Learn enough to solve a first problem in [Language Basics](/learn/basics/), [Competitive-programming I/O](/learn/io/), and [Control Flow](/learn/control-flow/).
