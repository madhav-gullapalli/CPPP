# CP++ Compiler

This project is a small compiler/transpiler for `.cppp` files. It reads a CP++ source file, emits a generated C++ file beside it, and can optionally compile and run that generated C++ program.

CP++ is currently an early competitive-programming-focused language prototype. The goal is to keep source code compact and beginner-readable while lowering to simple C++.

## Project Layout

* `src/cppp.cpp` is the command-line driver. It validates arguments, reads the `.cppp` file, writes the generated `.cpp`, and optionally invokes `g++`.
* `src/tokenizer.cpp` / `src/tokenizer.h` scan source text into tokens with line and column spans.
* `src/typesCppp.cpp` / `src/typesCppp.h` translate CP++ primitive declarations such as `bool`, `char`, `int`, and `float`.
* `src/expressions.cpp` / `src/expressions.h` parse and type-check expressions.
* `src/assignmentCppp.cpp` / `src/assignmentCppp.h` translate assignment statements, including compound assignment and multiple assignment.
* `src/controlFlow.cpp` / `src/controlFlow.h` parse control-flow headers such as `if`, `else if`, `else`, `while`, `for`, and `rep`.
* `src/printCppp.cpp` / `src/printCppp.h` translate `print(...)` and `describe(...)` statements.
* `src/errors.cpp` / `src/errors.h` collect and print source-level diagnostics, including mapping some generated C++ compiler/runtime errors back to `.cppp` lines.
* `in.cppp` is the sample input file.

## Requirements

* `g++` with C++17 support
* GNU Make, such as `make` on Linux/macOS or `mingw32-make` on some Windows MinGW/MSYS2 setups

On MSYS2/MinGW for Windows, the Make command is often named `mingw32-make`. If `make` is not found, use `mingw32-make` in the commands below.

## Build the Compiler

```sh
make
```

Or, on some Windows MinGW toolchains:

```sh
mingw32-make
```

This builds the compiler executable:

```text
build/cppp.exe    on Windows
build/cppp        on Linux/macOS
```

You can also compile it manually:

```sh
mkdir -p build
g++ -std=c++17 -Wall -Wextra -pedantic src/cppp.cpp src/assignmentCppp.cpp src/controlFlow.cpp src/errors.cpp src/expressions.cpp src/printCppp.cpp src/tokenizer.cpp src/typesCppp.cpp -o build/cppp
```

On Windows, use `build/cppp.exe` as the output path instead.

## Compile `.cppp` Files

To only translate a CP++ file to C++:

```sh
make transpile INPUT=in.cppp
```

This creates:

```text
in.cpp
```

To translate and compile the generated C++:

```sh
make compile INPUT=in.cppp
```

This creates:

```text
in.cpp
build/in.exe    on Windows
build/in        on Linux/macOS
```

To compile and immediately run it:

```sh
make run INPUT=in.cppp
```

Equivalent direct compiler commands:

```sh
build/cppp --cppp in.cppp --compile
build/in
```

On Windows:

```sh
.\build\cppp.exe --cppp in.cppp --compile
.\build\in.exe
```

## CP++ Syntax Currently Supported

Every normal source statement must end with a semicolon. Multiple semicolon-separated statements may appear on the same line.

```cpp
int a = 1; int b = 2; print(a + b);
```

Block headers such as `if`, `else`, `while`, `for`, and `rep` use braces.

```cpp
if (a < b) {
    print("a is smaller");
} else {
    print("b is smaller");
}
```

## Primitive Types

Supported primitive declarations:

```cpp
bool flag = true;
int count = 42;
char letter = 'A';
float ratio = 3.14;
```

CP++ currently lowers:

```text
bool  -> bool
char  -> CPPPChar
int   -> long long
float -> long double
```

`bigint`, `Bigint`, `bigfloat`, and `BigFloat` are currently removed. Use `int` or `float` for now.

## Printing

Supported printing:

```cpp
print("count:", count);
print("done", flush);
print();
```

`print()` with no arguments prints a newline.

`flush` is only valid as the final `print` argument. Print arguments can include expressions.

## Input

Basic input is available with target-typed `input()` in declarations and assignments:

```cpp
int x = input();
char c = input();
float f = input();
bool ok = input();

x = input();
print(x, c, f, ok);
```

`input()` reads one value from standard input using the destination variable's type.

For now, `input()` must be used as the entire value in a declaration or assignment.

Good:

```cpp
int x = input();
x = input();
```

Currently invalid:

```cpp
print(input());
int x = input() + 1;
```

## Numeric Conversions

Numeric conversions are intentionally checked by the frontend.

Current implicit conversions:

```text
bool  -> char
bool  -> int
bool  -> float

char  -> bool
char  -> int
char  -> float

int   -> bool
int   -> float

float -> bool
```

Examples:

```cpp
int x = 5;
float y = x;

char c = '0';
int s = c;

char bad = x;        // invalid without explicit cast
char ok = (char)x;   // valid
```

## Expressions

Supported expression forms include:

* literals
* identifiers
* parenthesized expressions
* explicit casts such as `(char)x`
* arithmetic operators: `+`, `-`, `*`, `/`, `%`
* comparison operators: `<`, `<=`, `>`, `>=`, `==`, `!=`
* logical operators: `!`, `&&`, `||`
* bitwise operators: `&`, `|`, `^`, `<<`, `>>`
* prefix/postfix increment and decrement on supported variables

Example:

```cpp
int total = 1 + 2 * 3;
total += 4;
total -= 1;
total *= 2;
total /= 3;
total %= 5;

if (total > 0 && total != 4) {
    print("total:", total);
}
```

## Assignments

Simple assignment is supported:

```cpp
int x = 3;
x = 5;
```

Compound assignments are supported:

```cpp
x += 1;
x -= 1;
x *= 2;
x /= 3;
x %= 5;
x <<= 1;
x >>= 1;
x &= 7;
x |= 8;
x ^= 3;
```

Logical compound assignments are also accepted:

```cpp
bool ok = true;
ok &&= false;
ok ||= true;
```

Multiple declaration assignment is supported:

```cpp
int a, b = 0, 1;
```

Multiple assignment is supported:

```cpp
int x = 1;
int y = 2;
x, y = y, x;
```

The number of values must match the number of targets.

## Control Flow

Supported control flow includes `if`, `else if`, `else`, `while`, C-style `for`, and `rep`.

```cpp
int n = input();

if (n % 2 == 0) {
    print("even");
} else {
    print("odd");
}
```

```cpp
int i = 0;
while (i < 5) {
    print(i);
    i++;
}
```

```cpp
for (int i = 0; i < 5; i++) {
    print(i);
}
```

```cpp
rep(5) {
    print("hello");
}
```

`rep(x)` repeats the block `x` times.

## Diagnostics

CP++ reports source-level diagnostics for many frontend errors, such as:

* missing semicolons
* undeclared variables
* duplicate variable declarations
* invalid type conversions
* malformed literals
* invalid assignment forms
* invalid control-flow headers
* unmatched or unclosed braces

Example:

```cpp
int x = true;
char c = x;
```

This should produce a CP++ error instead of exposing a raw generated C++ compiler error.

The compiler also attempts to map some generated C++ compile/runtime failures back to the original `.cppp` source location.

Runtime diagnostics are still early. Current runtime error handling should be treated as a development feature, not a complete safety system.

## Cleaning Build Outputs

```sh
make clean
```

This removes the `build` directory.
